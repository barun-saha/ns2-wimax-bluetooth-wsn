/*
 * Copyright (c) 2004,2005, University of Cincinnati, Ohio.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the OBR Center for 
 *      Distributed and Mobile Computing lab at the University of Cincinnati.
 * 4. Neither the name of the University nor of the lab may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "bt-linksched.h"
#include "baseband.h"
#include "lmp.h"
#include "lmp-link.h"
#include "lmp-piconet.h"
#include "random.h"


//////////////////////////////////////////////////////////
//                BTLinkScheduler                       //
//////////////////////////////////////////////////////////
BTLinkScheduler::BTLinkScheduler(Baseband * bb)
:  bb_(bb), inited_(0)
{
    // bb_->lmp_->l2capChPolicy_ = LMP::l2RR;
}

void BTLinkScheduler::init()
{
    if (bb_->lmp_->curPico && bb_->lmp_->curPico->activeLink) {
	lastSchedSlot_ = bb_->lmp_->curPico->activeLink->txBuffer->slot();
	lastSchedSlot_highclass_ = lastSchedSlot_;
	inited_ = 1;

	LMPLink *wk = bb_->lmp_->curPico->activeLink;
	for (int i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	    wk->txBuffer->deficit_ = 0;
	}
    }
}

int BTLinkScheduler::pass2(LMPLink * next)
{
    LMPLink *wk;
    int i;

    if (bb_->pollReserveClass()) {
	// Conduct a RR for Reserved class, to redistribute bandwidth.
	if (!bb_->txBuffer_[lastSchedSlot_highclass_]) {
	    lastSchedSlot_highclass_ =
		bb_->lmp_->curPico->activeLink->txBuffer->slot();
	}
	if (bb_->txBuffer_[lastSchedSlot_highclass_]->link()->suspended) {
	    wk = bb_->lmp_->curPico->activeLink;
	} else {
	    wk = bb_->txBuffer_[lastSchedSlot_highclass_]->link()->next;
	}
	for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	    if (wk->txBuffer->prioClass_ == TxBuffer::High &&
		wk->txBuffer->hasDataPkt()) {
		lastSchedSlot_highclass_ = wk->txBuffer->slot();
		wk->txBuffer->lastPollClk_highclass_ = bb_->clk_;
		return lastSchedSlot_highclass_;
	    }
	    wk = wk->next;
	}
    }

    if (bb_->useDynamicTpoll()) {
	// additional pass, used a dynamic T_poll to poll idle slaves.
	// The reason: the master should not poll if nobody has anything
	// to send, as it causes interferences to other piconets.
	wk = next;
	for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	    if (((bb_->clk_ - wk->txBuffer->lastPollClk_) >> 1) >=
		wk->txBuffer->T_poll_) {
		lastSchedSlot_ = wk->txBuffer->slot();
		wk->txBuffer->lastPollClk_ = bb_->clk_;
		return lastSchedSlot_;
	    }
	    wk = wk->next;
	}
	return Baseband::RecvSlot;	// Nothing to send

    } else {			// polling in every available slot.
	lastSchedSlot_ = next->txBuffer->slot();
	next->txBuffer->lastPollClk_ = bb_->clk_;
	return lastSchedSlot_;
    }
}

//////////////////////////////////////////////////////////
//                      BTRR                           //
//////////////////////////////////////////////////////////
// Round Robin
// We basically ignore computed sched_word for non-QoS ACL guys.

int BTRR::sched(int clk, BTSchedWord * sched, int curSlot)
{
    int i;
    LMPLink *wk, *next;

    if (!inited_) {
	return Baseband::RecvSlot;
    }

    if (curSlot == Baseband::ReserveSlot) {	// not handled at this moment
	return Baseband::RecvSlot;
    } else if (curSlot != Baseband::DynamicSlot
	       && bb_->txBuffer_[curSlot]->prioClass_ >= TxBuffer::High) {
	return curSlot;
    }

    if (!bb_->txBuffer_[lastSchedSlot_]) {
	init();
    }
    if (bb_->txBuffer_[lastSchedSlot_]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->txBuffer_[lastSchedSlot_]->link()->next;
    }
    next = wk;

    // first pass, try to get a pkt from an non-QoS guy
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	// FIXME: This should change to credit based.
	if (wk->txBuffer->prioClass_ >= TxBuffer::High) {
	    wk = wk->next;
	    continue;
	}

	if (wk->txBuffer->hasDataPkt()) {
	    lastSchedSlot_ = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk_ = bb_->clk_;
	    return lastSchedSlot_;

	    // to avoid starvation.  Force to poll every T_poll_max_.
	} else if (((bb_->clk_ - wk->txBuffer->lastPollClk_) >> 1) >=
		   wk->txBuffer->T_poll_max_) {
	    // wk->txBuffer->T_poll) {
	    lastSchedSlot_ = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk_ = bb_->clk_;
	    return lastSchedSlot_;
	}
	wk = wk->next;
    }

    return pass2(next);
}


//////////////////////////////////////////////////////////
//                      BTDRR                           //
//////////////////////////////////////////////////////////
// Deficit Round Robin
// We basically ignore computed sched_word for non-QoS ACL guys.
int BTDRR::sched(int clk, BTSchedWord * sched, int curSlot)
{
    int i;
    LMPLink *wk, *next;

    if (!inited_) {
	return Baseband::RecvSlot;
    }

    if (curSlot == Baseband::ReserveSlot) {	// not handled at this moment
	return Baseband::RecvSlot;
    } else if (curSlot != Baseband::DynamicSlot
	       && bb_->txBuffer_[curSlot]->prioClass_ >= TxBuffer::High) {
	return curSlot;
    }

    if (!bb_->txBuffer_[lastSchedSlot_]) {
	lastSchedSlot_ = bb_->lmp_->curPico->activeLink->txBuffer->slot();
	lastSchedSlot_highclass_ = lastSchedSlot_;
    }
    if (bb_->txBuffer_[lastSchedSlot_]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->txBuffer_[lastSchedSlot_]->link()->next;
    }
    next = wk;

    // first pass, try to get a pkt from an non-QoS guy
    for (int x = 0; x < 5; x++) {	// maximun slot usage is 10 slots
	for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	    // FIXME: This should change to credit based.
	    if (wk->txBuffer->prioClass_ >= TxBuffer::High) {
		wk = wk->next;
		continue;
	    }

	    if (wk->txBuffer->deficit_ >= 0 && wk->txBuffer->hasDataPkt()) {
		lastSchedSlot_ = wk->txBuffer->slot();
		wk->txBuffer->lastPollClk_ = bb_->clk_;
		return lastSchedSlot_;

		// to avoid starvation.  Force to poll every T_poll_max_.
	    } else if (((bb_->clk_ - wk->txBuffer->lastPollClk_) >> 1) >=
		       wk->txBuffer->T_poll_max_) {
		// wk->txBuffer->T_poll) {
		lastSchedSlot_ = wk->txBuffer->slot();
		wk->txBuffer->lastPollClk_ = bb_->clk_;
		return lastSchedSlot_;
	    }
	    wk = wk->next;
	}
	int negative = 0;
	for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	    if (wk->txBuffer->deficit_ < 0) {
		negative = 1;
	    }
	    wk->txBuffer->deficit_ += 2;
	    if (wk->txBuffer->deficit_ > 0) {
		wk->txBuffer->deficit_ = 0;
	    }
	    wk = wk->next;
	}
	if (!negative) {
	    break;
	}
    }

    return pass2(next);
}


//////////////////////////////////////////////////////////
//                      BTERR                           //
//////////////////////////////////////////////////////////
// Exhaustive Round Robin
int BTERR::sched(int clk, BTSchedWord * sched, int curSlot)
{
    int i;
    LMPLink *wk;

    // QoS req
    if (bb_->txBuffer_[curSlot] &&
	bb_->txBuffer_[curSlot]->prioClass_ >= TxBuffer::High) {
	return curSlot;
    }

    if (!bb_->txBuffer_[lastSchedSlot_]) {
	init();
    }
    if (bb_->txBuffer_[lastSchedSlot_]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->txBuffer_[lastSchedSlot_]->link();
    }
    // Exhaustive ??
    if (wk->txBuffer->hasDataPkt() || wk->txBuffer->nullCntr() == 0) {
	wk->txBuffer->lastPollClk_ = bb_->clk_;
	lastSchedSlot_ = wk->txBuffer->slot();
	return lastSchedSlot_;
    }

    wk = wk->next;
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (wk->txBuffer->hasDataPkt()) {
	    lastSchedSlot_ = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk_ = bb_->clk_;
	    return lastSchedSlot_;
	} else if (((bb_->clk_ - wk->txBuffer->lastPollClk_) >> 1) >=
		   wk->txBuffer->T_poll_max_) {
	    // wk->txBuffer->T_poll) {
	    lastSchedSlot_ = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk_ = bb_->clk_;
	    return lastSchedSlot_;
	}
	wk = wk->next;
    }

    return Baseband::RecvSlot;	// Nothing to send
}

//////////////////////////////////////////////////////////
//                      BTPRR                           //
//////////////////////////////////////////////////////////
// Priority Round Robin
int BTPRR::sched(int clk, BTSchedWord * sched, int curSlot)
{
    if (!inited_) {
	return Baseband::RecvSlot;
    }

    int i;
    int curPrioLevel = PRR_MIN_PRIO_MINUS_ONE, retSlot;
    LMPLink *wk, *next;

    // QoS req
    if (bb_->txBuffer_[curSlot]
	&& bb_->txBuffer_[curSlot]->prioClass_ >= TxBuffer::High) {
	return curSlot;
    }

    if (!bb_->txBuffer_[lastSchedSlot_]) {
	init();
    }
    if (bb_->txBuffer_[lastSchedSlot_]->link()->suspended) {
	wk = bb_->lmp_->curPico->activeLink;
    } else {
	wk = bb_->txBuffer_[lastSchedSlot_]->link()->next;
    }
    next = wk;

    // Find the guy who has data for has not been polled for a long period.
    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {

	// Force poll every T_poll_max_, to avoid starvation.
	if (((bb_->clk_ - wk->txBuffer->lastPollClk_) >> 1) >=
	    wk->txBuffer->T_poll_max_) {
	    lastSchedSlot_ = wk->txBuffer->slot();
	    wk->txBuffer->lastPollClk_ = bb_->clk_;
	    return lastSchedSlot_;
	}
	// Check for priority.
	if (wk->txBuffer->hasDataPkt()
	    && wk->txBuffer->prio_ > curPrioLevel) {
	    retSlot = wk->txBuffer->slot();
	    curPrioLevel = wk->txBuffer->prio_;
	}
	wk = wk->next;
    }

    if (curPrioLevel > PRR_MIN_PRIO_MINUS_ONE) {
	lastSchedSlot_ = retSlot;
	bb_->txBuffer_[lastSchedSlot_]->lastPollClk_ = bb_->clk_;
	return lastSchedSlot_;
    }

    return pass2(next);
}

//////////////////////////////////////////////////////////
//                      BTFCFS                           //
//////////////////////////////////////////////////////////
// First Come First Serve
BTFCFS::BTFCFS(Baseband * bb)
:  BTLinkScheduler(bb)
{
    // bb_->lmp_->l2capChPolicy_ = LMP::l2FCFS;
}

int BTFCFS::sched(int clk, BTSchedWord * sched, int curSlot)
{
    if (!inited_) {
	return Baseband::RecvSlot;
    }
    // QoS req
    if (bb_->txBuffer_[curSlot]
	&& bb_->txBuffer_[curSlot]->prioClass_ >= TxBuffer::High) {
	return curSlot;
    }

    LMPLink *wk = bb_->lmp_->curPico->activeLink;
    int i;
    double ts = 1E10;
    int fcSlot = -1;

    for (i = 0; i < bb_->lmp_->curPico->numActiveLink; i++) {
	if (!wk->txBuffer->suspended() && wk->txBuffer->curPkt()
	    && HDR_BT(wk->txBuffer->curPkt())->ts_ < ts) {
	    fcSlot = wk->txBuffer->slot();
	    ts = HDR_BT(wk->txBuffer->curPkt())->ts_;
	}
	wk = wk->next;
    }

    return (fcSlot > 0 ? lastSchedSlot_ = fcSlot : Baseband::RecvSlot);
}


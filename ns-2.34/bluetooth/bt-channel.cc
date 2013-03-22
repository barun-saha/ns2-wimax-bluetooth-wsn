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

#include "baseband.h"
#include "lmp.h"
#include "bt-channel.h"
#include "bt-lossmod.h"

#define USE_PICO_CHANNEL

static class BTChannelClass:public TclClass {
  public:
    BTChannelClass():TclClass("BTChannel") { }
    TclObject *create(int, const char *const *argv) {
	return new BTChannel();
    }
} class_btchannel;


BTChannel *BTChannel::chain_ = 0;
BTLossMod *BTChannel::lossMod_ = 0;
BTInterferQue *BTChannel::interfQ_ = 0;

BTChannel::BTChannel()
:  next_(this), txHandler_(this), txFinHandler_(this), unlockTxHandler_(this),
rxHandler_(this), addr_(-1), bb_(0), node_(0), pico_(0), numPico_(0)
{
    if (chain_ == NULL) {
	chain_ = this;
	chain_->next_ = chain_;
	BtStat::init_logfiles();
	lossMod_ = new BTLossMod();
	interfQ_ = new BTInterferQue[79];
    } else {
	next_ = chain_->next_;
	chain_->next_ = this;
	chain_ = this;
    }
}

BTChannel::~BTChannel()
{
    for (int i = 0; i < numPico_; i++) {
	pico_[i]->decRef();
    }
    delete pico_;
}

int BTChannel::lookupPicoChannel(int ma)
{
    for (int i = 0; i < numPico_; i++) {
	if (pico_[i]->master() == ma) {
	    return i;
	}
    }
    return -1;
}

void BTChannel::setPicoChannel(BTPicoChannel * pico)
{
    int ind = lookupPicoChannel(pico->master());
    if (ind >= 0) {
	if (pico_[ind] == pico) {
	    return;
	}
	pico_[ind]->decRef();
    } else {
	BTPicoChannel **np = new BTPicoChannel *[numPico_ + 1];
	for (int i = 0; i < numPico_; i++) {
	    np[i] = pico_[i];
	}
	delete[]pico_;
	pico_ = np;
	ind = numPico_;
	numPico_++;
    }

    pico_[ind] = pico;
    pico->incRef();

    // pico->dump(addr());
}

void BTChannel::clearPicoChannel(int ma)
{
    int ind = lookupPicoChannel(ma);
    if (ind >= 0) {
	pico_[ind]->decRef();
	for (int i = ind; i < numPico_; i++) {
	    pico_[ind] = pico_[ind + 1];
	}
	// the memory block is not shrinked.  it is ok, a little waiste though.
	// I don't want an extra integer to keep track the allocation size.
	if (--numPico_ == 0) {
	    delete pico_;
	    pico_ = 0;
	}
    }
}

void BTChannel::recvFirstBit(Packet * p)
{
    hdr_bt *bh = HDR_BT(p);
    int st;

    if (HDR_CMN(p)->direction() != hdr_cmn::UP) {
	fprintf(stderr, "wrong dir\n");
	bh->dump(stderr, '!', addr_, bb_->state_str_s());
	abort();
    }
    if ((st = bb_->recv_filter(p)) == 1) {
	Scheduler::instance().schedule(&rxHandler_, p,
				       bh->txtime() - BTDELAY);
	bb_->setRxRecv();

    } else {
	// if ((bh->receiver == addr_ || bh->target() == addr_)
	if ((bh->receiver == addr_)
	    && *bh->comment_ != 'P') {
	    const char *reason;
	    if (st == 6) {
		reason = "fs-mistch";
	    } else if (st == 4) {
		reason = "RX not on";
	    } else if (st == 5) {
		reason = "Locked";
	    } else if (st == 2) {
		reason = "wrong pkt type";
	    } else {
		reason = "unknown";
	    }

	/** Commented by Barun [07 March 2013]
	    fprintf(stdout,
		    "====> intended receiver filter fails: %s:%d\n",
		    reason, st);
	*/
	    bh->dump(stdout, '=', addr_, bb_->state_str_s());
	    // bh->dump(stdout, '=', addr_, "XX");
	}

	Packet::free(p);
    }
}

void BTChannel::sendUp(Packet * p, Handler * h)
{
    // uptarget_->recv(p, h);
    bb_->sendUp(p, h);		// these 2 are equivalent
}

void BTChannel::transmitComplete(Packet * p)
{
    bb_->turn_off_trx();

    hdr_bt *bh = HDR_BT(p);
    int numSlot = hdr_bt::slot_num(bh->type);

    if (numSlot > 1) {
	double t = numSlot * bb_->slotTime() - bh->txtime();

	if (t >= bb_->slotTime()) {
	    Scheduler::instance().schedule(&unlockTxHandler_,
					   &unlockTxEv_, t - 150E-6);
	    bb_->lockTx();
	/** Commented by Barun [07 March 2013]
	    fprintf(stdout, "%d %f unlock %f later\n",
		    bb_->bd_addr_, Scheduler::instance().clock(),
		    t - 150E-6);
	*/
	}
    }

    Packet::free(p);
}

void BTChannel::unlockTx()
{
    bb_->unlockTx();
}

void BTChannel::transmit(Packet * p)
{
    HDR_CMN(p)->direction() = hdr_cmn::UP;
    hdr_bt *bh = HDR_BT(p);
    // HDR_BT(p)->nokeep = 0;

    // schedule TX finish, cleaning TX state in BB.
    Scheduler::instance().schedule(&txFinHandler_, p,
				   bh->txtime() - BTDELAY);

#ifdef USE_PICO_CHANNEL
    bool chMatched = false;
    if (numPico_ > 0 && numPico_ == lmp_->numPico() &&
	bb_->state() == Baseband::CONNECTION &&
	HDR_BT(p)->type != hdr_bt::HLO) {
	for (int j = 0; j < numPico_; j++) {
	    // pico_[j]->dump(addr(), stdout);
	    if (pico_[j]->master() == HDR_BT(p)->ac) {
		chMatched = true;
		for (int i = 0; i < pico_[j]->numCh(); i++) {
		    if (pico_[j]->getCh(i) == this) {
			continue;
		    } else if (pico_[j]->getCh(i)->bb_->
			       getCurAccessCode() == HDR_BT(p)->ac) {
			pico_[j]->getCh(i)->recvFirstBit(p->copy());
		    }
		}
		break;
	    }
	}

	// Packet::free(p);
	if (chMatched) {
	    return;
	}
    }
#endif

    // access code should be able to filter out unwanted receiver.
    // So the overhead is maily walking through the whole list of
    // devices.  The above optimization makes sense only for 
    // very large networks.

    // UCBT makes the assumption that a device don't mismatch the 
    // access code.

    // every one gets it at the time that first bit arrives
    // Virtually, we consider BTDELAY is very small (close to 0).  Here
    // BTDELAY is set to half of the max slot misalignment for
    // timing syn at the receiver side.  By this way, the clk does not
    // need to be adjusted to generate correct FH.  See bt.h.
    // s.schedule(&wk->recvHandler, p->copy(), BTDELAY);


    BTChannel *wk = next_;
    while (wk != this) {	// doesn't loop back to myself

	// if (wk->_trx_st == RX_ON) -- consider slot boundary here too ???
	if (wk->bb_->getCurAccessCode() == HDR_BT(p)->ac) {
	    // Has to send copy first!!!.  It may be changed somewhere.
	    wk->recvFirstBit(p->copy());
	}
	wk = wk->next_;
    }
    // Packet::free(p);
}


void BTPicoChannel::dump(bd_addr_t ad, FILE * out)
{
    if (!out) {
	out = stderr;
    }

    fprintf(out, "[pico Ch] ad:%d ma:%d (refcnt:%d) has %d nodes (",
	    ad, master_, ref_, numCh_);
    for (int i = 0; i < numCh_; i++) {
	fprintf(out, "%d ", ch_[i]->addr());
    }
    fprintf(out, ")\n");
}

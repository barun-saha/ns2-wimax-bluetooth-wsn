/*
 * Copyright (c) 2004, University of Cincinnati, Ohio.
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

/*
 *	lmp-piconet.cc
 */

#include "baseband.h"
#include "lmp.h"
#include "l2cap.h"
#include "gridkeeper.h"
#include "lmp-piconet.h"
#include "bt-node.h"

#define LMPDEBUG 1

#ifdef NDEBUG
#undef NDEBUG
#endif

//////////////////////////////////////////////////////////
//                      Piconet                         //
//////////////////////////////////////////////////////////
BTSchedWord Piconet::_mword(6, 1, 0);
BTSchedWord Piconet::_sword(6, 0, 0);

Piconet::Piconet(Bd_info * mster, Bd_info * slave, LMPLink * l)
:  lmp_(l->lmp_), _sco_ev(LMPEvent::ScoWakeup),
_sco_suspend_ev(LMPEvent::SniffSuspend)
{
    _init();
    master_bd_addr_ = mster->bd_addr_;
    master_clk_ = mster->clkn_;
    numActiveLink = 1;
    activeLink = l;
    // numSlave = 1;
    // bd_info = slave;
    // master = mster;
    // bd_info->next_ = NULL;

    if (isMaster()) {
	setPicoChannel();
    }
}

// an empty master piconet
Piconet::Piconet(LMP * lmp)
:  lmp_(lmp), _sco_ev(LMPEvent::ScoWakeup),
_sco_suspend_ev(LMPEvent::SniffSuspend)
{
    _init();
    // master = lmp->_my_info;
    master_bd_addr_ = lmp->_my_info->bd_addr_;
    master_clk_ = lmp->_my_info->clkn_;
}

void Piconet::_init()
{
    prev = next = this;
    master_bd_addr_ = -1;
    master_clk_ = -1;
    // rfChannel_ = 0;
    numActiveLink = 0;
    activeLink = 0;
    numSuspendLink = 0;
    suspendLink = 0;
    numScoLink = 0;
    scoLink = 0;
    suspended = 0;
    suspendReqed = 0;
    // numSlave = 0;
    // bd_info = 0;
    // master = 0;
    old_slave = 0;
    numOldSlave = 0;
    clk_offset = 0;
    slot_offset = 0;
    _sched_word = 0;
    lt_table_len = 2;
    _scohand = 0;
    _res = 1;

    _DscoMap = 0;
    _DscoMap_ts = -100000;

    picoBCastLink = 0;
    activeBCastLink = 0;
    num_sniff1 = 0;
    num_sniff2 = 0;
    RP = -1;
    T_sniff = 0;
    prevFixedRP = -1;
    prp = -1;
    rpFixed = 0;

    for (int i = 0; i < 6; i++) {
	sco_table[i] = 0;
    }

    // def: LMPLink *lt_table[MAX_SLAVE_PER_PICONET + 1]
    memset(lt_table, 0, sizeof(void *) * (MAX_SLAVE_PER_PICONET + 1));
#ifdef PARK_STATE
    memset(pmaddr, 0xff, sizeof(bd_addr_t) * PMADDRLEN);
    memset(araddr, 0xff, sizeof(bd_addr_t) * ARADDRLEN);
#endif
}

Piconet::~Piconet()
{
    assert(numActiveLink == 0);
    assert(numSuspendLink == 0);

    lmp_->bb_->ch_->clearPicoChannel(lmp_->bb_->bd_addr_);
}

void Piconet::dump(FILE * out)
{
    if (!out) {
	out = stdout;
    }

    fprintf(out, "[%d(%c%d):", master_bd_addr_, 
	    (this == lmp_->curPico ? '*' : ' '),
	    numActiveLink + numSuspendLink);
    int i;
    LMPLink *link = activeLink;
    for (i = 0; i < numActiveLink; i++) {
	fprintf(out, "%d ", link->remote->bd_addr_);
	link = link->next;
    }
    link = suspendLink;
    for (i = 0; i < numSuspendLink; i++) {
	fprintf(out, "%d ", link->remote->bd_addr_);
	link = link->next;
    }
    fprintf(out, "]\n");
}

#ifdef PARK_STATE
uchar Piconet::allocPMaddr(bd_addr_t bd)
{
    for (int i = 1; i < PMADDRLEN; i++) {
	if (pmaddr[i] == BD_ADDR_BCAST) {
	    pmaddr[i] = bd;
	    return i;
	}
    }
    return 0;
}

uchar Piconet::allocARaddr(bd_addr_t bd)
{
    for (int i = 1; i < ARADDRLEN; i++) {
	if (araddr[i] == BD_ADDR_BCAST) {
	    araddr[i] = bd;
	    return i;
	}
    }
    return 0;
}
#endif

int Piconet::allocLTaddr()
{
    int lt = 1;
    for (; lt < lt_table_len; lt++) {
	if (lt_table[lt] == NULL) {
	    break;
	}
    }
    if (lt == lt_table_len) {
	lt_table_len++;
    }
    return lt;
}

// Master only
int Piconet::ltAddrIsValid(uchar lt)
{
    LMPLink *wk = activeLink;
    for (int i = 0; i < numActiveLink; i++) {
	if (wk->lt_addr_ == lt) {
	    return 1;
	}
	wk = wk->next;
    }
    return 0;
}

void Piconet::add_sco_table(int D_sco, int T_poll, LMPLink * slink)
{
    if (T_poll == 6) {
	sco_table[D_sco / 2] = sco_table[D_sco / 2 + 3] = slink;
    } else if (T_poll == 4) {
	if (D_sco == 2) {
	    sco_table[1] = sco_table[3] = sco_table[5] = slink;
	} else {
	    sco_table[0] = sco_table[2] = sco_table[4] = slink;
	}
    } else {
	sco_table[0] = sco_table[1] = sco_table[2] = sco_table[3]
	    = sco_table[4] = sco_table[5] = slink;
    }
}

TxBuffer *Piconet::lookupTxBuffer(hdr_bt * bh)
{
    LMPLink *link;
    LMPLink *scolink;

    if ((scolink = sco_table[(lmp_->bb_->clk_ % 24) / 4])) {
	return scolink->txBuffer;
    }

    if (bh->lt_addr_ >= lt_table_len
	|| (link = lt_table[bh->lt_addr_]) == NULL) {
	return NULL;
    }

    return link->txBuffer;
}

LMPLink *Piconet::addBCastLink(int active)
{
    int txslot = (active ? Baseband::BcastSlot : Baseband::BeaconSlot);
    return new LMPLink(lmp_, txslot);
}

void Piconet::picoBCast(Packet * p)
{
    if (!picoBCastLink) {
	picoBCastLink = addBCastLink(0);
    }
    picoBCastLink->enqueue(p);
}

void Piconet::activeBCast(Packet * p)
{
    if (!activeBCastLink) {
	activeBCastLink = addBCastLink(1);
    }
    activeBCastLink->enqueue(p);
}

void Piconet::fwdCommandtoAll(uchar opcode, uchar * content, int len,
			      int pl_len, LMPLink * from, int flags)
{
    int i;
    LMPLink *wk = activeLink;
    for (i = 0; i < numActiveLink; i++) {
	if (wk != from) {
	    lmp_->lmpCommand(opcode, content, len, pl_len, wk);
	    printf("%d sndALL \n", lmp_->bb_->bd_addr_);
	}
	wk = wk->next;
    }
    wk = suspendLink;
    for (i = 0; i < numSuspendLink; i++) {
	if (wk != from) {
	    lmp_->lmpCommand(opcode, content, len, pl_len, wk);
	    printf("%d sndALL \n", lmp_->bb_->bd_addr_);
	}
	wk = wk->next;
    }
}

void Piconet::setNeedAdjustSniffAttempt(LMPLink * excep)
{
    int i;
    LMPLink *wk = activeLink;
    for (i = 0; i < numActiveLink; i++) {
	if (wk->_in_sniff && wk != excep) {
	    // wk->needAdjustSniffAttempt = 1;
	    wk->adjustSniffAttempt();
	}
	wk = wk->next;
    }
    wk = suspendLink;
    for (i = 0; i < numSuspendLink; i++) {
	if (wk->_in_sniff) {
	    wk->needAdjustSniffAttempt = 1;
	}
	wk = wk->next;
    }
}

// the clk is based on link->piconet.
int Piconet::lookupRP(BrReq * brreq, LMPLink * link)
{
    int clkdiff = 0;
    int i;
    Piconet *pico = (link ? link->piconet : lmp_->curPico);
    if (pico != this) {
	clkdiff = clk_offset - pico->clk_offset;
	clkdiff /= 2;
	clkdiff %= lmp_->defaultTSniff_;
	if (clkdiff < 0) {
	    clkdiff += lmp_->defaultTSniff_;
	}
    }

    LMPLink *wk = activeLink;
    for (i = 0; i < numActiveLink; i++) {
	// if (wk->_in_sniff && wk != link) {
	if (wk->sniffreq && wk != link) {
	    brreq->
		add((wk->sniffreq->D_sniff - clkdiff +
		     lmp_->defaultTSniff_)
		    % lmp_->defaultTSniff_);
	}
	wk = wk->next;
    }
    wk = suspendLink;
    for (i = 0; i < numSuspendLink; i++) {
	// if (wk->_in_sniff && wk != link) {
	if (wk->sniffreq && wk != link) {
	    brreq->
		add((wk->sniffreq->D_sniff - clkdiff +
		     lmp_->defaultTSniff_)
		    % lmp_->defaultTSniff_);
	}
	wk = wk->next;
    }
    return 1;
}

void Piconet::checkLink()
{
    LMPLink *wk;
    LMPLink *link;
    double now = Scheduler::instance().clock();
    double t, ta;
    int i;

    // Do we need to check suspened Links??
    wk = suspendLink;
    for (i = numSuspendLink; i > 0; i--) {
	t = wk->lastPktRecvTime();
	ta = wk->lastDataPktRecvTime();
	link = wk;
	wk = wk->next;

	if (lmp_->supervisionEnabled_ && t > 0
	    && (now - t) > lmp_->supervisionTO_) {
	    detach_link(link, BTDISCONN_TIMEOUT);
	    continue;
	}
    }

    wk = activeLink;
    for (i = numActiveLink; i > 0; i--) {
	t = wk->lastPktRecvTime();
	ta = wk->lastDataPktRecvTime();
	link = wk;
	wk = wk->next;

	if (lmp_->supervisionEnabled_ && t > 0
	    && (now - t) > lmp_->supervisionTO_) {
	    detach_link(link, BTDISCONN_TIMEOUT);
	    continue;
	}
	if (lmp_->autoOnHold_ && ta > 0 && (now - ta) > lmp_->idleSchred_) {
	    uint32_t hi = (lmp_->bb_->clk_ >> 1) + link->T_poll_ * 6;
	    link->request_hold(lmp_->defaultHoldTime_, hi);
	}
    }
}

double Piconet::lookupWakeupTime()
{
    double t = -1;
    if (!suspendLink) {
	return t;
    }
    LMPLink *wk = suspendLink;
    do {
	if (t > 0) {
	    t = MIN(wk->_wakeupT, t);
	} else {
	    t = wk->_wakeupT;
	}
    } while ((wk = wk->next) != suspendLink);
    return t;
}

// move nl from activeLink to suspendLink
void Piconet::suspend_link(LMPLink * nl)
{
    assert(activeLink != NULL);
    if (activeLink == nl) {
	if (activeLink->next == activeLink) {	// singleton
	    activeLink = NULL;
	} else {
	    activeLink->prev->next = activeLink->next;
	    activeLink->next->prev = activeLink->prev;
	    activeLink = activeLink->next;
	}
    } else {
	LMPLink *wk = activeLink;
	do {
	    if (wk == nl) {
		wk->prev->next = wk->next;
		wk->next->prev = wk->prev;
		break;
	    }
	} while ((wk = wk->next) != activeLink);
    }
    numActiveLink--;

    if (suspendLink == NULL) {
	nl->next = nl->prev = nl;
	suspendLink = nl;
	numSuspendLink = 1;
    } else {
	nl->next = suspendLink;
	nl->prev = suspendLink->prev;
	suspendLink->prev->next = nl;
	suspendLink->prev = nl;
	numSuspendLink++;
    }
}

// move nl to activeLink from suspendLink
void Piconet::unsuspend_link(LMPLink * nl)
{
    assert(suspendLink != NULL);
    if (suspendLink == nl) {
	if (suspendLink->next == suspendLink) {	// singleton
	    suspendLink = NULL;
	} else {
	    suspendLink->prev->next = suspendLink->next;
	    suspendLink->next->prev = suspendLink->prev;
	    suspendLink = suspendLink->next;
	}
    } else {
	LMPLink *wk = suspendLink;
	do {
	    if (wk == nl) {
		wk->prev->next = wk->next;
		wk->next->prev = wk->prev;
		break;
	    }
	} while ((wk = wk->next) != suspendLink);
    }
    numSuspendLink--;

    if (activeLink == NULL) {
	nl->next = nl->prev = nl;
	activeLink = nl;
	numActiveLink = 1;
    } else {
	nl->next = activeLink;
	nl->prev = activeLink->prev;
	activeLink->prev->next = nl;
	activeLink->prev = nl;
	numActiveLink++;
    }
}

// master only
void Piconet::setPicoChannel()
{
    int numCh = numActiveLink + numSuspendLink + 1;
    BTChannel **ch = new BTChannel *[numCh];
    ch[0] = lmp_->bb_->ch_;
    int i;
    LMPLink *wk = activeLink;
    for (i = 1; i < numActiveLink + 1; i++) {
	ch[i] = lmp_->node_->lookupNode(wk->remote->bd_addr_)->phy_;
	wk = wk->next;
    }
    wk = suspendLink;
    for (; i < numCh; i++) {
	ch[i] = lmp_->node_->lookupNode(wk->remote->bd_addr_)->phy_;
	wk = wk->next;
    }

    BTPicoChannel *pc = new BTPicoChannel(numCh, ch, lmp_->bb_->bd_addr_);
    for (i = 0; i < numCh; i++) {
	ch[i]->setPicoChannel(pc);
    }
}

LMPLink *Piconet::add_slave(LMPLink * nl)
{
    // add_bd(nl->remote);
    // add_active_member(nl->remote);

    if (activeLink) {
	nl->next = activeLink;
	nl->prev = activeLink->prev;
	activeLink->prev->next = nl;
	activeLink->prev = nl;
    } else {
	activeLink = nl->next = nl->prev = nl;
	assert(numActiveLink == 0);
    }

    lt_table[nl->remote->lt_addr_] = nl;
    nl->lt_addr_ = nl->remote->lt_addr_;

    // numSlave++;
    numActiveLink++;
    nl->connhand->setLink(nl);
    nl->piconet = this;

#if 0
    if (rfChannel_) {
	rfChannel_->add(lmp_->node_->lookupNode(nl->remote->bd_addr_)->bb_);
    }
#endif

    if (isMaster()) {
	setPicoChannel();
    }
    return nl;
}

LMPLink *Piconet::add_slave(Bd_info * remote, LMP * lmp,
			    ConnectionHandle * connh)
{
    LMPLink *nl = new LMPLink(remote, lmp, connh);
    return add_slave(nl);
}

void Piconet::detach_link(LMPLink * link, uchar reason)
{
    bd_addr_t addr = link->remote->bd_addr_;

    lt_table[link->lt_addr_] = NULL;	// free LT_ADDR
    lmp_->bb_->freeTxBuffer(link->txBuffer);

    bool isActiveLink = false;
    if (activeLink) {
	if (activeLink == link) {
	    if (activeLink->next == activeLink) {	// singleton
		activeLink = NULL;
	    } else {
		activeLink->prev->next = activeLink->next;
		activeLink->next->prev = activeLink->prev;
		activeLink = activeLink->next;
	    }
	    numActiveLink--;
	    isActiveLink = true;
	} else {
	    LMPLink *wk = activeLink;
	    do {
		if (wk == link) {
		    wk->prev->next = wk->next;
		    wk->next->prev = wk->prev;
		    numActiveLink--;
		    isActiveLink = true;
		    break;
		}
	    } while ((wk = wk->next) != activeLink);
	}
    }

    if (!isActiveLink && suspendLink) {
	if (suspendLink == link) {
	    if (suspendLink->next == suspendLink) {	// singleton
		suspendLink = NULL;
	    } else {
		suspendLink->prev->next = suspendLink->next;
		suspendLink->next->prev = suspendLink->prev;
		suspendLink = suspendLink->next;
	    }
	    numSuspendLink--;
	} else {
	    LMPLink *wk = suspendLink;
	    do {
		if (wk == link) {
		    wk->prev->next = wk->next;
		    wk->next->prev = wk->prev;
		    numSuspendLink--;
		    break;
		}
	    } while ((wk = wk->next) != suspendLink);
	}
    }
    // link->detach();

    if (link->connhand) {
	while (link->connhand->chan) {
	    //link->connhand->remove_channel(chan);
	    link->connhand->chan->linkDetached();
	}
    }
    delete link;

    // notify other layers
    lmp_->node_->linkDetached(addr, reason);

    if (numActiveLink + numSuspendLink == 0) {
	// remove and free myself
	lmp_->remove_piconet(this);
    } else if (isActiveLink) {
	compute_sched();
	lmp_->bb_->set_sched_word(_sched_word);
	// lmp_->bb_->lastSchedSlot = activeLink->txBuffer->slot();
    }
}

void Piconet::add_slave_info(LMPLink::Slave_info * slave)
{
    Bd_info *wk = old_slave;
    while (wk) {
	if (wk->bd_addr_ == slave->addr) {
	    wk->lt_addr_ = slave->lt_addr_;
	    wk->active_ = slave->active;
	    return;
	}
	wk = wk->next_;
    }
    Bd_info *bd = new Bd_info(slave->addr, 0);
    bd->lt_addr_ = slave->lt_addr_;
    bd->active_ = slave->active;
    bd->next_ = old_slave;
    old_slave = bd;
}

/*
void Piconet::add_bd(Bd_info * bd)
{
    bd->next_ = bd_info;
    bd_info = bd;
}
*/

// In spec, D_sco = clk/2 % T_sco.  However, we consider a frame of 6 slots,
// with which the D_sco is defined, and assume D_sco is always referred to
// the first 6 slot frame within a 12 slots frame, that is, when activate
// the first instance of SCO link, the 6 slot frame is always aligned to
// a 12 slot frame boundary.
uchar Piconet::_getDsco()
{
    uchar d_sco = 0;

    // Check for freshness, a better scheme should be used.
    if (lmp_->bb_->clk_ - _DscoMap_ts > 1000) {
	_DscoMap = 0;
    }

    if (lmp_->scoPico && lmp_->scoPico != this) {
	d_sco = scoUnoccupiedSlot();
    } else {
	LMPLink *wk = scoLink;
	for (int i = 0; i < numScoLink; i++) {
	    _DscoMap |= (0x01 << wk->D_sco);
	    wk = wk->next;
	}
	d_sco =
	    (((_DscoMap & (0x01 << 2)) ? 4 : ((_DscoMap & 0x01) ? 2 : 0)));
    }
    _DscoMap |= (0x01 << d_sco);
    _DscoMap_ts = lmp_->bb_->clk_;
    return d_sco;
}

void Piconet::addScoLink(LMPLink * l)
{
    if (numScoLink == 0) {
	scoLink = l;
	l->prev = l->next = l;
    } else {
	l->next = scoLink;
	l->prev = scoLink->prev;
	scoLink->prev->next = l;
	scoLink->prev = l;
    }
    numScoLink++;
    lmp_->scoPico = this;
}

void Piconet::removeScoLink(LMPLink * l)
{
    if (numScoLink == 1) {
	scoLink = 0;
    } else {
	l->prev->next = l->next;
	l->next->prev = l->prev;
	if (scoLink == l) {
	    scoLink = l->next;
	}
    }
    numScoLink--;

    for (int i = 0; i < 6; i++) {
	if (sco_table[i] == l) {
	    sco_table[i] = 0;
	}
    }
}

LMPLink *Piconet::lookupScoLink(int connh)
{
    LMPLink *l = scoLink;
    for (int i = 0; i < numScoLink; i++) {
	if (l->sco_hand == connh) {
	    return l;
	}
	l = l->next;
    }
    return NULL;
}

// Handle SCO link in a bridging setting.  That is, if a bridge has a
// SCO link, piconet switch has to be done every T_SCO slot.
// This fuction intiate this switch mechanism.
void Piconet::schedule_SCO()
{
    Scheduler & s = Scheduler::instance();

    // find the next SCO frame.  Implicit assumption, only a single HV3 link.

    LMPLink *sco = scoLink;
    clk_t inst = lmp_->bb_->clk_ / (sco->T_poll_ << 1) * (sco->T_poll_ << 1)
	+ (sco->D_sco << 1);
    if (inst <= lmp_->bb_->clk_) {
	inst += (sco->T_poll_ << 1);
    }

    double t = (inst - (lmp_->bb_->clk_ & 0xFFFFFFFC)) * lmp_->bb_->tick()
	+ lmp_->bb_->t_clk_00_ - s.clock();
    _sco_ev.link = sco;
    _sco_suspend_ev.link = sco;
    s.schedule(&lmp_->_timer, &_sco_ev, t);
}

void Piconet::cancel_schedule_SCO()
{
    Scheduler & s = Scheduler::instance();

    s.cancel(&_sco_ev);
    s.cancel(&_sco_suspend_ev);
}

// return: number of HV3 links
// if aligned, a HV3 link takes up 2 slots, otherwise 4 slots.
int Piconet::updateSchedWord(BTSchedWord * sw, bool isMa, bool aligned)
{
    if (numScoLink == 0) {
	return 0;
    }

    int numSlink = 0;
    if (isMa) {
	// should consider a 12-slot frame instead, where a HV3 takes up
	// 4 slots, a HV2 takes up 6 and a HV1 takes up 12 slots.
	sw->expand(12);	
    }
    LMPLink *link = scoLink;
    for (int i = 0; i < numScoLink; i++) {
	// a HV2 link counts as 2 HV3 links
	numSlink += (link->T_poll_ == 2 ? 3 : (link->T_poll_ == 4 ? 2 : 1));
	if (isMa) {
	    int dsco = link->D_sco;
	    //XXX D_sco should consider clock offset!!!
	    if (!aligned) {
		
		fprintf(stderr, "Inquiry/Page with a slave SCO link is not "
			"supported at this moment. --check lmp-piconet.cc\n");
		abort();

#if 0
		sw->word[dsco] = sw->word[dsco + 1] =
		    Baseband::NotAllowedSlot;
		if (dsco == 0) {
		    sw->word[5] = sw->word[2] = Baseband::NotAllowedSlot;
		} else if (dsco == 4) {
		    sw->word[3] = sw->word[0] = Baseband::NotAllowedSlot;
		} else {
		    sw->word[dsco - 1] = sw->word[dsco + 2] =
			Baseband::NotAllowedSlot;
		}
#endif
	    } else {
		for (int j = 0; j < 12 / link->T_poll_; j++) { 
		    sw->word[dsco + j * link->T_poll_] = 
			sw->word[dsco  + j * link->T_poll_ + 1] =
			Baseband::NotAllowedSlot;
		}
	    }
	}
	link = link->next;
    }
    return numSlink;
}

int Piconet::isMaster()
{
    return master_bd_addr_ == lmp_->_my_info->bd_addr_;
}

void Piconet::clear_skip()
{
    LMPLink *wk = activeLink;
    int i;
    for (i = 0; i < numActiveLink; i++) {
	wk->skip_ = 0;
	wk = wk->next;
    }

    wk = suspendLink;
    for (i = 0; i < numSuspendLink; i++) {
	wk->skip_ = 0;
	wk = wk->next;
    }
}

LMPLink *Piconet::lookupLink(bd_addr_t bd)
{
    LMPLink *wk = activeLink;
    int i;
    for (i = 0; i < numActiveLink; i++) {
	if (wk->remote->bd_addr_ == bd) {
	    return wk;
	}
	wk = wk->next;
    }
    wk = suspendLink;
    for (i = 0; i < numSuspendLink; i++) {
	if (wk->remote->bd_addr_ == bd) {
	    return wk;
	}
	wk = wk->next;
    }
    return NULL;
}

LMPLink *Piconet::lookupScoLinkByAddr(bd_addr_t bd)
{
    LMPLink *wk = scoLink;
    for (int i = 0; i < numScoLink; i++) {
	if (wk->remote->bd_addr_ == bd) {
	    return wk;
	}
	wk = wk->next;
    }
    return NULL;
}

void Piconet::setSchedWord()
{
    compute_sched();
    // _sched_word = (isMaster() ? &_mword : &_sword);
    lmp_->bb_->set_sched_word(_sched_word);
}

// In case that a foreign SCO link exists, it will occupy 4 out of 6 slots
// in every 6 slot block, because the slots are not aligned.
// This method will try to get the still available master slot in case of the 
// presence of such foreign SCO link
//
// Return: the free master slot.
int Piconet::scoUnoccupiedSlot()
{
    // double block = lmp_->bb_->slotTime() * 6;

    // find the current clk for the two piconets.
    int clk = (lmp_->bb_->clkn_ & 0xFFFFFFFC) + clk_offset;
    int clk_sco =
	(lmp_->bb_->clkn_ & 0xFFFFFFFC) + lmp_->scoPico->clk_offset;

    // find the beginning time of the current blocks
    double t1 = lmp_->bb_->t_clkn_00_ + slot_offset * 1E-6;
    double t2 = lmp_->bb_->t_clkn_00_ + lmp_->scoPico->slot_offset * 1E-6;

    t1 += (2 - ((clk >> 2) % 3)) * 2 * lmp_->bb_->slotTime();
    t2 += (2 - ((clk_sco >> 2) % 3)) * 2 * lmp_->bb_->slotTime();

    // find the beginning time of the sco links.
    t2 += lmp_->bb_->slotTime() * lmp_->scoPico->scoLink->D_sco;

    int busySlot = (((int) ((t2 - t1) / lmp_->bb_->slotTime())) + 12) % 6;
    busySlot &= 0xFFFFFFFE;
    return (busySlot + 4) % 6;
}

// There are 3 types of LMPLink: tight, high, and low.  We always have a fixed
// Schedule for 'Tight' class and suggested schedule for 'High' class. So we
// compute the schedule of them here.  For 'Low' class, the schedule is taken
// care by the baseband scheduling alogorithm.  We refer 'Tight' class as
// SCO links, and 'High' class as QoS granted Links.

//  -- update: let's say we only compute schedule for Tight class and
//              have an estimate if the QoS can be satisfied.  Let's let
//              the link schduler to do the scheduling job.

// when switch from one piconet to another, the SCO link of the orignal
// piconet is still maitained. Therefore, when computing the sched_word
// for the new piconet, we still need to consider those SCO links.
int Piconet::compute_sched()
{
    if (numActiveLink == 0 && numScoLink == 0) {
	if (isMaster()) {
	    _sched_word = new BTSchedWord(2);
	} else {
	    _sched_word = new BTSchedWord(false);
	}
	// printf("LMP %d ", lmp_->bb_->bd_addr_);
	// _sched_word->dump();
	return 0;
    }
    // _sched_word = (isMaster() ? &_mword : &_sword);
    // return 0;

    _res = 1;			// Total resource: 100%
    int i;
    int noQosReqNum = 0;
    double res_req = 0;
    int lcm = 1;
    // Piconet *anotherScoPico;
    BTSchedWord *sw;
    int isslave;
    LMPLink *wk;
    int ptslotnum;		// slot num of sending packets
    int rptslotnum;		// slot num of receiving packets

    if (_sched_word && !_sched_word->in_use) {
	delete _sched_word;
    }
    // CASE 1:
    // 
    // two Piconets with SCO Link exists.
    // in this case, each of them has a single SCO link with T_sco = 6.
    if (lmp_->scoPico && lmp_->scoPico1) {

	// I'm not eithe one.  No chane for me.
	if (lmp_->scoPico != this && lmp_->scoPico1 != this) {
	    _sched_word = new BTSchedWord(false);
	    _res = 0;
	    return 0;
	}
	// I'm one the two piconets with SCO link.
	Piconet *scop = this;
	scop->_res = 0;
	sw = new BTSchedWord(6, false);
	isslave = (scop->isMaster()? 0 : 1);
	sw->word[scop->scoLink->D_sco + isslave]
	    = scop->scoLink->txBuffer->slot();
	scop->_sched_word = sw;

	return 0;
    }
    // CASE 2:
    // 
    // There exists a single Sco Piconet and it's not me.
    // in this case, I get at most 1/3 resourses.
    if (lmp_->scoPico && lmp_->scoPico != this) {
	if (lmp_->scoPico->numScoLink > 1 || lmp_->scoPico->scoLink->T_poll_
	    < 6) {
	    _res = 0;
	    _sched_word = new BTSchedWord(false);
	    return 0;
	}

	_res = 1.0 / 3;
	if (!isMaster()) {
	    _sched_word = new BTSchedWord(false);
	    return 0;
	}

	/* in every 6 slots, only the unoccupied'th slot is available */
	int unoccupied = scoUnoccupiedSlot();

	wk = activeLink;
	lcm = 6;
	noQosReqNum = 0;
	res_req = 0;
	for (i = 0; i < numActiveLink; i++) {
	    if ((ptslotnum = hdr_bt::slot_num(wk->pt)) > 1 ||
		(rptslotnum = hdr_bt::slot_num(wk->rpt)) > 1) {
		printf("Conflict packet type in present of SCO link.\n");
		//return 0;
		exit(-2);
	    }
	    if (wk->_noQos) {
		noQosReqNum++;
	    } else {
		res_req += (ptslotnum + rptslotnum * 1.0) / wk->T_poll_;
		lcm = _lcm(lcm, wk->T_poll_);
	    }
	    wk = wk->next;
	}

	int freeSlotNum = (int) (lcm * (_res - res_req) + 0.5);
	int T_poll_noqos;
	if (noQosReqNum == 0) {
	    T_poll_noqos = Baseband::T_poll_max_;
	} else if (freeSlotNum <= 0) {
	    printf("No capacity for noQos link");
	    exit(-2);
	    // return 0;
	} else {
	    int tmp = _lcm(freeSlotNum, noQosReqNum * 2);
	    tmp /= freeSlotNum;
	    lcm *= tmp;
	    T_poll_noqos = lcm * 2 / (tmp * freeSlotNum / noQosReqNum);
	}

	sw = new BTSchedWord(lcm, false);
	wk = activeLink;
	for (i = 0; i < numActiveLink; i++) {
	    int t;
	    int tp;

	    ptslotnum = 1;	// only 1-slot packet possible
	    rptslotnum = 1;

	    for (t = unoccupied; t < lcm; t += unoccupied) {
		if (sw->word[t] == Baseband::RecvSlot) {
		    break;
		}
	    }
	    if (wk->_noQos) {
		tp = T_poll_noqos;
		wk->T_poll_ = T_poll_noqos;
		// _res -= (slotn + 1.0) / Baseband::T_poll_max;
	    } else {
		tp = wk->T_poll_;
		_res -= (ptslotnum + rptslotnum * 1.0) / wk->T_poll_;
	    }

	    int mark = 0;
	    for (; t < lcm; t += tp) {	// only 1-slot packet possible
		sw->word[t] = wk->txBuffer->slot();

		// temp fix. need new design. search free slot first.
		// anyway, T_poll_ or tp should always be even ???
		if ((tp & 0x01)) {
		    if (mark++ % 2) {
			t++;
		    } else {
			t--;
		    }
		}
	    }
	    wk = wk->next;
	}
	// printf("LMP %d ", lmp_->bb_->bd_addr_);
	// sw->dump();
	_sched_word = sw;
	return 0;
    }
    // CASE 3:
    // 
    //  No external SCO link
    //  I may or may not have SCO links
    lcm = 1;
    noQosReqNum = 0;
    res_req = 0;

    wk = scoLink;
    for (i = 0; i < numScoLink; i++) {
	res_req += 2.0 / wk->T_poll_;
	lcm = _lcm(lcm, wk->T_poll_);
    }

    int maxSlot2way = (res_req > .6 ? 2 : (res_req > 0.3 ? 4 : 10));

    wk = activeLink;
    for (i = 0; i < numActiveLink; i++) {
	ptslotnum = hdr_bt::slot_num(wk->pt);
	rptslotnum = hdr_bt::slot_num(wk->rpt);

	if (ptslotnum + rptslotnum > maxSlot2way) {
	    printf("Conflict packet type in present of SCO link.\n");
	    //return 0;
	    exit(-2);
	}

	if (wk->_noQos) {
	    noQosReqNum++;

	    // This code need rework to handle fair allocation to nonQOS link.
	    if (ptslotnum == 3) {
		noQosReqNum++;
	    } else if (ptslotnum == 5) {
		noQosReqNum++;
		noQosReqNum++;
	    }
	    if (rptslotnum == 3) {
		noQosReqNum++;
	    } else if (rptslotnum == 5) {
		noQosReqNum++;
		noQosReqNum++;
	    }

	} else {
	    res_req += (ptslotnum + rptslotnum * 1.0) / wk->T_poll_;
	    lcm = _lcm(lcm, wk->T_poll_);
	}
	wk = wk->next;
    }

    int freeSlotNum = (int) (lcm * (1 - res_req) + 0.5);
    int T_poll_noqos;
    if (noQosReqNum == 0) {
	T_poll_noqos = Baseband::T_poll_max_;
    } else if (freeSlotNum <= 0) {
	printf("No capacity for noQos link");
	exit(-2);
	// return 0;
    } else {
	int tmp = _lcm(freeSlotNum, noQosReqNum * 2);
	tmp /= freeSlotNum;
	lcm *= tmp;
	T_poll_noqos = lcm * 2 / (tmp * freeSlotNum / noQosReqNum);
    }

    sw = new BTSchedWord(lcm, false);

    int slave = (isMaster()? 0 : 1);
    wk = scoLink;
    for (i = 0; i < numScoLink; i++) {
	_res -= 2.0 / wk->T_poll_;
	for (int t = wk->D_sco; t < lcm; t += wk->T_poll_) {
	    sw->word[t + slave] = wk->txBuffer->slot();
	    sw->word[t + 1 - slave] = Baseband::ScoRecvSlot;
	}
	wk = wk->next;
    }

    _sched_word = sw;
    if (!isMaster()) {
	// printf("LMP %d ", lmp_->bb_->bd_addr_);
	// sw->dump();
	return 0;
    }

    wk = activeLink;
    for (i = 0; i < numActiveLink; i++) {
	int t;
	int tp;
	ptslotnum = hdr_bt::slot_num(wk->pt);
	rptslotnum = hdr_bt::slot_num(wk->rpt);
	int slotnum2way = ptslotnum + rptslotnum;

	// Search for a frame bigger enough
	for (t = 0; t < lcm; t++, t++) {
	    if (slotnum2way == 2 && sw->word[t] == Baseband::RecvSlot) {
		break;
	    } else if (slotnum2way == 4
		       && sw->word[t] == Baseband::RecvSlot
		       && sw->word[t + 2] == Baseband::RecvSlot) {
		break;
	    } else if (slotnum2way == 6
		       && sw->word[t] == Baseband::RecvSlot
		       && sw->word[t + 2] == Baseband::RecvSlot
		       && sw->word[t + 4] == Baseband::RecvSlot) {
		break;
	    } else if (slotnum2way == 8
		       && sw->word[t] == Baseband::RecvSlot
		       && sw->word[t + 2] == Baseband::RecvSlot
		       && sw->word[t + 4] == Baseband::RecvSlot
		       && sw->word[t + 6] == Baseband::RecvSlot) {
		break;
	    } else if (slotnum2way == 10
		       && sw->word[t] == Baseband::RecvSlot
		       && sw->word[t + 2] == Baseband::RecvSlot
		       && sw->word[t + 4] == Baseband::RecvSlot
		       && sw->word[t + 6] == Baseband::RecvSlot
		       && sw->word[t + 8] == Baseband::RecvSlot) {
		break;
	    }
	}

	if (wk->_noQos) {
	    tp = T_poll_noqos;
	    wk->T_poll_ = T_poll_noqos;
	    // _res -= (slotn + 1.0) / Baseband::T_poll_;
	} else {
	    tp = wk->T_poll_;
	    _res -= (slotnum2way * 1.0) / wk->T_poll_;
	}

	int mark = 0;
	for (; t < lcm; t += tp) {
	    sw->word[t] = wk->txBuffer->slot();
	    for (int tt = 2; tt < slotnum2way; tt++, tt++) {
		// sw->word[t + tt] = Baseband::NotAllowedSlot;
		// in the following case, bb has to record tx state.
		// ie, knowing when can send next pkt.
		sw->word[t + tt] = Baseband::DynamicSlot;
	    }

	    // temp fix. need new design. search free slot first.
	    if ((tp & 0x01)) {
		if (mark++ % 2) {
		    t++;
		} else {
		    t--;
		}
	    }
	}
	wk = wk->next;
    }
    // printf("LMP %d ", lmp_->bb_->bd_addr_);
    // sw->dump();
    return 0;
}

BTSchedWord *Piconet::compute_bb_sco_sched()
{
    return compute_bb_sco_sched(isMaster());
}

BTSchedWord *Piconet::compute_bb_sco_sched(int m)
{
    BTSchedWord *sw = new BTSchedWord(*_sched_word, 6);
    int s0 = 0;
    int s4 = 0;

    for (int i = 0; i < sw->len; i++) {
	if (sw->word[i] >= Baseband::MinTxBufferSlot
	    && lmp_->bb_->txBuffer(sw->word[i])->link()->type() == SCO) {
	    if (m) {
		sw->word[i + 1] = Baseband::ScoRecvSlot;
		if (i - 2 < 0
		    || sw->word[i - 2] < Baseband::MinTxBufferSlot
		    || lmp_->bb_->txBuffer(sw->word[i - 2])->link()->
		    type() != SCO) {
		    sw->word[i - 1] = Baseband::ScoPrioSlot;
		}
		i++;
	    } else {
		sw->word[i - 1] = Baseband::ScoRecvSlot;
		if (i - 2 < 0
		    || sw->word[i - 2] < Baseband::MinTxBufferSlot
		    || lmp_->bb_->txBuffer(sw->word[i - 2])->link()->
		    type() != SCO) {
		    sw->word[i - 2] = Baseband::ScoPrioSlot;
		}
	    }
	    if (i < 2) {
		s0 = 1;
	    } else if (i > 3) {
		s4 = 1;
	    }
	} else if (m && i % 2 == 1) {
	    sw->word[i] = 1;
	} else {
	    sw->word[i] = 0;
	}
    }

    if (s0 == 1 && s4 == 0) {
	sw->word[5] = Baseband::ScoPrioSlot;
    }

    _sched_word = sw;
    return sw;
}

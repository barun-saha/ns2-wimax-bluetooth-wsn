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

/*
 *	lmp-link.cc
 */

//** Added by Barun; for MIN/MAX
#include <sys/param.h>

#include "baseband.h"
#include "lmp.h"
#include "l2cap.h"
#include "lmp-piconet.h"
#include "gridkeeper.h"
#include "random.h"
#include "scat-form.h"
#include "bt-node.h"

// #define PRINT_SUSPD_BUF_LINK
//#define SNIFF_ADJ

//////////////////////////////////////////////////////////
//                      LMPLink                         //
//////////////////////////////////////////////////////////

LMPLink::LMPLink(Bd_info * rm, LMP * lmp, ConnectionHandle * con)
{
    _init();
    lmp_ = lmp;
    remote = rm;
    connhand = con;
    txBuffer = lmp_->bb_->allocateTxBuffer(this);
    if (con) {
	con->setLink(this);
	// pt = (hdr_bt::packet_type) con->packet_type;
	// rpt = (hdr_bt::packet_type) con->recv_packet_type;
    }
}

LMPLink::LMPLink(LMPLink * acl_, LMP * lmp, ConnectionHandle * con,
		 int type)
{
    _init();
    acl = acl_;
    lmp_ = lmp;
    lt_addr_ = acl_->lt_addr_;
    remote = acl_->remote;
    piconet = acl_->piconet;
    connhand = con;
    txBuffer = lmp_->bb_->allocateTxBuffer(this);
    _type = type;
    if (_type != 0) {
	acl->addSCOLink(this);
	// acl->num_sco++;
    }
    if (con) {
	con->setLink(this);
	// pt = (hdr_bt::packet_type) con->packet_type;
	// rpt = (hdr_bt::packet_type) con->recv_packet_type;
    }
}

// Special ACL Link for broadcasting
LMPLink::LMPLink(LMP * lmp, int txslot)
{
    _init();
    lmp_ = lmp;

    // in bcast, connhand is basically one-way from
    // host to host controller. Received packet should
    // map to connhand for the ACL to the master ??
    connhand = 0;

    txBuffer = lmp_->bb_->txBuffer(txslot);
    remote = new Bd_info(BD_ADDR_BCAST, 0);
    txBuffer->setLink(this);
    piconet = lmp_->masterPico;
}

LMPLink::~LMPLink()
{
    if (connhand) {
	delete connhand;
    }

    Scheduler & s = Scheduler::instance();

    if (_hold_ev) {
	s.cancel(_hold_ev);
    }
    if (_hold_ev_wakeup) {
	s.cancel(_hold_ev_wakeup);
    }
    if (_sniff_ev) {
	s.cancel(_sniff_ev);
    }
    if (_sniff_ev_to) {
	s.cancel(_sniff_ev_to);
    }
    if (_park_ev) {
	s.cancel(_park_ev);
    }
    if (_park_ev_en) {
	s.cancel(_park_ev_en);
    }
}

void LMPLink::_init()
{
    prev = next = this;
    event = None;
    rs = 0;
    acl = 0;
    num_sco = 0;
    for (int i = 0; i < 3; i++) {
	sco[i] = 0;
    }
    lmp_ = 0;
    lt_addr_ = pm_addr = ar_addr = 0;
    remote = 0;
    txBuffer = 0;
    reset_as_ = 0;
    piconet = 0;
    connhand = 0;
    reqScoHand = 0;
    pt = hdr_bt::DH1;
    rpt = hdr_bt::DH1;
    _slot_offset = 0;
    clk_offset = 0;
    sent_setup_complete = 0;
    connected = 0;
    send_conn_ev = 0;
    readyToRelease = 0;
    _noQos = 1;
    sco_req = 0;
    sco_remv = NULL;
    sco_hand = -1;

    skip_ = 0;
    _lastSynClk = -1;

    holdreq = 0;
    parkreq = 0;
    sniffreq = 0;
    rpFixed = 0;
    _hold_ev = 0;
    _hold_ev_wakeup = 0;
    _sniff_ev = 0;
    _sniff_ev_to = 0;
    _park_ev = 0;
    _park_ev_en = 0;

    _wakeupT = -1;
    suspended = 0;
    _parked = 0;
    _unpark = 0;
    _on_hold = 0;		// 1 if hold event is scheduled
    _in_sniff = 0;
    _in_sniff_attempt = 0;
    needAdjustSniffAttempt = 0;
    _type = 0;
    delayedT = 0;

    ev = 0;
    disconnReason = BTDISCONN_LINKDOWN;
    tobedetached = 0;

    _curL2capPkt = 0;
    _curL2capPkt_remain_len = 0;

    curUse = 0;
    curSchedWind = -1;
    indexInSchedWind = -1;
}

void LMPLink::addSCOLink(LMPLink * s)
{
    if (num_sco >= 3) {
	fprintf(stderr, "Too many SCOs %d\n", num_sco);
	abort();
    }
    for (int i = 0; i < 3; i++) {
	if (!sco[i]) {
	    sco[i] = s;
	    ++num_sco;
	    break;
	}
    }
}

void LMPLink::removeScoLink(LMPLink * s)
{
    for (int i = 0; i < 3; i++) {
	if (sco[i] == s) {
	    sco[i] = 0;
	    --num_sco;
	    break;
	}
    }

    // If the ACL is not used by higher layer, it is detached too.
    if (num_sco == 0 && connhand->numChan == 0) {
	lmp_->HCI_Disconnect(connhand, s->disconnReason);
    };

    //XXX: do we need to notify higher layer
    // lmp_->node_->linkDetached(remote->bd_addr_, s->disconnReason); 

    delete s;
}

// Fragment is moved down form L2CAP to here, so it can be done dynamically.
// That is, packet type can be changed according to Channel conditions.
Packet *LMPLink::getAL2capPktFragment()
{
    if (!_curL2capPkt) {
	if (_l2capQ.length() <= 0 && connected && connhand
	    && connhand->head) {
	    L2CAPChannel *wk = connhand->head;
	    L2CAPChannel *newhead = wk;

	    // XXX -- This should be moved up to L2CAP layer !

	    // Multiple L2CAPChannels may share a single LMPLink
	    // get a packet from L2CAPChannel in a robin fashion
	    // FCFS is not implemented. --- XXX
	    // 
	    // Notes:  This does affect IP applications run over BNEP,
	    //         as a BNEP connection maps to a single L2CAPChannel.
	    // It is meaningful only when multiple protocols run over L2CAP.

	    switch (lmp_->l2capChPolicy_) {
	    case LMP::l2RR:

		//XXX to make FCFS come, we have to peek the IFQ, which is not 
		//    allowed.
	    case LMP::l2FCFS:

		do {
		    if (wk->ready_) {
			wk->send();
		    }
		    newhead = wk->linknext;	// move the head to linknext

		    if (_l2capQ.length() > 0) {
			// connhand->head = newhead;
			break;
		    }
		} while ((wk = wk->linknext) != connhand->head);
		break;

#if 0
	    case LMP::l2FCFS:
		do {
		} while ((wk = wk->linknext) != connhand->head);
		break;
#endif
	    }
	}
	// wk->send() already put packets into txBuffer.
	if (!txBuffer->available()) {
	    return NULL;
	}

	if (!_curL2capPkt && _l2capQ.length() > 0) {
	    _curL2capPkt = _l2capQ.deque();
	    _curL2capPkt_remain_len = HDR_CMN(_curL2capPkt)->size();
	    _fragno = 0;
	}
	if (!_curL2capPkt) {
	    return NULL;
	}
	// _curL2capPkt_remain_len = HDR_L2CAP(_curL2capPkt)->length;
    }
    // Fragment the l2cap pkt.

    hdr_cmn *ch = HDR_CMN(_curL2capPkt);
    // hdr_l2cap *lh = HDR_L2CAP(_curL2capPkt);
    hdr_bt *bh = HDR_BT(_curL2capPkt);
    hdr_bt::packet_type packet_type = pt;
    int payload_size = hdr_bt::payload_size(packet_type);
    bh->ph.length = (_curL2capPkt_remain_len > payload_size ?
		     payload_size : _curL2capPkt_remain_len);
#ifdef VARIABLE_PKT
    if (packet_type == hdr_bt::DH5 && bh->ph.length <= 183) {
	if (bh->ph.length <= 27) {
	    packet_type = hdr_bt::DH1;
	} else {
	    packet_type = hdr_bt::DH3;
	}
    } else if (packet_type == hdr_bt::DM5 && bh->ph.length <= 121) {
	if (bh->ph.length <= 17) {
	    packet_type = hdr_bt::DM1;
	} else {
	    packet_type = hdr_bt::DM3;
	}
    } else if (packet_type == hdr_bt::DH3 && bh->ph.length <= 27) {
	packet_type = hdr_bt::DH1;
    } else if (packet_type == hdr_bt::DM3 && bh->ph.length <= 17) {
	packet_type = hdr_bt::DM1;
    } else if (packet_type == hdr_bt::DH5_3 && bh->ph.length <= 552) {
	if (bh->ph.length <= 83) {
	    packet_type = hdr_bt::DH1_3;
	} else {
	    packet_type = hdr_bt::DH3_3;
	}
    } else if (packet_type == hdr_bt::DH5_2 && bh->ph.length <= 367) {
	if (bh->ph.length <= 54) {
	    packet_type = hdr_bt::DH1_2;
	} else {
	    packet_type = hdr_bt::DH3_2;
	}
    } else if (packet_type == hdr_bt::DH3_3 && bh->ph.length <= 83) {
	packet_type = hdr_bt::DH1_3;
    } else if (packet_type == hdr_bt::DH3_2 && bh->ph.length <= 54) {
	packet_type = hdr_bt::DH1_2;
    }
#endif

    bh->size = hdr_bt::packet_size(packet_type, bh->ph.length);
    bh->type = packet_type;
    ch->size() = bh->size / 8;
    bh->seqno = _fragno++;

    if (_fragno == 1) {
	bh->pid = hdr_bt::pidcntr++;
	bh->transmitCount = 0;
	bh->receiver = remote->bd_addr_;
	bh->ph.l_ch = L_CH_L2CAP_START;	// L2CAP new packet
    } else {
	bh->ph.l_ch = L_CH_L2CAP_CONT;	// L2CAP cont'd packet
    }
    _curL2capPkt_remain_len -= bh->ph.length;

    Packet *ret = _curL2capPkt;
    if (_curL2capPkt_remain_len > 0) {
	_curL2capPkt = _curL2capPkt->copy();
    } else {
	_curL2capPkt = NULL;
    }

    return ret;
}

int LMPLink::callback()
{
    if (!txBuffer->available()) {
	return 0;
    }
    if (suspended || _parked) {
	return 0;
    }

    Packet *p = NULL;
    if (_lmpQ.length() > 0) {
	p = _lmpQ.deque();
    } else if (_type == ACL) {
	p = getAL2capPktFragment();
    } else {
	p = _l2capQ.deque();	// SCO
    }

    return (p ? txBuffer->push(p) : 0);
}

void LMPLink::sendNull()
{
    if (txBuffer->available()) {
	Packet *p = Packet::alloc();
	hdr_cmn *ch = HDR_CMN(p);
	ch->ptype() = PT_BT;
	hdr_bt *bh = HDR_BT(p);
	bh->pid = hdr_bt::pidcntr++;
	bh->type = hdr_bt::Null;
	bh->arqn = 1;
	bh->size = hdr_bt::packet_size(bh->type);
	bh->ph.l_ch = L_CH_LM;
	bh->receiver = remote->bd_addr_;
	bh->comment("K");

	txBuffer->push(p);
    }
}

Packet *LMPLink::genPollPkt(int ack)
{
    Packet *p = Packet::alloc();
    hdr_cmn *ch = HDR_CMN(p);
    ch->ptype() = PT_BT;
    hdr_bt *bh = HDR_BT(p);
    bh->pid = hdr_bt::pidcntr++;
    bh->type = hdr_bt::Poll;
    bh->arqn = ack;
    bh->size = hdr_bt::packet_size(bh->type);
    bh->ph.l_ch = L_CH_LM;
    bh->receiver = remote->bd_addr_;
    bh->comment("O");
    return p;
}

void LMPLink::sendPoll(int ack)
{
    if (txBuffer->available()) {
	Packet *p = genPollPkt(ack);
	txBuffer->push(p);
    }
}

void LMPLink::sendACK()
{
    if (callback() == 0) {
	txBuffer->mark_ack();
    }
}

// The interface queue is mainly located at L2CAP.
// What was put in the queues here is quite limited.
void LMPLink::enqueue(Packet * p)
{
    hdr_bt *bh = HDR_BT(p);

    if (bh->ph.l_ch == L_CH_LM) {	// lmp message
	_lmpQ.enque(p);
    } else {
	_l2capQ.enque(p);
    }

#if 1
    if (txBuffer->available()) {
	if ((p = _lmpQ.deque()) != 0) {
	    txBuffer->push(p);
	} else if (_type == ACL && (p = getAL2capPktFragment())) {
	    txBuffer->push(p);
	} else if (_type == SCO && (p = _l2capQ.deque())) {
	    txBuffer->push(p);
	}
    }
#endif
}

// Change
void LMPLink::flushL2CAPPkt()
{
    Packet *p = _l2capQ.head();
    while (p && HDR_BT(p)->ph.l_ch == L_CH_L2CAP_CONT) {
	_l2capQ.deque();
	p = _l2capQ.head();
    }
}

double LMPLink::lastPktRecvTime()
{
    if (txBuffer->lastPktRecvSlot() > 0) {
	int slots = lmp_->bb_->clk_ - txBuffer->lastPktRecvSlot();
	return lmp_->bb_->t_clk_00_ - slots * lmp_->bb_->tick();
    }
    return -1;
}

double LMPLink::lastDataPktRecvTime()
{
    if (txBuffer->lastDataPktRecvSlot() > 0) {
	int slots = lmp_->bb_->clk_ - txBuffer->lastDataPktRecvSlot();
	return lmp_->bb_->t_clk_00_ - slots * lmp_->bb_->tick();
    }
    return -1;
}

uint16_t LMPLink::lookup_slot_offset(bd_addr_t addr)
{
    SlotOffset *wk = _slot_offset;
    while (wk) {
	if (wk->bd_addr == addr) {
	    return wk->offset;
	}
	wk = wk->next;
    }
    return 0;
}

void LMPLink::add_slot_offset(SlotOffset * off)
{
    SlotOffset *wk = _slot_offset;
    while (wk) {
	if (wk->bd_addr == off->bd_addr) {
	    wk->offset = off->offset;
	}
	wk = wk->next;
    }
    wk = new SlotOffset(off->offset, off->bd_addr);
    wk->next = _slot_offset;
    _slot_offset = wk;
}

void LMPLink::send_slave_info()
{
#if 0
    Slave_info_msg msg;
    Slave_info *info = new Slave_info[piconet->numSlave - 1];
    int ind = 0;
    int i;

    LMPLink *wk = piconet->activeLink;
    for (i = piconet->numActiveLink - 1; i > 0; i--) {
	if (wk == this) {
	    wk = wk->next;
	}
	info[ind].addr = wk->remote->bd_addr_;
	info[ind].lt_addr_ = wk->lt_addr_;
	info[ind].active = 1;
	ind++;
	wk = wk->next;
    }
    wk = piconet->suspendLink;
    for (i = piconet->numSuspendLink; i > 0; i--) {
	info[ind].addr = wk->remote->bd_addr_;
	info[ind].lt_addr_ = wk->lt_addr_;
	info[ind].active = 1;
	ind++;
	wk = wk->next;
    }
    ind--;
    if (ind > 0 && ind % 2) {
	msg.num = 1;
	msg.s1 = info[ind];
	lmp_->lmpCommand(LMP::LMP_SLAVE_INFO, (uchar *) & msg, sizeof(msg),
			 8, this);
	ind--;
    }
    for (i = ind; i > 0; i--) {
	msg.num = 2;
	msg.s1 = info[i--];
	msg.s1 = info[i];
	lmp_->lmpCommand(LMP::LMP_SLAVE_INFO, (uchar *) & msg, sizeof(msg),
			 15, this);
    }
#endif
}

// receving lots of POLL/NULL pkt. The peer has nothing useful to send.
// increase T_poll ??
void LMPLink::nullTrigger()
{
}

// send failure.  It's maybe the peer is participating in other piconet.
// Notify LMP to stop sending data packet.  LMP should poll the peer at a
// greater interval.
void LMPLink::failTrigger()
{
    if (piconet->isMaster()) {
#ifdef PRINT_SUSPD_BUF_LINK
	printf("Suspend txBuffer.\n");
#endif
	txBuffer->suspend();
    }
}


//////////////////////////////////////////////////////////////
//                                                          //
//                        Role Switch                       //
//                                                          //
//////////////////////////////////////////////////////////////
/* 1. M -> S
 *    a) M has a single link. simple case.
 *    b) Become a M/S Bridge. Only this link is affected.
 *    c) hand the whole piconet over to S  (Seems this case is dismissed in 1.2)
 *       i) perform MS switch
 *       ii) send all slave info
 *
 * 2. S -> M
 *    a) S has a Master Piconet suspended, merge this link.
 *    b) S don't have a Master Piconet, S takes over the M.
 *       i) perform MS switch
 *       -------------- in spec 1.2 the following are not mentioned any more.
 *       ii) get all slave info
 *       iii) for each slave, send new FHS and verify the switch
 *
 * Steps:
 *    a) TDD switch
 *	 new slave using the old slave's lt_addr_. using old pico parameter.
 *
 *    b) Piconet switch
 *       change to new piconet parameter (clk, fs, slot offset)
 *
 *    c) Piconet switch for other slaves
 */

void LMPLink::handle_role_switch_tdd()
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();
    int offset;

    if (piconet->isMaster()) {	// I'm master, to switch to slave Role.
	piconet->_sched_word = new BTSchedWord(false);
	// piconet->master = remote;	// piconet is no longer masterPico
	piconet->master_bd_addr_ = remote->bd_addr_;
	// lmp_->masterPico = NULL;
	lmp_->bb_->change_state(Baseband::ROLE_SWITCH_SLAVE);
	offset = lookup_slot_offset(remote->bd_addr_);

    } else {			// I'm slave, to switch to master Role.
	piconet->_sched_word = new BTSchedWord(2);
	// piconet->master = lmp_->_my_info;	// becomes master piconet;
	piconet->master_bd_addr_ = lmp_->_my_info->bd_addr_;
	// There may have 2 master piconets at this moment:
	// curPico and suspendPico
	//lmp_->masterPico_old = lmp_->masterPico;
	//lmp_->masterPico = piconet;

	lmp_->bb_->change_state(Baseband::ROLE_SWITCH_MASTER);
	offset =
	    (int) ((lmp_->bb_->t_clkn_00_ - lmp_->bb_->t_clk_00_) * 1E6);
	if (offset < 0) {
	    offset += 1250;
	}
	// Why need clk_offset ?
	clk_offset = (lmp_->bb_->clk_ & 0xFFFFFFFC) -
	    (lmp_->bb_->clkn_ & 0xFFFFFFFC);
	if (now - lmp_->bb_->t_clkn_00_ >= offset * 1E-6) {
	    clk_offset += 4;
	}
	lmp_->bb_->slave_lt_addr_ = lmp_->get_lt_addr(remote->bd_addr_);
    }

    lmp_->bb_->slot_offset_ = offset;
    lmp_->bb_->set_sched_word(piconet->_sched_word);
    // lmp_->bb_->slave_lt_addr_ = lt_addr_;
    lmp_->bb_->slave_ = remote->bd_addr_;
    slot_offset = offset;

    if (lmp_->trace_state()) {
	printf("** slot_offset: %d (%d->%d) new ltaddr: %d\n",
	       lmp_->bb_->slot_offset_,
	       remote->bd_addr_, lmp_->_my_info->bd_addr_,
	       lmp_->bb_->slave_lt_addr_);
	lmp_->dump(stdout, 1);
    }
}

void LMPLink::handle_rs_takeover()
{
    // BTSchedWord *sw = new BTSchedWord(2);
    lmp_->bb_->slot_offset_ = slot_offset;
    lmp_->bb_->set_sched_word(piconet->_sched_word);
    lmp_->bb_->slave_lt_addr_ = lt_addr_;
    lmp_->bb_->slave_ = remote->bd_addr_;
    lmp_->bb_->change_state(Baseband::ROLE_SWITCH_MASTER);
}

void LMPLink::role_switch_tdd_failed()
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();

    if (lmp_->bb_->clk_ev_.uid_ > 0) {
	if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(stdout, "%d %f %s cancel clk\n", lmp_->bb_->bd_addr_,
		    now, __FUNCTION__);
	*/
	}
	s.cancel(&lmp_->bb_->clk_ev_);
    }

    if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(stdout, "%d %f %s\n", lmp_->bb_->bd_addr_, now,
		__FUNCTION__);
	*/
	lmp_->dump(stdout, 1);
    }

    if (piconet->isMaster()) {	// I was slave, changed to master, change back
	// piconet->master = remote;
	piconet->master_bd_addr_ = remote->bd_addr_;
	lmp_->bb_->setPiconetParam(piconet);
	lmp_->slaveStartClk(piconet);

    } else {			// Back to master Role.
	// piconet->master = lmp_->_my_info;
	piconet->master_bd_addr_ = lmp_->_my_info->bd_addr_;
	lmp_->bb_->setPiconetParam(piconet);
	lmp_->startMasterClk();
    }
    if (lmp_->trace_state()) {
	lmp_->dump(stdout, 1);
    }
    // lmp_->reqOutstanding = 0;
    // lmp_->tmpPico_ = 0;
    // if (lmp_->reqOutstanding && piconet->isMaster()) {       // try it again
    ++lmp_->RsFailCntr_;	// update slave's counter too.
    if (lmp_->disablePiconetSwitch_ && piconet->isMaster()
	&& lmp_->RsFailCntr_ < RS_REPEAT_COUNT) {	// try it again
	lmp_->HCI_Switch_Role(remote->bd_addr_, 0x01);
    }
}

// parked slaved need to be unparked first. after rs, park them again. 
// for an active slave:
// 1. add link, allocate txbuffer, tell bb switch to old parameter.
// 2. send slot_offset
// 3. send FHS
// 4. upon receiving ID, switch to new parameter, and send POLL
// 5. upon receving NULL, report to lmp.
void LMPLink::take_over_slave(Bd_info * slave)
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();
    clk_t clk;
    double t;

    if (lmp_->bb_->clk_ev_.uid_ > 0) {
	if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(stdout, "%d %s cancel clk\n", lmp_->bb_->bd_addr_,
		    __FUNCTION__);
	*/
	}
	s.cancel(&lmp_->bb_->clk_ev_);
    }
    t = lmp_->bb_->slotTime() * 2 - (now - lmp_->bb_->t_clkn_00_) +
	BT_CLKN_CLK_DIFF;
    clk = (lmp_->bb_->clkn_ & 0xFFFFFFFC) + 2;
    lmp_->bb_->clk_ = clk;

    s.schedule(&lmp_->bb_->clk_handler_, &lmp_->bb_->clk_ev_, t);
    piconet->compute_sched();
    lmp_->bb_->set_sched_word(piconet->_sched_word);

    SlotOffset req(slot_offset, lmp_->bb_->bd_addr_);
    lmp_->lmpCommand(LMP::LMP_SLOT_OFFSET, (uchar *) & req, sizeof(req), 8,
		     this);

    // schedue bb change to RS_MASTER
    s.schedule(&lmp_->_timer, (new
			       LMPEvent(this, LMPEvent::RSTakeOver)),
	       t + lmp_->bb_->slotTime() * 8);
}

void LMPLink::handle_role_switch_pico()
{
    // Scheduler & s = Scheduler::instance();
    // double now = s.clock();

    lmp_->dump(stdout, 1);
    lmp_->reqOutstanding = 0;

    if (piconet->isMaster()) {	// I was slave, switched to master Role.

	lt_addr_ = lmp_->bb_->slave_lt_addr_;
	remote->lt_addr_ = lt_addr_;
	piconet->master_bd_addr_ = lmp_->bb_->bd_addr_;

	if (lmp_->masterPico && lmp_->masterPico != piconet) {	// merge this link 
	    // if (lmp_->masterPico_old) {  // merge this link 
	    // When the master wake up??
	    // merge when the master wake up ??

	    // if (lmp_->bb_->bd_addr_ == 167) {
	    //      fprintf(stderr, "numPico() %d\n", lmp_->numPico());
	    // }
	    // Piconet *oldPico = piconet;

	    // lmp_->dump();
	    // piconet->dump();
	    lmp_->masterPico->dump();
	    lmp_->remove_piconet(piconet);
	    piconet = lmp_->masterPico;
	    lmp_->masterPico->add_slave(this);
	    lmp_->switchPiconet(lmp_->masterPico, 1);
	    // lmp_->switchPiconet(lmp_->masterPico, 0, lmp_->bb_->slotTime() - 126 * 1E-6 + BT_CLKN_CLK_DIFF);
	    // lmp_->unsuspend(lmp_->masterPico);
	    // lmp_->bb_->setPiconetParam(lmp_->masterPico);
	    if (lmp_->numPico() > 1) {
		/** Commented by Barun [07 March 2013]
		fprintf(stdout,
			"***warning: numPico() %d > 1 after merge.\n",
			lmp_->numPico());
		*/
		// abort();
	    }
	    lmp_->masterPico->dump();
	    // lmp_->dump();
	    // delete oldPico;

	} else if (lmp_->masterPico == piconet) {	// take over slaves

	} else {
	    lmp_->masterPico = piconet;	// add new Master piconet
	    // piconet->compute_sched();
	    // lmp_->bb_->set_sched_word(piconet->_sched_word);
	}

	if (piconet->old_slave) {
	    take_over_slave(piconet->old_slave);
	    piconet->numOldSlave--;
	    piconet->old_slave = piconet->old_slave->next_;
	} else {
	    lmp_->bb_->setPiconetParam(piconet);

	    // lmp_->link_setup(connhand);
	}

	piconet->setPicoChannel();

    } else {			// I was master, switched to slave Role.

	lt_addr_ = lmp_->bb_->lt_addr_;
	piconet->master_bd_addr_ = remote->bd_addr_;
	piconet->clk_offset = lmp_->bb_->clock_offset_;
	piconet->slot_offset = lmp_->bb_->slot_offset_;
	// piconet->slot_offset = slot_offset;

	if (0) {		// become bridge
	    // suspend current piconet.
	    // add piconet
	    lmp_->add_slave_piconet(this);
	    // schedule clk event.

	} else {
	    // set slot offset

	    // piconet->compute_sched();        // ???
	    lmp_->masterPico = NULL;
	    lmp_->bb_->setPiconetParam(piconet);
	    // set correct piconet parameters.
	    // reschedule clk event.

	    // send slave info to the new master
	}
    }
    txBuffer->reset_seqn();
    // txBuffer->_current = txBuffer->_next = NULL;

/*
    if (piconet->master->bd_addr_ != piconet->master_bd_addr_) {
	printf("%d m:%d mbd:%d\n", lmp_->bb_->bd_addr_,
	       piconet->master->bd_addr_, piconet->master_bd_addr_);
	abort();
    }
*/

    // if (!piconet->isMaster() && lmp_->node_->scatFormator_) {
    if (lmp_->node_->scatFormator_) {
	if (!piconet->isMaster()) {
	    lmp_->l2cap_->connection_ind(connhand);
	}
    }
    lmp_->bb_->inRS_ = 0;

    if (lmp_->trace_state()) {
	lmp_->dump(stdout, 1);
    }
}

//////////////////////////////////////////////////////////////
//                                                          //
//                             SCO                          //
//                                                          //
//////////////////////////////////////////////////////////////
void LMPLink::handle_sco_wakeup()
{
    Scheduler & s = Scheduler::instance();
    if (suspended == 0) {
	if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, BTPREFIX1 " ALREADY alive!!!\n");
	*/
	}
	return;
    } else {
	suspended = 0;
    }
    s.schedule(&lmp_->_timer, &piconet->_sco_ev,
	       T_poll_ * lmp_->bb_->slotTime());
    // Only active for 2 slots.
    s.schedule(&lmp_->_timer, &piconet->_sco_suspend_ev,
	       2 * lmp_->bb_->slotTime());
    lmp_->switchPiconet(piconet, 0, 0.0);
}

void LMPLink::handle_sco_suspend()
{
    lmp_->suspendCurPiconet();
    // need to restore Baseband state for inquiry/page/scan.
}

//////////////////////////////////////////////////////////////
//                                                          //
//                            SNIFF                         //
//                                                          //
//////////////////////////////////////////////////////////////
void LMPLink::handle_sniff_wakeup()
{
    Scheduler & s = Scheduler::instance();
    int tsniff = sniffreq->T_sniff;
    int dsniff = sniffreq->D_sniff;
    if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		BTPREFIX1
		"sniff: %d-%d at %f %d act rw %d %f skip:%d clk n %d o %d e"
		" %d c %d clkn_00 %f clk_00 %f.\n",
		lmp_->_my_info->bd_addr_, remote->bd_addr_, s.clock(),
		int (s.clock() / lmp_->bb_->slotTime()) %
		lmp_->defaultTSniff_, sniffreq->sniff_attempt,
		sniffreq->sniff_attempt * lmp_->bb_->slotTime(), skip_,
		lmp_->bb_->clkn_, piconet->clk_offset,
		lmp_->bb_->clkn_ + piconet->clk_offset, lmp_->bb_->clk_,
		lmp_->bb_->t_clkn_00_, lmp_->bb_->t_clk_00_);
	*/
	lmp_->dump(BtStat::log_, 1);
    }

#if 0
    if (lmp_->bb_->in_transmit_ > 2 && (lmp_->bb_->activeBuffer_
					&& lmp_->bb_->activeBuffer_->link()
					&& lmp_->bb_->activeBuffer_->
					link()->piconet != piconet)) {
#ifdef SNIFF_ADJ
	s.schedule(&lmp_->_timer, _sniff_ev, (lmp_->bb_->in_transmit_ - 2)
		   * lmp_->bb_->tick());
	delayedT += (lmp_->bb_->in_transmit_ - 2) * lmp_->bb_->tick();
	return;
#else
	// cancel TX
	lmp_->bb_->activeBuffer_->pauseCurrentPkt();
#endif
    } else if (lmp_->bb_->in_receiving_ > 2 && (lmp_->bb_->activeBuffer_
						&& lmp_->bb_->
						activeBuffer_->link()
						&& lmp_->bb_->
						activeBuffer_->link()->
						piconet != piconet)) {
#ifdef SNIFF_ADJ
	s.schedule(&lmp_->_timer, _sniff_ev, (lmp_->bb_->in_receiving_ - 2)
		   * lmp_->bb_->tick());
	delayedT += (lmp_->bb_->in_receiving_ - 2) * lmp_->bb_->tick();
	return;
#else
/*
	if (lmp_->bb_->_rxPkt && lmp_->bb_->_rxPkt->uid_ > 0) {
	    s.cancel(lmp_->bb_->_rxPkt);
	    Packet::free(lmp_->bb_->_rxPkt);
	}
*/
#endif
    }
#endif

    // add up active time needed for channel synchronization
    if (!piconet->isMaster() && !LMP::useReSyn_) {
	double syntime = (Random::integer(1000) * 1E-6 *
			  sniffreq->T_sniff * lmp_->bb_->slotTime());
	lmp_->bb_->energyRec_.energy_ -= 
		(syntime * lmp_->bb_->energyRec_.activeEnrgConRate_);
	lmp_->bb_->energyRec_.activeTime_ += syntime;
    }
    // schedule next wakeup
    int skip = skip_;
    skip_ = 0;
    if (_in_sniff) {
	_in_sniff_attempt = 1;
	// force other link in sniff to suspend
	if (lmp_->curPico && lmp_->curPico != piconet
	    && lmp_->curPico->activeLink) {
	    if (lmp_->curPico->activeLink->_in_sniff) {

		// in case that a bridge has more than 2 roles, 
		// it may skip this cycle.
		if (lmp_->numPico() > 2 &&
		    lmp_->rpScheduler && lmp_->rpScheduler->skip(this)) {
		    if (lmp_->trace_state()) {
			/** Commented by Barun [07 March 2013]
			fprintf(BtStat::log_,
				BTPREFIX1 " ** SKIP this Cycle.\n");
			*/
		    }
		    skip = 1;

		} else {

		    lmp_->_pending_act = LMP::NoAct;
		    if (lmp_->curPico->activeLink->_sniff_ev_to->uid_ > 0) {
			s.cancel(lmp_->curPico->activeLink->_sniff_ev_to);
		    }
		    if (lmp_->trace_state()) {
			/** Commented by Barun [07 March 2013]
			fprintf(BtStat::log_, BTPREFIX1
				"%d suspends active Link in other picos\n",
				lmp_->bb_->bd_addr_);
			*/
		    }
		    lmp_->curPico->activeLink->handle_sniff_suspend();
		}
	    } else {
		skip = 1;
	    }
	}
#if 1
	if (_sniff_ev->uid_ > 0) {
	    s.cancel(_sniff_ev);
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1 "FIXME: %s: _sniff_ev->uid_ > 0\n",
		    __FUNCTION__);
	*/
	}
#endif

	double clkdrft = 0;
	if (LMP::useReSyn_ && !piconet->isMaster()) {
	    clkdrft = (lmp_->lowDutyCycle_ ? CLKDRFT_SLEEP : CLKDRFT);
	}

	s.schedule(&lmp_->_timer, _sniff_ev,
		   sniffreq->T_sniff * lmp_->bb_->slotTime() * (1 -
							      clkdrft) -
		   delayedT);

	if (lmp_->disablePiconetSwitch_ || skip || lmp_->reqOutstanding || lmp_->tmpPico_) {	// skip this cycle
	    delayedT = 0;
	    if (lmp_->trace_state()) {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_,
			"skip this cycle because skip:%d req:%d tmp:%d\n",
			skip, lmp_->reqOutstanding, lmp_->tmpPico_);
		*/
	    }
	    return;
	} else if (lmp_->bb_->isBusy()) {
	    lmp_->_bb_cancel();
	}
	// Should this timer be re-checked at LMP::sendUp() ?
	if (_sniff_ev_to->uid_ > 0) {
	    s.cancel(_sniff_ev_to);
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1 "FIXME: %s: _sniff_ev_to->uid_ > 0\n",
		    __FUNCTION__);
	*/
	}
	s.schedule(&lmp_->_timer, _sniff_ev_to,
		   sniffreq->sniff_attempt * lmp_->bb_->slotTime() -
		   delayedT);
	delayedT = 0;
    } else {
	clear_sniff_ev();
	needAdjustSniffAttempt = 0;
	delete sniffreq;
	sniffreq = NULL;
    }

    if (!_sniff_started) {	// first time
	_sniff_started = 1;
	return;
    }

    if (suspended == 0) {
	//abort();
	if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, BTPREFIX1 " ALREADY alive!!!\n");
	*/
	}
	return;
    } else {
	suspended = 0;
    }

    piconet->unsuspend_link(this);
    lmp_->wakeupClk_ = 0;
    // if (piconet != lmp_->curPico) {
    if (piconet->numActiveLink == 1) {
	piconet->cancel_schedule_SCO();
	if (!piconet->isMaster()) {
	    int numSniff =
		(lmp_->bb_->clkn_ - _lastSynClk) / tsniff / 2 + 1;
	    lmp_->bb_->resyncWind_ =
		(lmp_->lowDutyCycle_ ? CLKDRFT_SLEEP : CLKDRFT) * tsniff *
		numSniff * 2 * lmp_->bb_->slotTime();
	    if (lmp_->bb_->resyncWind_ < MAX_SLOT_DRIFT) {
		lmp_->bb_->resyncWind_ = MAX_SLOT_DRIFT;
	    }
	    lmp_->bb_->resyncWindSlotNum_ =
		int (lmp_->bb_->resyncWind_ / 1.25E-3) * 2 + 2;
	    int clk =
		(lmp_->bb_->clkn_ & 0xfffffffc) + piconet->clk_offset;
	    lmp_->wakeupClk_ = ((clk / 2 / tsniff) * tsniff + dsniff) * 2;
	    if (lmp_->wakeupClk_ < clk) {
		lmp_->wakeupClk_ += tsniff * 2;
	    }
	    if (lmp_->wakeupClk_ % 4) {
		fprintf(stderr, "DSniff %d is not even ? \n", dsniff);
		abort();
	    }
	    if (!LMP::useReSyn_) {
		_lastSynClk = lmp_->wakeupClk_ - 4;
	    }
	}
	lmp_->switchPiconet(piconet, 0, 0.0);
	if (piconet != lmp_->curPico) {
	    fprintf(stderr,
		    "OOps, %d problems with switchPiconet. Force curPico.\n",
		    lmp_->bb_->bd_addr_);
	    lmp_->curPico = piconet;
	    lmp_->curPico->suspended = 0;
	}
    } else {
	lmp_->schedule_set_schedWord(this);
    }
    txBuffer->session_reset();
    if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	printf("  -- clk: %d e: %d n: %d clkoffset %d t_clkn_00 %f\n",
	       lmp_->bb_->clk_, lmp_->wakeupClk_, lmp_->bb_->clkn_,
	       piconet->clk_offset, lmp_->bb_->t_clkn_00_);
	*/
    }

    if (needAdjustSniffAttempt) {
	adjustSniffAttempt();
    }
}

// Check if this link is in sleep or is about to sleep within 2 slots.
// This check is necessary because of the inaccuracy of the bounndary
// of piconet switch.
bool LMPLink::isAboutToSleep()
{
    if (suspended) {
	return true;
    }
    Scheduler & s = Scheduler::instance();
    if (_sniff_ev_to->uid_ > 0 && _sniff_ev_to->time_ - s.clock() <
	lmp_->bb_->slotTime() * 2) {
	return true;
    }
    return false;
}

// When to sleep again?
// This may enable the node to sleep before N_sniff_attempt if
// there is not enough packet to send/receive.
void LMPLink::recvd_in_sniff_attempt(Packet * p)
{
#if 0
    hdr_bt *bh = HDR_BT(p);
    Scheduler & s = Scheduler::instance();
    s.cancel(_sniff_ev_to);
    if (_in_sniff) {
	double t = MAX(sniffreq->sniff_timeout,
		       hdr_bt::slot_num(bh->type)) * BTslotTime();
	s.schedule(&lmp_->_timer, _sniff_ev_to, t);
    }
#endif
}

void LMPLink::handle_sniff_suspend()
{
    Scheduler & s = Scheduler::instance();

    s.cancel(_sniff_ev_to);	// may from a different path so cancel 2nd sleep.

#if 0
    if (lmp_->bb_->in_transmit_ > 2 && lmp_->bb_->activeBuffer_ == txBuffer) {
#ifdef SNIFF_ADJ
	s.schedule(&lmp_->_timer, _sniff_ev_to,
		   (lmp_->bb_->in_transmit_ - 2)
		   * lmp_->bb_->tick());
	// delayedT += (lmp_->bb_->in_transmit_ - 2) * lmp_->bb_->tick();
	delayedT = 0;
	return;
#else
	txBuffer->pauseCurrentPkt();
#endif
    } else if (lmp_->bb_->in_receiving_ > 2
	       && lmp_->bb_->activeBuffer_ == txBuffer) {
#ifdef SNIFF_ADJ
	s.schedule(&lmp_->_timer, _sniff_ev_to,
		   (lmp_->bb_->in_receiving_ - 2)
		   * lmp_->bb_->tick());
	// delayedT += (lmp_->bb_->in_receiving_ - 2) * lmp_->bb_->tick();
	delayedT = 0;
	return;
#else
/*
	if (lmp_->bb_->_rxPkt && lmp_->bb_->_rxPkt->uid_ > 0) {
	    s.cancel(lmp_->bb_->_rxPkt);
	    Packet::free(lmp_->bb_->_rxPkt);
	}
*/
#endif
    }
#endif

    // a hook for MultiRoleDRP
    if (lmp_->rpScheduler
	&& lmp_->rpScheduler->type() == RPSched::MRDRP
	&& ((MultiRoleDRP *) lmp_->rpScheduler)->_activeLink == this) {
	((MultiRoleDRP *) lmp_->rpScheduler)->_activeLink = NULL;
    }

    delayedT = 0;
    if (suspended) {
	if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1
		    "%s: %d-%d has been suspended already. %f \n",
		    __FUNCTION__, lmp_->_my_info->bd_addr_, remote->bd_addr_,
		    s.clock());
	*/
	}
	return;
    }

    if (_in_sniff) {		// switch off current link
	if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1 "sniff: %d-%d at %f %d absent rw %d %f.\n",
		    lmp_->_my_info->bd_addr_, remote->bd_addr_, s.clock(),
		    int (s.clock() / lmp_->bb_->slotTime()) %
		    lmp_->defaultTSniff_, sniffreq->sniff_attempt,
		    sniffreq->sniff_attempt * lmp_->bb_->slotTime());
	*/
	}
	suspended = 1;
	_in_sniff_attempt = 0;
	_sniff_started = 1;
	piconet->suspend_link(this);

	if (lmp_->disablePiconetSwitch_) {
	    return;
	}

	if (piconet->numActiveLink == 0 && piconet == lmp_->curPico) {
	    if (piconet->numScoLink == 0) {
		lmp_->suspendCurPiconet();
		if (lmp_->rpScheduler
		    && lmp_->rpScheduler->type() == RPSched::LPDRP) {
		    lmp_->bb_->suspendClkn_ = 1;
		}
	    } else {
		piconet->schedule_SCO();
	    }

	} else {
	    lmp_->schedule_set_schedWord(this);
	}
    } else {			// SNIFF process is terminated.
	clear_sniff_ev();
	needAdjustSniffAttempt = 0;
	delete sniffreq;
	sniffreq = NULL;
    }
}

void LMPLink::clear_sniff_ev()
{
    Scheduler & s = Scheduler::instance();

    if (_sniff_ev) {
	s.cancel(_sniff_ev);
	delete _sniff_ev;
    }

    if (_sniff_ev_to) {
	s.cancel(_sniff_ev_to);
	delete _sniff_ev_to;
    }

    _sniff_ev = NULL;
    _sniff_ev_to = NULL;
}

void LMPLink::schedule_sniff()
{
    lmp_->idleSchred_ = 9999999;	// disable idleSchred check
    Scheduler & s = Scheduler::instance();

    clear_sniff_ev();
    _sniff_ev = new LMPEvent(this, LMPEvent::SniffWakeup);
    _sniff_ev_to = new LMPEvent(this, LMPEvent::SniffSuspend);
    _in_sniff = 1;
    _in_sniff_attempt = 0;
    _sniff_started = 0;
    suspended = 0;
    txBuffer->setPriority(PRIO_BR);

    Tsniff = sniffreq->T_sniff;
    Dsniff = sniffreq->D_sniff;
    Nattempt = sniffreq->sniff_attempt;
    Ntimeout = sniffreq->sniff_timeout;

    _lastSynClk = lmp_->bb_->clkn_;

    // If this is the first Sniff Link, set RP for the piconet.
    if (piconet->RP < 0) {
	piconet->RP = sniffreq->D_sniff;
	if (piconet->isMaster()) {
	    lmp_->bb_->linkSched_->reset();
	}
    }

    double t = compute_sniff_instance();

    double clkdrft = 0;
    if (LMP::useReSyn_ && !piconet->isMaster()) {
	clkdrft = (lmp_->lowDutyCycle_ ? CLKDRFT_SLEEP : CLKDRFT);
    }

    s.schedule(&lmp_->_timer, _sniff_ev, (t - s.clock()) * (1 - clkdrft));
}

int LMPLink::absclk(int16_t t)
{
    if (t < 0) {
	return t;
    }
    int clkdiff = 0;

    if (lmp_->curPico) {
	clkdiff = piconet->clk_offset - lmp_->curPico->clk_offset;
	clkdiff /= 2;
	clkdiff %= lmp_->defaultTSniff_;
	if (clkdiff < 0) {
	    clkdiff += lmp_->defaultTSniff_;
	}
    }
    uint16_t T_sniff = lmp_->defaultTSniff_;
    int sniffoffset = (((lmp_->bb_->clk_ + clkdiff) >> 2) << 1) % T_sniff;
    double sniffinst =
	lmp_->bb_->t_clk_00_ - sniffoffset * lmp_->bb_->slotTime();

    return (int (sniffinst / lmp_->bb_->slotTime()) + t) %T_sniff;
}

// receiver has the reponsibility to update RP in another piconet.
void LMPLink::update_dsniff(int16_t ds)
{
    int tsniff = sniffreq->T_sniff;
    // uint16_t sniff_attempt = lmp_->defaultTSniff_ / 2;
    uint16_t sniff_attempt = tsniff / 2;
#ifdef ADJSATT
    sniff_attempt = (sniff_attempt - 1) & 0xfffffffe;
#endif
    update_dsniff(ds, sniff_attempt, true);
}

void LMPLink::update_dsniff(int16_t ds, uint16_t sniff_attempt,
			    bool chooseDs)
{
    // uint16_t T_sniff = lmp_->defaultTSniff_;
    int T_sniff = sniffreq->T_sniff;
    int D_sniff = ds;
    // uint16_t sniff_attempt = T_sniff / 2;
    uint16_t sniff_timeout = lmp_->defaultSniffTimeout_;

    if (lmp_->numPico() > 1) {	//M/S BR, one piconet has only one RP.
	chooseDs = false;
    }

/*
    if (chooseDs && (sniffreq->D_sniff - ds > T_sniff / 2
		     || sniffreq->D_sniff - ds < -T_sniff / 2)) {
*/
    if (chooseDs) {
	int dist = sniffreq->D_sniff - ds;
	if (dist < 0) {
	    dist = -dist;
	}
	if (dist > T_sniff / 4 && dist < T_sniff * 3 / 4) {
	    D_sniff = (D_sniff + T_sniff / 2) % T_sniff;
	}
	// D_sniff = (D_sniff + T_sniff / 2) % T_sniff;
    }
    if (D_sniff != sniffreq->D_sniff ||
	sniffreq->sniff_attempt != sniff_attempt) {
	// sniffreq->sniff_attempt < sniff_attempt) {
	uchar flags = 0;

	int sniffoffset =
	    (((lmp_->bb_->clk_ + piconet->clk_offset -
	       lmp_->curPico->clk_offset) >> 2) << 1) % T_sniff;
	double sniffinst =
	    lmp_->bb_->t_clk_00_ - sniffoffset * lmp_->bb_->slotTime();
	if (lmp_->trace_state()) {
	    printf("%s new ds inst %d\n", __FUNCTION__,
		   (int (sniffinst / lmp_->bb_->slotTime()) +
		    D_sniff) %T_sniff);
	}

	if (sniffreq) {
	    delete sniffreq;
	}
	sniffreq =
	    new SniffReq(flags, D_sniff, T_sniff,
			 sniff_attempt, sniff_timeout);
	// sniffreq->setFlagTerm();
	if (chooseDs) {
	    // sniffreq->setFlagAtt();
	} else {
	    sniffreq->setFlagTerm();
	}

	lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) sniffreq,
			 sizeof(SniffReq), 9, this);
    }
}

void LMPLink::update_dsniff_forTree(int16_t ds, bd_addr_t root)
{
    uint16_t T_sniff = lmp_->defaultTSniff_;
    int D_sniff = ds;
    uint16_t sniff_attempt = T_sniff / 2;
    uint16_t sniff_timeout = lmp_->defaultSniffTimeout_;
    uchar flags = 0;

    if (sniffreq) {
	delete sniffreq;
    }
    sniffreq =
	new SniffReq(flags, D_sniff, T_sniff,
		     sniff_attempt, sniff_timeout);
    sniffreq->setFlagTerm();
    sniffreq->setFlagTree();
    sniffreq->root = root;

    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) sniffreq,
		     sizeof(SniffReq), 15, this);
}

void LMPLink::adjustSniffAttempt(uint16_t attmpt)
{
    needAdjustSniffAttempt = 0;
    sniffreq->sniff_attempt = attmpt;
    sniffreq->clearFlagAll();
    sniffreq->setFlagTerm();

    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) sniffreq,
		     sizeof(SniffReq), 9, this);
}

void LMPLink::adjustSniffAttempt()
{
#if 0
    if (lmp_->bb_->bd_addr_ == 9) {
	fprintf(stderr, "JJJJJ\n");
    }
#endif
    needAdjustSniffAttempt = 0;

    // int sniff_attempt = lmp_->defaultTSniff_ / 2;
    int sniff_attempt = sniffreq->T_sniff / 2;
#ifdef ADJSATT
    if (sniff_attempt == req->T_sniff / 2) {
	sniff_attempt = (sniff_attempt - 1) & 0xfffffffe;
    }
#endif

    BrReq brreq;
    lmp_->lookupRP(&brreq);
    brreq.sort();
    int myind = brreq.lookup(sniffreq->D_sniff);
    uint16_t nextDs;
    if (myind < 0) {
	return;
    } else if (myind == brreq.len - 1) {
	nextDs = brreq.dsniff[0] + sniffreq->T_sniff;
    } else {
	nextDs = brreq.dsniff[myind + 1];
    }

#if 0
    if (brreq.len != 2 || piconet->isMaster()) {
	return;
    }
#endif

    // assert(brreq.dsniff[0] == sniffreq->D_sniff);

    // int D_sniff = sniffreq->D_sniff;
    int T_sniff = sniffreq->T_sniff;

    // adjust sniff_attempt
    // sniff_attempt = (nextDs - sniffreq->D_sniff) / 2;
    sniff_attempt = (nextDs - sniffreq->D_sniff);
    if (sniff_attempt > T_sniff / 2) {
	sniff_attempt = T_sniff / 2;
    }
#ifdef ADJSATT
    if (sniff_attempt == req->T_sniff / 2) {
	sniff_attempt = (sniff_attempt - 1) & 0xfffffffe;
    }
#endif

    if (sniff_attempt != sniffreq->sniff_attempt) {
	printf("%d %s att old %d new %d \n",
	       lmp_->bb_->bd_addr_, __FUNCTION__,
	       sniffreq->sniff_attempt, sniff_attempt);
	// should we change it right now, or after the negotiation ???
	sniffreq->sniff_attempt = sniff_attempt;
	sniffreq->clearFlagAll();
	sniffreq->setFlagTerm();

#if 1
	if (sniff_attempt < T_sniff / 4) {
	    fprintf(stderr, "%s sniff_attempt %d is too small.\n",
		    __FUNCTION__, sniff_attempt);
	    abort();
#if 0
	    lmp_->rpScheduler->start(this);
	    return;
#endif
	}
#endif

	lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) sniffreq,
			 sizeof(SniffReq), 9, this);
    }

}

double LMPLink::compute_sniff_instance()
{
    Scheduler & s = Scheduler::instance();

    // do we need wait 6 T_poll here?? 
    uint16_t nT_poll = 0;
    uint32_t schedPointClk = lmp_->bb_->clk_ + T_poll_ * nT_poll * 2;

    // at sched point, offset within superframe T_sniff
    int offset = (schedPointClk >> 1) % sniffreq->T_sniff;

    int waitSlot = sniffreq->D_sniff - offset;
    if (waitSlot <= 0) {
	waitSlot += sniffreq->T_sniff;
    }

    double t = lmp_->bb_->t_clk_00_ +
	(schedPointClk + waitSlot * 2 - (lmp_->bb_->clk_ & 0xfffffffc))
	* lmp_->bb_->tick();
    if (t < s.clock()) {
	t += sniffreq->T_sniff * lmp_->bb_->slotTime();
    }
    // t += sniffreq->T_sniff * lmp_->bb_->slotTime();
    t += MAX(lmp_->defaultTSniff_,
	     sniffreq->T_sniff) * lmp_->bb_->slotTime();
    // t -= 4E-6;

    if (lmp_->trace_state()) {
	printf
	    ("LMPLink::schedule_sniff(): %d-%d %f(%d:%d) @ %f T_: %d D_: %d \n",
	     lmp_->_my_info->bd_addr_, remote->bd_addr_, s.clock(),
	     lmp_->bb_->clk_, lmp_->bb_->clkn_, t, sniffreq->T_sniff,
	     sniffreq->D_sniff);
    }
    return t;
}

void LMPLink::recvd_sniff_req(LMPLink::SniffReq * req)
{
    if (lmp_->rpScheduler && lmp_->rpScheduler->type() !=
	RPSched::MDRP
	&& lmp_->rpScheduler->type() != RPSched::LPDRP
	&& req->sniff_attempt < req->T_sniff / 4) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		"%d->%d sniff_attempt: %d (%d) is too small.\n",
		remote->bd_addr_, lmp_->bb_->bd_addr_,
		req->sniff_attempt, req->T_sniff);
	*/
	// abort();
    }
    // clear_sniff_ev();
    if (sniffreq) {
	delete sniffreq;
    }
    sniffreq = new LMPLink::SniffReq(*req);
    sniffreq->clearFlagAll();
    if (piconet->isMaster()) {
	lmp_->bb_->linkSched_->reset();
    }
    // Negotiating better sniff parameters.
    if (!req->flagTerm()) {

	// Check if sniff_attempt is ok
	// if (lmp_->curPico->isMaster()) {
	if (lmp_->curPico->isMaster() &&
	    (!lmp_->rpScheduler
	     || lmp_->rpScheduler->type() == RPSched::MDRP)) {
	    BrReq brreq;
	    lmp_->lookupRP(&brreq);
	    brreq.sort();
	    int myind = brreq.lookup(req->D_sniff);
	    uint16_t nextDs;
	    if (myind >= 0) {
		if (myind == brreq.len - 1) {
		    nextDs = brreq.dsniff[0] + req->T_sniff;
		} else {
		    nextDs = brreq.dsniff[myind + 1];
		}
		uint16_t sniff_attempt = (nextDs - req->D_sniff);
		if (sniff_attempt > req->T_sniff / 2) {
		    sniff_attempt = req->T_sniff / 2;
		}
#ifdef ADJSATT
		if (sniff_attempt == req->T_sniff / 2) {
		    sniff_attempt = (sniff_attempt - 1) & 0xfffffffe;
		}
#endif


		if (sniff_attempt < req->sniff_attempt) {
		    sniffreq->sniff_attempt = sniff_attempt;
		    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ,
				     (uchar *) sniffreq, sizeof(SniffReq),
				     9, this);
		    return;
		}
	    }
	}
	// A BR renegotiate Sniff Param with the other master.
	if (!lmp_->curPico->isMaster() && lmp_->suspendPico) {
	    LMPLink *al = (lmp_->suspendPico->suspendLink ?
			   lmp_->suspendPico->suspendLink :
			   lmp_->suspendPico->activeLink);
	    if (!al) {
		fprintf(stderr, "*** %d %f %s: suspendPico has no link,"
			" numPico(): %d\n",
			lmp_->bb_->bd_addr_,
			Scheduler::instance().clock(),
			__FUNCTION__, lmp_->numPico());
	    }
	    if (lmp_->rpScheduler && al) {
		lmp_->rpScheduler->renegotiateRP(al);
	    }
	}
    }

    lmp_->lmpCommand(LMP::LMP_ACCEPTED, LMP::LMP_SNIFF_REQ, this);
    schedule_sniff();

    // MDRP set this if the RW of the BR need to be adjusted.
    if (req->rpAlgm == RPSched::MDRP) {
	int attmpt = req->D_sniff - req->affectedDs;
	if (attmpt < 0) {
	    attmpt += lmp_->defaultTSniff_;
	}
	if (attmpt > lmp_->defaultTSniff_ / 2) {
	    fprintf(stderr, "MDRP: attmpt :%d > defaultTSniff_ / 2 \n",
		    attmpt);
	    abort();
	}
	if (!lmp_->suspendPico) {
	    return;
	}
	LMPLink *sl = (lmp_->suspendPico->suspendLink ?
		       lmp_->suspendPico->suspendLink :
		       lmp_->suspendPico->activeLink);
	sl->adjustSniffAttempt(attmpt);
	return;
    }
    // A ugly hack for roles >=3
    // correct behavior should be adjust a specific RW against this link.
    if (req->flagAtt() && lmp_->numPico() < 3) {
	lmp_->setNeedAdjustSniffAttempt(this);
    }

    if (req->flagTree()) {	// Tree adjustment
	// if (req->rpAlgm == RPSched::TDRP) {
	((TreeDRP *) lmp_->rpScheduler)->adjust(this, req->D_sniff,
						req->root);
	// }
    }

    if (lmp_->rpScheduler) {
	lmp_->rpScheduler->postprocessRPsched(this);
    }
}

void LMPLink::request_sniff()
{
    if (!sniffreq) {
	uchar flags = 0;
	uint16_t T_sniff = (sniff_Max_Interval + sniff_Min_Interval) / 2;
	uint16_t D_sniff = (lmp_->bb_->clk_ + T_poll_ * 6) % T_sniff;
	uint16_t sniff_attempt = sniff_Attempt;
	uint16_t sniff_timeout = sniff_Timeout;

	sniffreq =
	    new SniffReq(flags, D_sniff, T_sniff, sniff_attempt,
			 sniff_timeout);
	if (piconet->isMaster()) {
	    lmp_->bb_->linkSched_->reset();
	}
    }

    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) sniffreq,
		     sizeof(SniffReq), 9, this);
}

void LMPLink::handle_unsniff()
{
    _in_sniff = 0;
    txBuffer->setPriority(PRIO_PANU);
    // delete sniffreq;
    // sniffreq = NULL;
}

void LMPLink::request_unsniff()
{
    lmp_->lmpCommand(LMP::LMP_UNSNIFF_REQ, this);
    if (piconet->isMaster()) {
	handle_unsniff();
    }
}

#ifdef PARK_STATE
//////////////////////////////////////////////////////////////
//                                                          //
//                            PARK                          //
//                                                          //
//////////////////////////////////////////////////////////////
void LMPLink::handle_park()
{
    // Scheduler & s = Scheduler::instance();
    if (suspended) {		// wake up to listen to beacons.
	suspended = 0;
    }
}

// Master
// or use a schedWord to handle it ??
void LMPLink::sendBeacon()
{
    Scheduler & s = Scheduler::instance();
    s.schedule(&lmp_->_timer, _park_ev,
	       parkreq->T_B * lmp_->bb_->slotTime());
    // check for unpark request. if so send bcast packet.
    // push NULL if no packet.
    //
    // if unpark sent, Schedule (NewConnTO) to check if unpack successful
    // to readjust the unpark list.
}

// Slave
// at each wakeup instant, the piconet should switch back, and the slave
// try to synchronize to the master.
void LMPLink::wakeUpForBeacon()
{
    Scheduler & s = Scheduler::instance();
    s.schedule(&lmp_->_timer, _park_ev, parkreq->T_B * parkreq->N_Bsleep
	       * lmp_->bb_->slotTime());
    if (piconet != lmp_->curPico) {
	lmp_->switchPiconet(piconet);
    }
    if (_unpark) {
	// should schedule when to change the state.
	// This state will sending id packet at predefined slot.
	lmp_->bb_->change_state(Baseband::UNPARK_SLAVE);
    } else {
	lmp_->bb_->change_state(Baseband::PARK_SLAVE);
    }
}

void LMPLink::enter_park()
{
    Scheduler & s = Scheduler::instance();
    suspended = 1;		// ??
    if (piconet->isMaster()) {
	// schedule beacon broadcasting
	// reclaim slave's lt_addr
	// how to handle the ACL link ??
	piconet->lt_table[lt_addr_] = NULL;
    } else {
	// schedule wake up at beacon instant.
	int ds =
	    parkreq->D_B + parkreq->T_B * parkreq->D_Bsleep -
	    lmp_->bb_->clk_ % (parkreq->T_B * parkreq->N_Bsleep);
	if (ds < 0) {
	    ds += parkreq->T_B * parkreq->N_Bsleep;
	}
	s.schedule(&lmp_->_timer, _park_ev, ds * lmp_->bb_->slotTime());
	lmp_->suspendCurPiconet();
	// lmp_->handle_pending_act(); //???
    }
}

void LMPLink::recvd_park_req(LMPLink::ParkReq * req)
{
    if (piconet->isMaster()) {
	parkreq = new ParkReq(*req);
	parkreq->pm_addr = get_pm_addr();
	parkreq->ar_addr = get_ar_addr();
	lmp_->lmpCommand(LMP::LMP_PARK_REQ, (uchar *) req,
			 sizeof(ParkReq), 16, this);
    } else {
	lmp_->lmpCommand(LMP::LMP_ACCEPTED, LMP::LMP_PARK_REQ, this);
	parkreq = new ParkReq(*req);
	schedule_park();
    }

}
void LMPLink::schedule_park()
{
    _park_ev_en = new LMPEvent(this, LMPEvent::EnterPark);
    _park_ev = new LMPEvent(this, LMPEvent::Park);
    _parked = 1;
    _unpark = 0;
    // schedule entering park state
    Scheduler::instance().schedule(&lmp_->_timer, _park_ev_en,
				   T_poll_ * 6 * lmp_->bb_->slotTime());
}

uchar LMPLink::get_pm_addr()
{
    if (piconet->isMaster()) {
	return piconet->allocPMaddr(remote->bd_addr_);
    } else {
	return 0;
    }
}

uchar LMPLink::get_ar_addr()
{
    if (piconet->isMaster()) {
	return piconet->allocARaddr(remote->bd_addr_);
    } else {
	return 0;
    }
}

void LMPLink::request_park()
{
    uchar flags = 0;
    uint16_t D_B = 0;		// offset of beacon instant
    uint16_t T_B = (beacon_Max_Interval + beacon_Min_Interval) / 2;
    uchar N_B = 3;		// Beacon slot repete N_B times
    uchar Det_B = 2;		// beacon slots in the train seperated by Det_B
    uchar pm_addr = get_pm_addr();
    uchar ar_addr = get_ar_addr();
    uchar N_Bsleep = 1;		// slave sleep T_B * N_Bsleep slots
    uchar D_Bsleep = 0;		// D_Bsleep'th N_Bsleep, it wakes up once.
    uchar D_access = 16;	// offset of beginning access window
    uchar T_access = 16;	// access window width
    uchar N_acc_slots;
    uchar N_poll = 40;		// slave's Poll interval
    uchar M_access = 8;		// number of access windows
    uchar access_scheme = 0;	// only POLL is defined in specs

    parkreq = new ParkReq(flags, D_B, T_B, N_B, Det_B, pm_addr,
			  ar_addr, N_Bsleep, D_Bsleep, D_access, T_access,
			  N_acc_slots, N_poll, M_access, access_scheme);

    lmp_->lmpCommand(LMP::LMP_PARK_REQ, (uchar *) parkreq,
		     sizeof(ParkReq), 16, this);
}

void LMPLink::request_unpark()
{
    if (piconet->isMaster()) {
	// piconet->add_unpack_member(remote);

    } else if (pm_addr != 0) {

	_unpark = 1;
	if (lmp_->bb_->state() == Baseband::PARK_SLAVE) {
	    lmp_->bb_->change_state(Baseband::UNPARK_SLAVE);
	}
    }
}
#endif

//////////////////////////////////////////////////////////////
//                                                          //
//                            HOLD                          //
//                                                          //
//////////////////////////////////////////////////////////////
// invoked by LMPTimer at scheduled hold instant or wake up time.
void LMPLink::handle_hold_wakeup()
{
    Scheduler & s = Scheduler::instance();
    if (!suspended) {
	if (lmp_->trace_state()) {
	    printf
		("LMPLink::handle_hold(): %d-%d at %f wakeup. -- already wake\n",
		 lmp_->_my_info->bd_addr_, remote->bd_addr_, s.clock());
	}
	_on_hold = 0;
	clear_hold_ev();
	delete holdreq;
	holdreq = NULL;
	return;
    }
    if (lmp_->trace_state()) {
	printf("LMPLink::handle_hold(): %d-%d at %f wakeup hi:%d ht:%d.\n",
	       lmp_->_my_info->bd_addr_, remote->bd_addr_, s.clock(),
	       holdreq->hold_instant, holdreq->hold_time);
    }
    if (lmp_->bb_->isBusy() ||
	(lmp_->curPico && lmp_->curPico != piconet
	 && lmp_->curPico->activeLink &&
	 // !lmp_->curPico->activeLink->connhand->ready_)) {
	 !lmp_->curPico->activeLink->_in_sniff
	 // )) {
	 && lmp_->rpScheduler && lmp_->rpScheduler->sniffBased())) {
	s.schedule(&lmp_->_timer, _hold_ev_wakeup,
		   100 * lmp_->bb_->slotTime());
	holdreq->hold_time += 100;
	if (lmp_->trace_state()) {
	    printf("-- bb_ is busy. wait for 100 slots.\n");
	}
	return;
    }
    suspended = 0;
    _on_hold = 0;
    clear_hold_ev();

    int hldt = holdreq->hold_time;
    int hldi = holdreq->hold_instant;

    // force other link in sniff to suspend
    if (lmp_->curPico && lmp_->curPico != piconet
	&& lmp_->curPico->activeLink
	&& lmp_->curPico->activeLink->_in_sniff) {
	lmp_->_pending_act = LMP::NoAct;
	if (lmp_->curPico->activeLink->_sniff_ev_to->uid_ > 0) {
	    s.cancel(lmp_->curPico->activeLink->_sniff_ev_to);
	}
	lmp_->curPico->activeLink->handle_sniff_suspend();
    }
    piconet->unsuspend_link(this);

    lmp_->wakeupClk_ = 0;
    if (piconet->numActiveLink == 1) {
	piconet->cancel_schedule_SCO();
	if (!piconet->isMaster()) {
	    lmp_->bb_->resyncWind_ =
		(lmp_->lowDutyCycle_ ? CLKDRFT_SLEEP : CLKDRFT) * hldt *
		lmp_->bb_->slotTime() * 2;
	    if (lmp_->bb_->resyncWind_ < MAX_SLOT_DRIFT) {
		lmp_->bb_->resyncWind_ = MAX_SLOT_DRIFT;
	    }
	    lmp_->bb_->resyncWindSlotNum_ =
		int (lmp_->bb_->resyncWind_ / 1.25E-3) * 2 + 2;
	    lmp_->wakeupClk_ = (hldi + hldt) * 2;
	}
	lmp_->switchPiconet(piconet, 0, 0.0);
    } else {
	lmp_->schedule_set_schedWord(this);
    }
    txBuffer->session_reset();
    if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	printf("  -- clk: %d e: %d n: %d clkoffset %d\n", lmp_->bb_->clk_,
	       lmp_->wakeupClk_, lmp_->bb_->clkn_, piconet->clk_offset);
	*/
    }

    delete holdreq;
    holdreq = 0;

    if (lmp_->numPico() > 1 ||
	(lmp_->rpScheduler && !lmp_->rpScheduler->sniffBased())) {
	lmp_->setup_bridge(this);
    }
}

void LMPLink::clear_hold_ev()
{
    Scheduler & s = Scheduler::instance();

    if (_hold_ev) {
	s.cancel(_hold_ev);
	delete _hold_ev;
    }
    if (_hold_ev_wakeup) {
	s.cancel(_hold_ev_wakeup);
	delete _hold_ev_wakeup;
    }
    _hold_ev = NULL;
    _hold_ev_wakeup = NULL;
}

void LMPLink::handle_hold()
{
    Scheduler & s = Scheduler::instance();
    if (suspended) {
	if (lmp_->trace_state()) {
	    printf
		("LMPLink::handle_hold(): %d-%d at %f on hold -- already suspended\n",
		 lmp_->_my_info->bd_addr_, remote->bd_addr_, s.clock());
	}
	return;
    }
    suspended = 1;
    _on_hold = 1;

    // schedule when to wake up
    _hold_ev_wakeup = new LMPEvent(this, LMPEvent::HoldWakeup);

    double clkdrft = 0;
    if (LMP::useReSyn_ && !piconet->isMaster()) {
	clkdrft = (lmp_->lowDutyCycle_ ? CLKDRFT_SLEEP : CLKDRFT);
    }

    double t = (holdreq->hold_time) * lmp_->bb_->slotTime() * (1 - clkdrft);
    // t -= 4E-6;
    s.schedule(&lmp_->_timer, _hold_ev_wakeup, t);
    _wakeupT = s.clock() + t;

    if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf
	    (BtStat::log_,
	     BTPREFIX1
	     "LMPLink::handle_hold(): %d-%d at %f on hold, wakeup at %f hi:%d ht:%d.\n",
	     lmp_->_my_info->bd_addr_, remote->bd_addr_, s.clock(), _wakeupT,
	     holdreq->hold_instant, holdreq->hold_time);
	*/
    }

    piconet->suspend_link(this);

    if (piconet->numActiveLink == 0) {
	if (piconet->numScoLink == 0) {
	    lmp_->suspendCurPiconet();
	} else {
	    piconet->schedule_SCO();
	}
    } else {
	lmp_->schedule_set_schedWord(this);
    }
}

void LMPLink::force_oneway_hold(int ht)
{
    Scheduler & s = Scheduler::instance();
    suspended = 1;
    _on_hold = 1;

    // schedule when to wake up
    clear_hold_ev();
    _hold_ev_wakeup = new LMPEvent(this, LMPEvent::HoldWakeup);
    double t_2nextframe = lmp_->bb_->t_clk_00_ + lmp_->bb_->slotTime() * 2
	- s.clock();
    int hi = (lmp_->bb_->clk_ & 0xFFFFFFFC) / 2 + 2;
    if (holdreq) {
	delete holdreq;
    }
    holdreq = new HoldReq(ht, hi);

    double clkdrft = 0;
    if (LMP::useReSyn_ && !piconet->isMaster()) {
	clkdrft = (lmp_->lowDutyCycle_ ? CLKDRFT_SLEEP : CLKDRFT);
    }
    double t = (holdreq->hold_time) * lmp_->bb_->slotTime() * (1 - clkdrft)
	+ t_2nextframe;
    // t -= 4E-6;

    s.schedule(&lmp_->_timer, _hold_ev_wakeup, t);
    _wakeupT = s.clock() + t;

    if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf
	    (BtStat::log_,
	     BTPREFIX1
	     "LMPLink::oneway_hold(): %d-%d at %f on hold, wakeup at %f hi:%d ht:%d.\n",
	     lmp_->_my_info->bd_addr_, remote->bd_addr_, s.clock(), _wakeupT,
	     holdreq->hold_instant, holdreq->hold_time);
	*/
    }

    piconet->suspend_link(this);

    if (piconet->numActiveLink == 0) {
	if (piconet->numScoLink == 0) {
	    lmp_->suspendCurPiconet();
	} else {
	    piconet->schedule_SCO();
	}
    }
}

void LMPLink::schedule_hold()
{
    if (!holdreq) {
	return;
    }
    Scheduler & s = Scheduler::instance();
    clear_hold_ev();
    _hold_ev = new LMPEvent(this, LMPEvent::Hold);
    _on_hold = 1;
    suspended = 0;
#if 0
    double t = lmp_->bb_->t_clk_00_ +
	(holdreq->hold_instant * 2 - (lmp_->bb_->clk_ & 0xfffffffc))
	* lmp_->bb_->tick();
#endif

    int clk_hi = holdreq->hold_instant * 2;
    int clk_00 = lmp_->bb_->clk_ & 0xfffffffc;

    if (clk_hi <= clk_00) {
	clk_hi = clk_00 + 4;
	holdreq->hold_instant = clk_hi / 2;
    }

    double t = lmp_->bb_->t_clk_00_ + (clk_hi - clk_00) * lmp_->bb_->tick();
    double now = s.clock();

    s.schedule(&lmp_->_timer, _hold_ev, t - now);
    if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		BTPREFIX1 "sched hold %d-%d: ht:%u hi:%u @ %f (%f)\n",
		lmp_->bb_->bd_addr_, remote->bd_addr_, holdreq->hold_time,
		holdreq->hold_instant, t, now);
	*/
    }
}

void LMPLink::recvd_hold_req(LMPLink::HoldReq * req)
{
    if (_in_sniff) {
	lmp_->lmpCommand(LMP::LMP_NOT_ACCEPTED, LMP::LMP_HOLD_REQ, this);
    } else {
	if (holdreq) {		// I'm sending LMP_HOLD_REQ also
	    if (piconet->isMaster()) {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_, BTPREFIX1
			"I'm master, discard hold request.\n");
		*/
		// both parties send link->holdreq. Master wins.
		// the request is silencely discarded.
		// TODO: Should check timestamp
		return;
	    } else {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_, BTPREFIX1
			"I'm slave, reset my own hold request.\n");
		*/
		delete holdreq;	// discard my own request
		// link->holdreq = NULL;
	    }
	}
	holdreq = new HoldReq(*req);
	lmp_->lmpCommand(LMP::LMP_ACCEPTED, LMP::LMP_HOLD_REQ, this);
	schedule_hold();
    }

}

void LMPLink::request_hold(uint16_t ht, uint32_t hi)
{
    if (hi & 0x01) {
	hi++;
    }
    if (holdreq) {
	if (int (holdreq->hold_instant << 1) < lmp_->bb_->clk_
	    && !_on_hold) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1
		    "%d previous holdreq is not honored. delete it.\n",
		    lmp_->bb_->bd_addr_);
	*/
	    // delete holdreq;
	    return;
	} else {
	/** Commented by Barun [07 March 2013]
	    fprintf
		(BtStat::log_,
		 BTPREFIX1
		 "holdReq %d-%d: ht:%u hi:%u is pending. Can't accept new one.\n",
		 lmp_->bb_->bd_addr_, remote->bd_addr_, holdreq->hold_time,
		 holdreq->hold_instant);
	*/
	    return;
	}
    }
    holdreq = new HoldReq(ht, hi);

    lmp_->lmpCommand(LMP::LMP_HOLD_REQ, (uchar *) holdreq,
		     sizeof(HoldReq), 6, this);
    if (lmp_->trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		BTPREFIX1 "holdReq %d-%d: ht:%u hi:%u clk1:%d t: %f\n",
		lmp_->bb_->bd_addr_, remote->bd_addr_, holdreq->hold_time,
		holdreq->hold_instant, (lmp_->bb_->clk_ >> 1),
		Scheduler::instance().clock());
	*/
    }
    // schedule_hold(); // put it here make it forced hold,
    // -- moved to after req accepted.
}

void LMPLink::request_hold()
{
    // specs: >= 6*T_poll or 9*T_poll
    uint32_t hi =
	((lmp_->bb_->clk_ >> 2) << 1) + 10 * connhand->link->T_poll_;
    uint16_t ht = (hold_Mode_Min_Interval + hold_Mode_Max_Interval) / 2;
    request_hold(ht, hi);
}

void LMPLink::recv_detach_ack()
{
    if (ev && ev->type == LMPEvent::DetachFirstTimer) {
	Scheduler & s = Scheduler::instance();
	s.cancel(ev);
	delete ev;
	ev = new LMPEvent(this, LMPEvent::Detach);
	ev->reason = disconnReason;
	s.schedule(&lmp_->_timer, ev, T_poll_ * 3 * lmp_->bb_->slotTime());
    }
}

void LMPLink::handle_detach_firsttimer()
{
    Scheduler & s = Scheduler::instance();
    if (ev) {
	s.cancel(ev);
	delete ev;
    }
    ev = new LMPEvent(this, LMPEvent::Detach);
    ev->reason = disconnReason;
    s.schedule(&lmp_->_timer, ev, lmp_->supervisionTO_);
}

void LMPLink::qos_request_accepted()
{
    piconet->_res -= res_req;
    N_bc = N_bc_req;
    T_poll_ = T_poll_req;
    if (piconet->isMaster()) {
	txBuffer->setPrioClass(TxBuffer::High);
	piconet->compute_sched();
	// set bb schedword ???
    }
}

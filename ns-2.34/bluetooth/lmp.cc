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
 *	lmp.cc
 */

//** Added by Barun; for MIN/MAX
#include <sys/param.h> 

#include "baseband.h"
#include "lmp.h"
#include "l2cap.h"
#include "sco-agent.h"
#include "bt-node.h"
#include "random.h"
#include "lmp-piconet.h"
#include "aodv-bt.h"


/** Added by Barun [07 March 2013] */
#undef BTDEBUG


static class LMPClass:public TclClass {
  public:
    LMPClass():TclClass("LMP") { }
    TclObject *create(int, const char *const *argv) {
	return new LMP();
    }
} class_lmpclass;


//////////////////////////////////////////////////////////
//                      LMPTimer                        //
//////////////////////////////////////////////////////////
void LMPTimer::handle(Event * e)
{
    LMPEvent *le = (LMPEvent *) e;

    switch (le->type) {

    case LMPEvent::CheckLink:
	_lmp->checkLink();
	break;

    case LMPEvent::PeriodicInq:
	_lmp->handle_PeriodicInq();
	break;

    case LMPEvent::TurnOnSCOLink:
	_lmp->add_Sco_link_complete(le);
	delete e;
	break;

    case LMPEvent::SetSchedWord:
	le->link->piconet->setSchedWord();
	break;

    case LMPEvent::ScoWakeup:
	le->link->handle_sco_wakeup();
	break;

    case LMPEvent::ScoSuspend:
	le->link->handle_sco_suspend();
	break;

    case LMPEvent::SniffWakeup:
	le->link->handle_sniff_wakeup();
	break;

    case LMPEvent::SniffSuspend:
	le->link->handle_sniff_suspend();
	break;

    case LMPEvent::Hold:
	le->link->handle_hold();
	break;

    case LMPEvent::HoldWakeup:
	le->link->handle_hold_wakeup();
	break;

#ifdef PARK_STATE
    case LMPEvent::EnterPark:
	le->link->enter_park();
	break;

    case LMPEvent::Park:
	le->link->handle_park();
	break;
#endif

    case LMPEvent::RoleSwitch:
	le->link->handle_role_switch_tdd();
	break;

    case LMPEvent::RSTakeOver:
	le->link->handle_rs_takeover();
	break;

    case LMPEvent::ForcePage:
	_lmp->force_page();
	break;

    case LMPEvent::ForceScan:
	_lmp->force_scan();
	break;

    case LMPEvent::ForceInquiry:
	_lmp->force_inquiry();
	break;

    case LMPEvent::DetachFirstTimer:
	le->link->handle_detach_firsttimer();
	break;

    case LMPEvent::Detach:
	le->link->piconet->detach_link(le->link, le->reason);
	break;

    case LMPEvent::SlaveStartClk:
	_lmp->doSlaveStartClk(le->pico);
	delete e;
	break;

    case LMPEvent::NumEvent:
	break;

    default:
	break;
    }
}


//////////////////////////////////////////////////////////
//                      ConnectionHandle                //
//////////////////////////////////////////////////////////

ConnectionHandle::ConnectionHandle(uint16_t pt, uint8_t rs)
:
next(0), reqScoHand(0),
packet_type(pt), recv_packet_type(hdr_bt::DH1),
allow_role_switch(rs), ready_(0), link(0), chan(0),
head(0), _last(0), numChan(0)
{
}

void ConnectionHandle::add_channel(L2CAPChannel * ch)
{
    if (chan == NULL) {
	ch->linknext = ch;
	head = ch;
	_last = ch;
	numChan = 1;
    } else {
	ch->linknext = chan->linknext;
	chan->linknext = ch;
	numChan++;
    }
    chan = ch;
    ch->_connhand = this;
}

void ConnectionHandle::remove_channel(L2CAPChannel * ch)
{
    if (chan == NULL) {
	return;
    }
    if (numChan == 1) {
	if (chan == ch) {
	    chan = head = _last = NULL;
	    numChan = 0;
	    // HCI_Disconnect(this, 0);
	} else {
	    fprintf(stderr, "Ooops, %s ch does not match\n", __FUNCTION__);
	    abort();
	}
	return;
    }

    L2CAPChannel *wk = chan;
    L2CAPChannel *par = _last;
    for (int i = 0; i < numChan; i++) {
	if (wk == ch) {
	    if (chan == ch) {
		chan = chan->linknext;
	    }
	    if (head == ch) {
		head = head->linknext;
	    }
	    if (_last == ch) {
		_last = par;
	    }
	    numChan--;
	    par->linknext = wk->linknext;
	    return;
	}
	par = wk;
	wk = wk->linknext;
    }
}

int ConnectionHandle::isMaster()
{
    return link->piconet->isMaster();
}

//////////////////////////////////////////////////////////
//                      LMP                             //
//////////////////////////////////////////////////////////
// int LMP::useReSyn_ = 1;
int LMP::useReSyn_ = 0;

hdr_bt::packet_type LMP::_defaultPktType1slot = hdr_bt::DH1;
hdr_bt::packet_type LMP::_defaultPktType3slot = hdr_bt::DH3;
hdr_bt::packet_type LMP::_defaultPktType5slot = hdr_bt::DH5;

LMP::LMP()
:
bb_(0), l2cap_(0),
_checkLink_ev(LMPEvent::CheckLink),
_name("unamed"), _timer(this), periodInq_ev(LMPEvent::PeriodicInq),
forcePage(LMPEvent::ForcePage),
forceScan(LMPEvent::ForceScan), forceInquiry(LMPEvent::ForceInquiry)
{
    bind("giac_", &giac_);
    bind("inq_max_period_length_", &inq_max_period_length_);
    bind("inq_min_period_length_", &inq_min_period_length_);
    bind("inquiry_length_", &inquiry_length_);
    bind("inq_num_responses_", &inq_num_responses_);
    bind("nb_timeout_", &nb_timeout_);
    bind("nb_dist_", &nb_dist_);
    bind("takeover_", &takeover_);
    bind("scan_after_inq_", &scan_after_inq_);

    bind("supervisionTO_", &supervisionTO_);
    bind("supervisionEnabled_", &supervisionEnabled_);
    bind("idleSchred_", &idleSchred_);
    bind("defaultHoldTime_", &defaultHoldTime_);
    bind("minHoldTime_", &minHoldTime_);
    bind("maxHoldTime_", &maxHoldTime_);
    bind("autoOnHold_", &autoOnHold_);
    bind("idleCheckEnabled_", &idleCheckEnabled_);
    bind("idleCheckIntv_", &idleCheckIntv_);
    bind("nullTriggerSchred_", &nullTriggerSchred_);
    bind("failTriggerSchred_", &failTriggerSchred_);

    bind("NPage_manual_", &NPage_manual_);
    bind("NInqury_manual_", &NInqury_manual_);

    bind("defaultTSniff_", &defaultTSniff_);
    bind("defaultSniffAttempt_", &defaultSniffAttempt_);
    bind("defaultSniffTimeout_", &defaultSniffTimeout_);

    bind("defaultPktType_", &defaultPktType_);
    bind("defaultRecvPktType_", &defaultRecvPktType_);
    bind("allowRS_", &allowRS_);

    bind("pageStartTO_", &pageStartTO_);
    bind("inqStartTO_", &inqStartTO_);
    bind("scanStartTO_", &scanStartTO_);

    bind("scanWhenOn_", &scanWhenOn_);
    bind("lowDutyCycle_", &lowDutyCycle_);

    _init();
}

void LMP::_init()
{
    _root = 0;
    _my_info = 0;
    _bd = 0;
    numPico_ = 0;
    masterPico = 0;
    masterPico_old = 0;
    scoPico = 0;
    scoPico1 = 0;
    curPico = 0;
    nextPico = 0;
    suspendPico = 0;

    tmpPico_ = 0;
    disablePiconetSwitch_ = 0;
    RsFailCntr_ = 0;

    nb_timeout_ = NB_TIMEOUT;
    nb_dist_ = NB_RANGE;
    takeover_ = 0;
    scan_after_inq_ = 0;
    NPage_manual_ = 0;
    NInqury_manual_ = 0;

    reqOutstanding = 0;

    rpScheduler = NULL;

    l2capChPolicy_ = l2RR;

    maxPageRetries_ = 3;
    pageStartTO_ = PAGESTARTTO;
    inqStartTO_ = INQSTARTTO;
    scanStartTO_ = SCANSTARTTO;

    defaultPktType_ = hdr_bt::DH1;
    defaultRecvPktType_ = hdr_bt::DH1;
    allowRS_ = 1;
    role_ = DontCare;

    _agent = 0;

    giac_ = GIAC;

    inq_max_period_length_ = INQ_MAX_PERIOD_LEN;
    inq_min_period_length_ = INQ_MIN_PERIOD_LEN;
    inquiry_length_ = T_GAP_100;
    inq_num_responses_ = INQ_NUM_RESP;

    _inq_callback_ind = 0;
    _num_inq_callback = 0;

    _pagereq = _pagereq_tail = 0;

    _pending_act = None;
    _scan_mask = 0;

    _num_Broadcast_Retran = BT_NUM_BCAST_RETRAN;
    _max_num_retran = BT_TRANSMIT_MAX_RETRAN;
    _on = 0;
    scanWhenOn_ = 1;
    lowDutyCycle_ = 0;

    supervisionTO_ = SUPERVISIONTO;
    supervisionEnabled_ = 0;
    idleSchred_ = IDLESCHRED;	// sec
    defaultHoldTime_ = DEF_HOLDT;	// slots 
    minHoldTime_ = MIN_HOLDT;
    maxHoldTime_ = MAX_HOLDT;
    autoOnHold_ = 0;
    idleCheckEnabled_ = 1;
    idleCheckIntv_ = IDLECHECKINTV;
    nullTriggerSchred_ = DEF_NULL_TRGR_SCHRED;
    failTriggerSchred_ = DEF_POLL_TRGR_SCHRED;

    defaultTSniff_ = DEF_TSNIFF;
    defaultSniffAttempt_ = DEF_SNIFFATTEMPT;
    defaultSniffTimeout_ = DEF_SNIFFTIMEOUT;
}

void LMP::reset()
{
    if (curPico) {
	remove_piconet(curPico);
    }
    for (int i = numPico_; i > 0; i--) {
	remove_piconet(suspendPico);
    }
    numPico_ = 0;
    masterPico = 0;
    masterPico_old = 0;
    scoPico = 0;
    scoPico1 = 0;
    curPico = 0;
    nextPico = 0;
    suspendPico = 0;
    tmpPico_ = 0;

    freeBdinfo();

    remove_page_req();

    // bb_->reset();
    // _init();
    // _my_info = new Bd_info(bb_->bd_addr_, bb_->clkn_);
}

void LMP::setup(int bdaddr, BTChannel * ch, Baseband * bb, L2CAP * l2cap,
		BTNode * node)
{
    int clk = Random::integer(CLK_RANGE);	// not too big to wrap around
    _my_info = new Bd_info(bdaddr, clk);
    bb_ = bb;
    l2cap_ = l2cap;
    node_ = node;

    bb_->setup(bdaddr, clk, ch, this, node);
}

void LMP::on()
{
    if (_on) {
	return;
    }
    _on = 1;

    bb_->on();

    checkLink();
    if (scanWhenOn_) {
	HCI_Write_Scan_Enable(0x03);	// enable pagescan and inqscan
    }
}

// return number of nodes within radio range
int LMP::computeGeometryDegree()
{
    int num = 0;
    BTNode *wk = node_;
    while ((wk = wk->getNext()) != node_) {	// exclude myself
	if (node_->distance(wk) < node_->radioRange_) {
	    num++;
	}
    }
    return num;
}

// Degree in the connectivity graph
//   slave: 1
//   master: number of 1 hop masters or 2 hop masters via S/S bridges
//   S/S bridge: number of masters
//   M/S bridge: number of masters + number of 1 hop masters or 2 hop 
//                                   masters via S/S bridges for master role.
//   Well, the degree of a master need more thoughts.
//    Basically, we want to remove duplicated bridges.
int LMP::computeDegree(int *role, int *num_br, int *numSlave)
{
    if (numPico() == 0) {
	*role = 0;		// not connected.
	return 0;
    }
    if (masterPico) {
	int ma[128];
	int numMa = 0;
	int i;
	int numBr = 0;
	ma[numMa++] = bb_->bd_addr_;

	LMPLink *link = masterPico->activeLink;
	for (i = 0; i < masterPico->numActiveLink; i++) {
	    numBr += computeNumMasterOfBridge(link, ma, &numMa);
	    link = link->next;
	}
	link = masterPico->suspendLink;
	for (i = 0; i < masterPico->numSuspendLink; i++) {
	    numBr += computeNumMasterOfBridge(link, ma, &numMa);
	    link = link->next;
	}
	*role = (numPico() == 1 ? 2 : 4);	// 2: master 4: M/S bridge
	*num_br = numBr;
	*numSlave = masterPico->numActiveLink + masterPico->numSuspendLink;
	return (numBr < numMa - 1 ? numBr : (numMa - 1)) + numPico() - 1;
    } else {
	*role = (numPico() == 1 ? 1 : 3);	// 1: slave 3: S/S bridge
	return numPico();
    }
}

int LMP::computeNumMasterOfBridge(LMPLink * link, int *ma, int *numMa)
{
    BTNode *rmt = node_->lookupNode(link->remote->bd_addr_);
    if (rmt->lmp_->numPico() <= 1) {
	return 0;
    } else if (rmt->lmp_->masterPico) {
	ma[(*numMa)++] = rmt->bb_->bd_addr_;
	return 1;
    }

    int i = 0;
    Piconet *pico = rmt->lmp_->curPico;
    if (pico) {
	LMPLink *lk = (pico->activeLink ? pico->activeLink :
		       pico->suspendLink);
	_addMaforBr(ma, numMa, lk->remote->bd_addr_);
	i++;
    }
    pico = rmt->lmp_->suspendPico;
    for (; i < rmt->lmp_->numPico(); i++) {
	if (!pico) {
	    break;
	}
	LMPLink *lk = (pico->activeLink ? pico->activeLink :
		       pico->suspendLink);
	if (lk) {
	    _addMaforBr(ma, numMa, lk->remote->bd_addr_);
	} else {
	    fprintf(stderr, "*** %d %f %s: suspendPico has no link,"
		    " numPico(): %d\n",
		    rmt->lmp_->bb_->bd_addr_,
		    Scheduler::instance().clock(),
		    __FUNCTION__, rmt->lmp_->numPico());
	}
	pico = pico->next;
    }
    return 1;
}

void LMP::_addMaforBr(int *ma, int *numMa, int newma)
{
    for (int i = 0; i <= *numMa; i++) {
	if (ma[i] == newma) {
	    return;
	}
    }
    ma[(*numMa)++] = newma;
}

void LMP::dump(FILE * out, int dumpCurPico)
{
    if (out == NULL) {
	out = stdout;
    }
    int isma = 0;
    if (numPico() == 0) {
	/** Commented by Barun [07 March 2013]
	fprintf(out, "%d : not connected.\n", bb_->bd_addr_);
	*/
    } else if (masterPico) {
	/** Commented by Barun [07 March 2013]
	fprintf(out, "%d(m) :", bb_->bd_addr_);
	*/
	int i;
	LMPLink *link = masterPico->activeLink;
	for (i = 0; i < masterPico->numActiveLink; i++) {
	/** Commented by Barun [07 March 2013]
	    fprintf(out, " %d", link->remote->bd_addr_);
	*/
	    link = link->next;
	}
	link = masterPico->suspendLink;
	for (i = 0; i < masterPico->numSuspendLink; i++) {
	/** Commented by Barun [07 March 2013]
	    fprintf(out, " %d", link->remote->bd_addr_);
	*/
	    link = link->next;
	}
	/** Commented by Barun [07 March 2013]
	fprintf(out, "\n");
	*/
	isma = 1;
    }

    if (numPico() == 1 && !isma) {
	/** Commented by Barun [07 March 2013]
	fprintf(out, "%d(s) :", bb_->bd_addr_);
	*/
	Piconet *pico = (curPico ? curPico : suspendPico);
	/** Commented by Barun [07 March 2013]
	fprintf(out, " %d\n", pico->master_bd_addr_);
	*/
    }

    if (numPico() > 1) {
	if (isma) {
	/** Commented by Barun [07 March 2013]
	    fprintf(out, " M/S %d roles: ", numPico());
	*/
	} else {
	/** Commented by Barun [07 March 2013]
	    fprintf(out, "%d(S/S %d roles): ", bb_->bd_addr_, numPico());
	*/
	}
	int i = 0;
	Piconet *pico = curPico;
	if (pico) {
	/** Commented by Barun [07 March 2013]
	    fprintf(out, " %d", pico->master_bd_addr_);
	*/
	    i++;
	}
	pico = suspendPico;
	for (; i < numPico(); i++) {
	    if (!pico) {
		break;
	    }
	/** Commented by Barun [07 March 2013]
	    fprintf(out, " %d", pico->master_bd_addr_);
	*/
	    pico = pico->next;
	}
	/** Commented by Barun [07 March 2013]
	fprintf(out, "\n");
	*/
    }
    if (curPico && dumpCurPico) {
	/** Commented by Barun [07 March 2013]
	fprintf(out, " cur: %d has %d activeLinks and %d suspenedLinks\n",
		curPico->master_bd_addr_,
		curPico->numActiveLink, curPico->numSuspendLink);
	*/
    }
}

// This function should only be used before the node is turned on.
// Extra work needed to update various clock offset if used in the middle
// of simulation.
void LMP::setClock(int clk)
{
    if (_on) {
	fprintf(stderr,
		"LMP::setClock() can't be used after the node is on.\n");
	abort();
    }
    bb_->clkn_ = clk;
    _my_info->clkn_ = clk;
}

// return the piconet with master as MASTER
Piconet *LMP::lookupPiconetByMaster(bd_addr_t master)
{
    if (curPico && curPico->master_bd_addr_ == master) {
	return curPico;
    }
    if (!suspendPico) {
	return NULL;
    }
    Piconet *wk = suspendPico;
    do {
	if (wk->master_bd_addr_ == master) {
	    return wk;
	}
    } while ((wk = wk->next) != suspendPico);
    return NULL;
}

void LMP::remove_piconet(Piconet * p)
{
    if (rpScheduler) {
	rpScheduler->stop(p);
    }

    if (masterPico == p) {
	masterPico = NULL;
	// should free RF channel too.
    }
    if (curPico == p) {
	// delete curPico;
	curPico = NULL;
	numPico_--;
	// Scheduler::instance().cancel(&bb_->clk_ev_);
	//return;
    } else {

	int nPico = (curPico ? numPico() - 1 : numPico());

	Piconet *pico = suspendPico;
	for (int i = 0; i < nPico; i++) {
	    if (pico == p) {
		if (nPico == 1) {
		    suspendPico = NULL;
		} else {
		    pico->prev->next = pico->next;
		    pico->next->prev = pico->prev;
		    if (pico == suspendPico) {
			suspendPico = suspendPico->next;
		    }
		}
		numPico_--;
		break;
	    }
	    pico = pico->next;
	}
    }

    if (numPico_ == 0) {
	bb_->clearConnectState();
    }
}

// Add a new piconet.
LMPLink *LMP::add_piconet(Bd_info * remote, int myrole,
			  ConnectionHandle * connh)
{
    Bd_info *master, *slave;
    if (myrole) {		// master role
	master = _my_info;
	slave = remote;
    } else {
	master = remote;
	slave = _my_info;
    }

    LMPLink *link = new LMPLink(remote, this, connh);
    Piconet *np = _create_piconet(master, slave, link);
    add_piconet(np, myrole);
    if (myrole) {
	// masterPico = np;
#if 0
	masterPico->rfChannel_ = new BTChannel();
	masterPico->rfChannel_->add(bb_);
	masterPico->rfChannel_->add(node_->lookupNode(slave->bd_addr_)->
				    bb_);
#endif
    }

    return link;
}

// Add a new piconet.
LMPLink *LMP::add_piconet(Piconet * np, int myrole)
{
    if (myrole && masterPico) {
	fprintf(stderr, "***OOps, %d %s: multiple master piconets.\n",
		bb_->bd_addr_, __FUNCTION__);
	abort();
    }

    if (curPico) {
	if (curPico->numActiveLink > 0) {
	    fprintf(stderr,
		    "***OOps, %d %s: curPico exists with ActiveLink.\n",
		    bb_->bd_addr_, __FUNCTION__);
	    // abort();
	}

	fprintf(stderr, "***WARNing, %d %s: curPico exists.\n",
		bb_->bd_addr_, __FUNCTION__);
	// Suspend curPico -- put it on the suspendPico list.
	if (suspendPico == NULL) {
	    suspendPico = curPico;
	} else {
	    curPico->next = suspendPico;
	    curPico->prev = suspendPico->prev;
	    suspendPico->prev->next = curPico;
	    suspendPico->prev = curPico;
	}
	curPico = NULL;
    }
    numPico_++;

    if (myrole) {
	masterPico = np;
    }
    curPico = np;
    // return link;
    return curPico->activeLink;
}

Piconet *LMP::_create_piconet(Bd_info * master, Bd_info * slave,
			      LMPLink * link)
{
    Piconet *np = new Piconet(master, slave, link);
    // at this moment, np only has one ACL, ie, np->activeLink.
    np->lt_table[slave->lt_addr_] = np->activeLink;
    np->lt_table_len =
	(slave->lt_addr_ <
	 np->lt_table_len ? np->lt_table_len : slave->lt_addr_ + 1);
    np->activeLink->lt_addr_ = slave->lt_addr_;

    link->piconet = np;

    return np;
}

// Used for RS
// It is a piconet where I'm a slave.  The piconet has
// a single acl link
void LMP::add_slave_piconet(LMPLink * link)
{
    curPico = _create_piconet(link->remote, _my_info, link);
}

// type: 0 -- i'm master
//       1 -- bd is master
//       2 -- doesn't care
LMPLink *LMP::lookupLink(bd_addr_t bd, int type)
{
    LMPLink *ret;
    if (curPico) {
	ret = curPico->lookupLink(bd);
	if (ret) {
	    return ret;
	}
    }
    if (!suspendPico) {
	return 0;
    }
    Piconet *wk = suspendPico;
    do {
	if ((ret = wk->lookupLink(bd))) {
	    return ret;
	}
    } while ((wk = wk->next) != suspendPico);

    return 0;

#if 0
    if (type == 0) {
	if (masterPico == NULL) {
	    return NULL;
	} else {
	    return masterPico->lookupLink(bd);
	}
    } else if (type == 1) {
	// I'm a slave. Only single ACL exists.
	if (curPico && curPico->master_bd_addr_ == bd) {
	    return curPico->activeLink ? curPico->activeLink : curPico->
		suspendLink;
	} else if (suspendPico) {
	    Piconet *wk = suspendPico;
	    do {
		if (wk->master_bd_addr_ == bd) {

		    // TODO: This is weird.
		    if (!wk->suspendLink) {
			fprintf(stderr,
				"OOps, suspeneded Pico has active Link.\n");
			wk->dump(stderr);
		    }
		    return (wk->suspendLink ? wk->suspendLink : wk->
			    activeLink);
		}
	    } while ((wk = wk->next) != suspendPico);
	}
	return NULL;
    } else {
	if ((ret = lookupLink(bd, 0))) {
	    return ret;
	} else {
	    return lookupLink(bd, 1);
	}
    }
#endif
}

void LMP::freeBdinfo()
{
    Bd_info *wk = _bd;
    while (wk) {
	wk = wk->next_;
	delete _bd;
	_bd = wk;
    }
}

// LMP maintains a database pointed by _bd
Bd_info *LMP::lookupBdinfo(bd_addr_t addr)
{
    Bd_info *wk = _bd;
    while (wk) {
	if (wk->bd_addr_ == addr) {
	    return wk;
	}
	wk = wk->next_;
    }
    return (addr == _my_info->bd_addr_ ? _my_info : NULL);
}

// Caution: Side effect: if bd is in the database, the record will be freed.
// A pointer to the old record with updated content will be returned.
// TODO: why not just fill in bd_addr_, etc in arguements.
Bd_info *LMP::_add_bd_info(Bd_info * bd, int *newbd)
{
    Bd_info *wk = _bd;

    // check if it's registered.  If so, simply update it.
    while (wk) {
	if (wk->bd_addr_ == bd->bd_addr_) {
	    wk->clkn_ = bd->clkn_;
	    wk->offset_ = bd->offset_;
	    if (newbd) {
		if (wk->last_seen_time_ >= _inq_time) {
		    *newbd = 0;
		} else {
		    *newbd = 1;
		}
	    }
	    wk->last_seen_time_ = bd->last_seen_time_;
	    delete bd;		// CAUTION !
	    return wk;
	}
	wk = wk->next_;
    }
    bd->next_ = _bd;
    if (newbd) {
	*newbd = 1;
    }
    _bd = bd;
    return bd;
}

void LMP::lmpCommand(uchar opcode, uchar * content, int len, int pl_len,
		     LMPLink * link)
{
    Packet *p = Packet::alloc();
    hdr_cmn *ch = HDR_CMN(p);
    ch->ptype() = PT_LMP;
    hdr_bt *bh = HDR_BT(p);
    bh->pid = hdr_bt::pidcntr++;
    bh->u.lmpcmd.opcode = opcode;
    if (len > 0) {
	memcpy(bh->u.lmpcmd.content, content, len);
    }
    //
    // Warning: different from specs.
    //
    if (pl_len > 16) {
	bh->type = hdr_bt::DH1;
    } else {
	bh->type = hdr_bt::DM1;
    }

    bh->size = hdr_bt::packet_size(bh->type, pl_len + 1);
    bh->ph.l_ch = L_CH_LM;
    bh->receiver = link->remote->bd_addr_;
    bh->comment("lm");
    bh->transmitCount = 0;

    // TODO: set up queue length. Remove stale record. LMP Timeout??
    link->enqueue(p);
}

// --
// Should move above the stack ??
void LMP::fwdCommandtoAll(uchar opcode, uchar * content, int len,
			  int pl_len, LMPLink * from, int flags)
{
    int numactivepico = 0;
    if (curPico) {
	curPico->fwdCommandtoAll(opcode, content, len, pl_len, from,
				 flags);
	numactivepico = 1;
    }

    Piconet *wk = suspendPico;
    for (int i = 0; i < numPico() - numactivepico; i++) {
	wk->fwdCommandtoAll(opcode, content, len, pl_len, from, flags);
	wk = wk->next;
    }
}

void LMP::recvRoleSwitchReq(uint32_t instant, LMPLink * link)
{
    send_slot_offset(link);	// Slave only

    if (int (instant) <= bb_->clk_) {
	lmpCommand(LMP_NOT_ACCEPTED, LMP_SWITCH, link);
	return;
    }

    Scheduler & s = Scheduler::instance();
    double t = (instant - (bb_->clk_ & 0xFFFFFFFC)) * bb_->tick()
	+ bb_->t_clk_00_ - s.clock();

    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_, BTPREFIX1
		"%d LMP_SWITCH:instant: %d (%f) clk:%d diff:%d\n",
		bb_->bd_addr_, instant, t + s.clock(),
		bb_->clk_, instant - bb_->clk_);
	*/

    }

    s.schedule(&_timer, (new LMPEvent(link, LMPEvent::RoleSwitch)), t);

    lmpCommand(LMP_ACCEPTED, LMP_SWITCH, link);
}

void LMP::adjustSlotNum(int nslotnum, hdr_bt::packet_type * pt_ptr,
			LMPLink * link)
{
    uchar oldslotnum = hdr_bt::slot_num(*pt_ptr);
    // link->pt_slot_num = slotnum;
    if (nslotnum == 5) {
	*pt_ptr = _defaultPktType5slot;
    } else if (nslotnum == 3) {
	*pt_ptr = _defaultPktType3slot;
    } else {
	*pt_ptr = _defaultPktType1slot;
    }
    if (nslotnum != oldslotnum) {
	schedule_set_schedWord(link);
    }
}

void LMP::sendUp(Packet * p, Handler * h)
{
    hdr_bt *bh = HDR_BT(p);
    if (bh->bcast) {
	// handle bcast packets
	/** Commented by Barun [07 March 2013]
	fprintf(stdout, "%d BCAST packet received. Not handled.\n",
		bb_->bd_addr_);
	*/
	bh->dump();
	Packet::free(p);

#ifdef PARK_STATE
	// TODO: pack mode has very low priority, probably never complete.
	if (bb_->_inBeacon) {
	    // check if it's to unpack me.
	    // if so, unpark().
	    // enter UNPARK_POST state, waiting POLL util newConnTO.
	    // or start a timer, after newConnTO, check if it's ok.
	}
#endif
	// pass to L2CAP ??
	return;
    }

    LMPLink *link = bh->txBuffer->link();
    bh->connHand_ = link->connhand;

    if (bh->scoOnly() && link->type() == SCO) {	// SCO data
	link->connhand->agent->recv(p);
	return;
    }

    if (bh->ph.l_ch != L_CH_LM) {	// User DATA -> L2CAP
	uptarget_->recv(p, h);
	return;
    }
#ifdef BTDEBUG
    if (trace_state()) {
	if (bh->u.lmpcmd.opcode == LMP_ACCEPTED) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, BTPREFIX1
		    "%d-%d LMP::recv ACCEPT %s\n", bb_->bd_addr_,
		    link->remote->bd_addr_,
		    opcode_str(bh->u.lmpcmd.content[0]));
	*/
	} else if (bh->u.lmpcmd.opcode == LMP_NOT_ACCEPTED) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, BTPREFIX1
		    "%d-%d LMP::recv NOT_ACCEPT %s\n", bb_->bd_addr_,
		    link->remote->bd_addr_,
		    opcode_str(bh->u.lmpcmd.content[0]));
	*/
	} else {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, BTPREFIX1
		    "%d-%d LMP::recv %s\n", bb_->bd_addr_,
		    link->remote->bd_addr_,
		    opcode_str(bh->u.lmpcmd.opcode));
	*/
	}
    }
#endif

    switch (bh->u.lmpcmd.opcode) {
    case LMP_NAME_REQ:
	break;
    case LMP_NAME_RES:
	break;
    case LMP_ACCEPTED:
	switch (bh->u.lmpcmd.content[0]) {
	case LMP_HOST_CONNECTION_REQ:
	    lmpCommand(LMP_SETUP_COMPLETE, link);
	    link->sent_setup_complete = 1;
	    break;
	case LMP_SCO_LINK_REQ:	// master
	    _allocate_Sco_link(link);
	    break;
	case LMP_REMOVE_SCO_LINK_REQ:
	    // find out which SCO link is going to be disconnnected.
	    if (link->sco_remv) {
		if (link->acl) {
		    link->acl->sco_remv = NULL;
		}
		_remove_Sco_link(link->sco_remv);
		link->sco_remv = NULL;
	    }
	    break;
	case LMP_HOLD_REQ:
	    link->schedule_hold();
	    break;
	case LMP_UNSNIFF_REQ:
	    if (!link->piconet->isMaster()) {
		link->handle_unsniff();
	    }
	    break;
	case LMP_SNIFF_REQ:
	    link->schedule_sniff();
	    break;
#ifdef PARK_STATE
	case LMP_PARK_REQ:
	    link->schedule_park();
	    break;
#endif
	case LMP_QOS_REQ:
	    link->qos_request_accepted();
	    break;
	case LMP_MAX_SLOT_REQ:
	    {
		uchar oldslotnum = hdr_bt::slot_num(link->pt);
		link->pt =
		    (hdr_bt::packet_type) link->connhand->packet_type;
		if (hdr_bt::slot_num(link->pt) != oldslotnum) {
		    schedule_set_schedWord(link);	// compute new schedword.
		}
		negotiate_link_param(link);
		break;
	    }
	}
	break;

    case LMP_NOT_ACCEPTED:
	switch (bh->u.lmpcmd.content[0]) {
	case LMP_HOST_CONNECTION_REQ:
	    break;
	case LMP_SCO_LINK_REQ:
	    // _sco_connection_abort(link);
	    break;
	case LMP_HOLD_REQ:
	    delete link->holdreq;
	    link->holdreq = NULL;
	    break;
	case LMP_QOS_REQ:
	    fprintf(BtStat::log_, BTPREFIX1
		    "LMP:%d: QOS_REQ is NOT accepted by %d\n",
		    bb_->bd_addr_, bh->sender);
	    break;
	}
	break;

    case LMP_CLKOFFSET_REQ:
	break;
    case LMP_CLKOFFSET_RES:
	break;
    case LMP_DETACH:
	recv_detach(link, bh->u.lmpcmd.content[0]);
	break;

    case LMP_SLOT_OFFSET:
	{
	    LMPLink::SlotOffset * res = (LMPLink::SlotOffset *)
		bh->u.lmpcmd.content;

	    link->add_slot_offset(res);
	    bb_->slot_offset_ = res->offset;
	}
	break;

    case LMP_SWITCH:
	recvRoleSwitchReq(*(uint32_t *) bh->u.lmpcmd.content, link);
	break;

    case LMP_HOLD:		// forced Hold.
	// break;
    case LMP_HOLD_REQ:
	link->recvd_hold_req((LMPLink::HoldReq *) bh->u.lmpcmd.content);
	break;

    case LMP_SNIFF_REQ:
	link->recvd_sniff_req((LMPLink::SniffReq *) bh->u.lmpcmd.content);
	break;
    case LMP_UNSNIFF_REQ:
	lmpCommand(LMP_ACCEPTED, LMP_UNSNIFF_REQ, link);
	link->handle_unsniff();
	break;
#ifdef PARK_STATE
    case LMP_PARK_REQ:
	link->recvd_park_req((LMPLink::ParkReq *) bh->u.lmpcmd.content);
	break;
    case LMP_UNPARK_BD_ADDR_REQ:
	break;
    case LMP_UNPARK_PM_ADDR_REQ:
	break;
#endif
    case LMP_QOS:
	{
	    short int T_poll = *(short int *) bh->u.lmpcmd.content;
	    uchar N_bc = bh->u.lmpcmd.content[2];
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, BTPREFIX1
		    "QOS_ind from %d: T_poll: %d  N_bc:%d \n",
		    bh->sender, T_poll, N_bc);
	*/
	}
	break;

    case LMP_QOS_REQ:
	{
	    short int T_poll = *(short int *) bh->u.lmpcmd.content;
	    uchar N_bc = bh->u.lmpcmd.content[2];
	    int pktslot = hdr_bt::slot_num(link->connhand->packet_type);
	    double rpktslot =
		(double) hdr_bt::slot_num(link->connhand->
					  recv_packet_type);
	    double r = (pktslot + rpktslot) / T_poll;

	    link->piconet->compute_sched();
	    if (r < link->piconet->_res) {
		lmpCommand(LMP_NOT_ACCEPTED, LMP_QOS_REQ, link);
	    } else {
		link->piconet->_res -= r;
		lmpCommand(LMP_ACCEPTED, LMP_QOS_REQ, link);
		link->N_bc = N_bc;
		link->T_poll_ = T_poll;
		if (link->piconet->isMaster()) {
		    link->txBuffer->setPrioClass(TxBuffer::High);
		    link->piconet->compute_sched();
		}
	    }
	}
	break;

    case LMP_SCO_LINK_REQ:
	{
	    LMPLink::SCOReq * scoreq = (LMPLink::SCOReq *)
		bh->u.lmpcmd.content;
	/** Commented by Barun [07 March 2013]
	    printf("SCOReq: dsco %d tsco %d \n", scoreq->D_sco,
		   scoreq->T_sco);
	*/
	    if (link->piconet->isMaster()) {
		if (2.0 / scoreq->T_sco > link->piconet->_res) {
		    lmpCommand(LMP_NOT_ACCEPTED, LMP_SCO_LINK_REQ, link);
		} else {
		    // air mode: u-law log PCM, A-laq log PCM, or CVSD
		    // is NOT considered !!
		    HCI_Add_SCO_Connection(link->connhand,
					   scoreq->packet_type);
		}
	    } else {
		if (2.0 / scoreq->T_sco > link->piconet->_res) {
		    lmpCommand(LMP_NOT_ACCEPTED, LMP_SCO_LINK_REQ, link);
		} else {
		    if (link->sco_req) {
			delete link->sco_req;
		    }
		    link->sco_req = new LMPLink::SCOReq(*scoreq);
		    lmpCommand(LMP_ACCEPTED, LMP_SCO_LINK_REQ, link);
		    _allocate_Sco_link(link);
		}
	    }
	}
	break;

    case LMP_REMOVE_SCO_LINK_REQ:
	{
	    LMPLink::SCODisconnReq * req = (LMPLink::SCODisconnReq *)
		bh->u.lmpcmd.content;

	    // check if the req is received from a ACL.
	    if (!link->acl) {
		link = curPico->lookupScoLink(req->connhand);
		if (!link) {
		    break;
		}
	    }
	    // let's send LMP_ACCEPTED on the ACL link and detach
	    // this SCO link immediately.
	    lmpCommand(LMP_ACCEPTED, LMP_REMOVE_SCO_LINK_REQ, link->acl);
	    link->disconnReason = req->reason;
	    if (link->acl->sco_remv == link) {
		link->acl->sco_remv = 0;
	    }
	    _remove_Sco_link(link);
	}
	break;

    case LMP_SETUP_COMPLETE:
	if (!link->sent_setup_complete) {	// slave
	    link->sent_setup_complete = 1;
	    lmpCommand(LMP_SETUP_COMPLETE, link);
	    if (!_sco_req_pending(link->remote->bd_addr_)) {
		l2cap_->connection_ind(link->connhand);
	    }
	    // slave ??
#if 0
	    if (!curPico->rfChannel_) {
		curPico->rfChannel_ =
		    node_->lookupNode(link->remote->bd_addr_)->lmp_->
		    masterPico->rfChannel_;
	    }
#endif
	    if (lowDutyCycle_ && rpScheduler) {
		rpScheduler->start(link);
	    }

	} else {		// master
	    if (link->connhand->reqScoHand) {
		_add_sco_connection(link->connhand);
	    } else {
		link_setup_complete(link);
	    }
	}
	link->connected = 1;
	link->connhand->ready_ = 1;
	if (tmpPico_) {
	    if (!link->piconet->isMaster()) {
		HCI_Switch_Role(link->remote->bd_addr_, 0x00);
	    }
	    tmpPico_ = 0;
	    reqOutstanding = 1;	// RS request.
	} else if (suspendPico) {
	    setup_bridge(link);
	}
	break;

    case LMP_MAX_SLOT:
	adjustSlotNum(bh->u.lmpcmd.content[0], &link->pt, link);
	break;

    case LMP_MAX_SLOT_REQ:
	adjustSlotNum(bh->u.lmpcmd.content[0], &link->rpt, link);
	lmpCommand(LMP_ACCEPTED, LMP_MAX_SLOT_REQ, link);
	break;

    case LMP_HOST_CONNECTION_REQ:
	link->sent_setup_complete = 0;
	lmpCommand(LMP_ACCEPTED, LMP_HOST_CONNECTION_REQ, link);
	break;

	/* begin UCBT extension */
    case LMP_BR_REQ:
	// A slave sends request to the master.
	if (!link->piconet->isMaster()) {
	    if (rpScheduler) {
		rpScheduler->start(link);
	    }
	    // Only a master handle the request.
	} else {
	    BrReq *brreq = (BrReq *) bh->u.lmpcmd.content;
	    if (!rpScheduler || brreq->algo != rpScheduler->type()) {
		fprintf(stderr, "brAlgo_ desn't match.\n");
		abort();
	    }
	    rpScheduler->handle_request(brreq, link);
	}
	break;

    case LMP_RP_SYN:
	{
	    RPSynMsg *req = (RPSynMsg *) bh->u.lmpcmd.content;
	    if (rpScheduler) {
		rpScheduler->recvRPSyn(req, link);
	    }
	}
	break;

    case LMP_DSNIFF_OPT_REQ:
	if (rpScheduler) {
	    rpScheduler->recvRPOptReq((DSniffOptReq *) bh->u.lmpcmd.
				      content, link);
	}
	break;

    case LMP_DSNIFF_OPT_REP:
	if (rpScheduler) {
	    rpScheduler->recvRPOptReply((DSniffOptReq *) bh->u.lmpcmd.
					content, link);
	}
	break;

    case LMP_SLAVE_INFO_REQ:
	link->send_slave_info();
	break;

	// Added to pass slave info for piconet take over.
    case LMP_SLAVE_INFO:
	{
	    LMPLink::Slave_info_msg * msg =
		(LMPLink::Slave_info_msg *) bh->u.lmpcmd.content;
	    link->piconet->add_slave_info(&msg->s1);
	    if (msg->num == 2) {
		link->piconet->add_slave_info(&msg->s2);
	    }
	}
	break;
	/* end UCBT extension */

    }

    Packet::free(p);
}

void LMP::checkLink()
{
    Scheduler & s = Scheduler::instance();
    if (idleCheckEnabled_) {
	if (curPico) {
	    curPico->checkLink();
	}
	if (scoPico && scoPico != curPico) {
	    scoPico->checkLink();
	}
	if (scoPico1 && scoPico1 != curPico) {
	    scoPico1->checkLink();
	}
    }
    s.schedule(&_timer, &_checkLink_ev, idleCheckIntv_);
}

// set bb schedWord at the beginning of next frame.
void LMP::schedule_set_schedWord(LMPLink * link)
{
    Scheduler & s = Scheduler::instance();
    double t = bb_->t_clk_00_ + bb_->slotTime() * 2 - s.clock() - 1E-6;
    if (t > bb_->slotTime() * 2 || t < 0) {
	//XXX
	t = 0;
	link->piconet->setSchedWord();
    } else {
	s.schedule(&_timer, new LMPEvent(link, LMPEvent::SetSchedWord), t);
    }
}

// this function is invoked by the bridge node.  Whenever a node detects
// that it is involved with multiple piconets.  It calls this function to
// try to setup a bridge.
void LMP::setup_bridge(LMPLink * link)
{
    if (!suspendPico) {
	return;
    }

    if (rpScheduler) {
	rpScheduler->start(link);
    }
}

int LMP::lookupRP(BrReq * brreq, LMPLink * exceptLink)
{
    int cntr = 0;
    if (curPico) {
	curPico->lookupRP(brreq, exceptLink);
	cntr = 1;
    }
    Piconet *wk = suspendPico;
    for (int i = 0; i < numPico() - cntr; i++) {
	if (!wk) {
	    return 1;
	}
	wk->lookupRP(brreq, exceptLink);
	wk = wk->next;
    }
    return 1;
}

void LMP::clear_skip()
{
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(stdout, "* %d at %f clear_skip().\n", bb_->bd_addr_,
		Scheduler::instance().clock());
	*/
    }
    int cntr = 0;
    if (curPico) {
	curPico->clear_skip();
	cntr = 1;
    }
    Piconet *wk = suspendPico;
    for (int i = cntr; i < numPico(); i++) {
	wk->clear_skip();
	wk = wk->next;
    }
}

int LMP::get_lt_addr(bd_addr_t slave)
{
    return (masterPico ? masterPico->allocLTaddr() : 1);
}

void LMP::setNeedAdjustSniffAttempt(LMPLink * except)
{
    curPico->setNeedAdjustSniffAttempt(except);
    Piconet *wk = suspendPico;
    for (int i = 0; i < numPico() - 1; i++) {
	wk->setNeedAdjustSniffAttempt(except);
	wk = wk->next;
    }
}

// called by higher layer (bnep).
// XXX: not tested.
void LMP::wakeup(Piconet * pico)
{
    if (pico == NULL) {		// Scanning
	HCI_Write_Scan_Enable(_scan_mask);
	return;
    }
    if (curPico == pico) {	// do nothing ??
	unsuspend(pico);
    } else if (curPico) {
	nextPico = pico;
	_pending_act = PicoSwitch;
    } else {			// curPico = NULL. May still in bb process.
	// check bb state()
	if (bb_->isBusy()) {
	    nextPico = pico;
	    _pending_act = PicoSwitch;
	} else {
	    _bb_cancel();
	    switchPiconet(pico);	//set curPico = pico;
	}
    }
}

// This is one of important functions where timing is very important.
// This function will make pico as curPico and start the new piconet
// at the right timing, ie. at the slot/frame boundary with correct CLK.
//   If keepClk, CLK at bb will not restarted, so timing is not important.
//   If timingOffset < 0, this function will try to figure out the timing
// itself, otherwise the specified timingOffset is added to s.clock().
void LMP::switchPiconet(Piconet * pico, int keepClk, double timingOffset)
{
    if (!pico) {
	fprintf(stderr, "*** Warning: %d %s: pico is null.",
		bb_->bd_addr_, __FUNCTION__);
	return;
    }

    if (pico->numActiveLink == 0 && pico->numScoLink == 0) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		BTPREFIX1 "Warning %d %s: no active link\n",
		bb_->bd_addr_, __FUNCTION__);
	*/
	return;			// no active link
    }

    if (keepClk) {
	// supposed BB has been set up for the new piconet pico.
	unsuspend(pico);	// set curPico = pico;
	bb_->turn_off_trx();
	bb_->setPiconetParam(pico);
	if (!curPico) {
	    fprintf(stderr, "*** Warning: %d %s: curPico is null\n",
		    bb_->bd_addr_, __FUNCTION__);
	}
	return;
    }

    Scheduler & s = Scheduler::instance();

    if (pico == curPico) {
	if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1 "Warning %d %s: switch to myself\n",
		    bb_->bd_addr_, __FUNCTION__);
	*/
	}
	if (bb_->clk_ev_.uid_ > 0) {
	    s.cancel(&bb_->clk_ev_);
	}
	if (bb_->resync_ev_.uid_ > 0) {
	    s.cancel(&bb_->resync_ev_);
	}
/*
	if (bb_->clk_ev_.uid_ > 0 || bb_->resync_ev_.uid_ > 0) {
	    // looks like pico is active already.
	    return;
	}
*/

    } else {
	// check if curPico has activeLink(ACL), if so suspend them
	if (curPico && curPico->numActiveLink > 0) {
	    nextPico = pico;
	    _pending_act = PicoSwitch;
	    // try to put every link on hold, ht=100
	    if (!suspendCurPiconetReq(100)) {
		// Well, I cannot swith at this moment.  Wait for curPico
		// to suspend itself first.
		return;
	    } else if (curPico) {
		suspendCurPiconet();
	    }
	}
	// check if a new link setup pending -- PAGE/PAGE SCAN phase.
	if (!curPico && bb_->clk_ev_.uid_ > 0) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1 "Warning %d %s: clk_ev_ %d > 0.\n",
		    bb_->bd_addr_, __FUNCTION__, int (bb_->clk_ev_.uid_));
	*/
	    s.cancel(&bb_->clk_ev_);
	}

	if (curPico && bb_->clk_ev_.uid_ > 0) {
	    if (trace_state()) {
		/** Commented by Barun [07 March 2013]
		fprintf(stdout, "%d %s cancel clk\n", bb_->bd_addr_,
			__FUNCTION__);
		*/
	    }
	    s.cancel(&bb_->clk_ev_);
	}
	// aborting any pending transmission/receiving.
	if (bb_->rxPkt_) {
	    s.cancel(bb_->rxPkt_);
	    Packet::free(bb_->rxPkt_);
	    bb_->rxPkt_ = NULL;
	}
	// FIXME: how to abort TX, since the packets are copied to
	// the receiver side.  

	// bb_->turn_off_trx();

#if 0
	if (timingOffset == 0 && bb_->_txPkt && bb_->_txPkt->uid_ > 0) {
	    s.cancel(bb_->_txPkt);
	    Packet::free(bb_->_txPkt);
	    bb_->_txPkt = NULL;
	}
#endif

	unsuspend(pico);	// set curPico = pico;
    }

    bb_->setPiconetParam(pico);
    bb_->turn_off_trx();


    // Now.  Carefully, the new piconet will to synchronized to the 
    // correct slot timing and clk.

    if (timingOffset < 0) {
	if (bb_->isMaster()) {
	    startMasterClk();
	} else {
	    slaveStartClk(pico);
	}
	return;
    }

    if (bb_->isMaster()) {
	startMasterClk(timingOffset);

    } else if (useReSyn_) {

	bb_->clk_ = wakeupClk_;
	bb_->enter_re_sync(timingOffset);

    } else {

	bb_->clk_ = wakeupClk_ - 2;
	if (timingOffset > 0) {
	    bb_->clk_ += 4;
	}
	s.schedule(&bb_->clk_handler_, &bb_->clk_ev_, timingOffset);
	if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(stdout,
		    " %d %f Slave start at %f clk:%d t %f 00: %f\n",
		    bb_->bd_addr_, s.clock(), s.clock() + timingOffset,
		    bb_->clk_ + 2, timingOffset, bb_->t_clkn_00_);
	*/
	}
    }
}

// Start master CLK at least timingOffset later
void LMP::startMasterClk(double timingOffset)
{
    assert(bb_->isMaster());
    Scheduler & s = Scheduler::instance();

    double t = s.clock() - bb_->t_clkn_00_;
    if (t < 0) {
	/** Commented by Barun [07 March 2013]
	fprintf(stdout, "*** t_clkn_00_ > s.clock() by %f\n", t);
	*/
	abort();
    }
    if (t < BT_CLKN_CLK_DIFF) {
	bb_->clk_ = bb_->clkn_ - 2;
	t = BT_CLKN_CLK_DIFF - t;
    } else {
	t = bb_->slotTime() * 2 + BT_CLKN_CLK_DIFF - t;
	bb_->clk_ = (bb_->clkn_ & 0xfffffffc) + 2;
    }

    if (t < timingOffset) {
	t += bb_->slotTime() * 2;
	bb_->clk_ += 4;
    }
    if (bb_->clk_ev_.uid_ > 0) {
	s.cancel(&bb_->clk_ev_);
    }
    s.schedule(&bb_->clk_handler_, &bb_->clk_ev_, t);
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(stdout,
		" %d %f start MasterCLK at %f clk:%d t %f 00: %f clkn:%d\n",
		bb_->bd_addr_, s.clock(), s.clock() + t, bb_->clk_ + 2, t,
		bb_->t_clkn_00_, bb_->clkn_);
	*/
    }
}

// Slave start CLK at frame boundary at least timingOffset later
// Note:  the timing should be established by 3 variables:
//        bb_->t_clkn_00_, 
//        bb_->clkn_,
//        pico->slot_offset,
//        pico->clk_offset
// TODO: not functional at this moment
void LMP::slaveStartClk(Piconet * pico, double timingOffset)
{
    assert(timingOffset >= 0);
    assert(curPico == pico);
    assert(pico->slot_offset >= 0 && pico->slot_offset < 1250);
    Scheduler & s = Scheduler::instance();

    // make sure clkn_ update first.
    double newFrameBegin = bb_->t_clkn_00_ + pico->slot_offset * 1E-6
	+ (pico->slot_offset == 0 ? BT_CLKN_CLK_DIFF : 0);

    double t = newFrameBegin - s.clock();
    if (t < timingOffset) {
	t += bb_->slotTime() * 2;
    }
    //XXX: do we need to lock LMP state until CLK is started??
    s.schedule(&_timer, new LMPEvent(pico, LMPEvent::SlaveStartClk), t);
}

void LMP::doSlaveStartClk(Piconet * pico)
{
    assert(curPico == pico);
    Scheduler & s = Scheduler::instance();

    bb_->clk_ = (bb_->clkn_ & 0xfffffffc) + pico->clk_offset - 2;

    s.schedule(&bb_->clk_handler_, &bb_->clk_ev_, 0);
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(stdout, " %d new act start at %f clk:%d expects %d\n",
		bb_->bd_addr_, s.clock(), bb_->clk_ + 2, wakeupClk_);
	*/
    }
}

// return true/1 if every link is suspened and the piconet can be suspended
//                  immediately.
// Otherwize, return false.
int LMP::suspendCurPiconetReq(int suspend_slots)
{
    // method: hold, sniff, park
    // 1. check suspendLink, find the wakeup time.
    // 2. compute the wakeup time.
    // 3. suspend individual Links.

    int ret = 1;
    if (!curPico) {
	return ret;
    }
    curPico->suspendReqed = 1;
    LMPLink *wk = curPico->activeLink;
    int i;
    int T_poll = 2;		// minimum
    double t = 0;

    // get the largest T_poll.
    for (i = 0; i < curPico->numActiveLink; i++) {
	if (wk->T_poll_ > T_poll) {
	    T_poll = wk->T_poll_;
	}
	wk = wk->next;
    }

    uint16_t ht;
    Scheduler & s = Scheduler::instance();

    if (suspend_slots == 0) {	// undefined.
	t = lookupWakeupTime();

	if (t > s.clock()) {
	    ht = (int) ((t - s.clock()) / bb_->slotTime());
	} else if (_pending_act == Page) {
	    // ht = bb_->pageTO_ * 2;
	    ht = bb_->pageTO_ / 4;
	} else {
	    ht = defaultHoldTime_;
	}
    } else {
	ht = suspend_slots;
    }

    // specs: >= 6*T_poll or 9*T_poll
    // I dont want T_poll is too big.  Because big T_poll means it doesn't
    // have traffic.
    if (T_poll > 8) {
	T_poll = 8;
    }
    uint32_t hi = (bb_->clk_ >> 1) + T_poll * 6;
    wk = curPico->activeLink;
    int numActLink = curPico->numActiveLink;
    for (i = 0; i < numActLink; i++) {	// curPico may be 0 after loop 
	if (!wk->suspended) {
	    if (wk->_in_sniff) {
		// _pending_act = LMP::NoAct;
		if (wk->_sniff_ev_to->uid_ > 0) {
		    s.cancel(wk->_sniff_ev_to);
		}
		wk->handle_sniff_suspend();
	    } else if (wk->_on_hold) {
		if (wk->_hold_ev->uid_ > 0) {
		    s.cancel(wk->_hold_ev);
		}
		wk->handle_hold();
	    } else {
		wk->request_hold(ht, hi);
		ret = 0;
	    }
	}
	wk = wk->next;
    }
    return ret;
}

// When the last ACL is suspended, this function is called to
// transfer control to other activities (forming scatternet).
// In case that SCO exists, it should be called right after the SCO frame.
void LMP::suspendCurPiconet()
{
    if (!curPico) {
	return;
    }
    Scheduler & s = Scheduler::instance();
    curPico->suspended = 1;
    curPico->suspendReqed = 0;

#if 0
    if (!curPico->isMaster()) {
	// curPico->clk_offset = bb_->clk_ - bb_->clkn_;
	int clkoffset =
	    (int) ((bb_->clk_ & 0xfffffffc) - (bb_->clkn_ & 0xfffffffc));
	int sltoffset = (int) ((bb_->t_clk_00_ - bb_->t_clkn_00_) * 1E6);
	if (sltoffset < 0) {
	    sltoffset += 1250;
	    clkoffset += 4;
	}
#if 1
	printf("%d:%d Update clock offset before SW %d -> %d, %d -> %d\n",
	       bb_->bd_addr_, bb_->clk_,
	       curPico->clk_offset, clkoffset,
	       curPico->slot_offset, sltoffset);
#endif

	curPico->clk_offset = clkoffset;
	curPico->slot_offset = sltoffset;
    }
#endif

    // s.cancel(&bb_->clk_ev_);

    // check if need to put curPico to suspended list.
    if (!(curPico->isMaster()
	  && (_pending_act == Page || _pending_act == Inq))) {
	// put curPico to suspended list and reset it to NULL.
	if (suspendPico == NULL) {
	    suspendPico = curPico;
	} else {
	    curPico->next = suspendPico;
	    curPico->prev = suspendPico->prev;
	    suspendPico->prev->next = curPico;
	    suspendPico->prev = curPico;
	}
	curPico = NULL;
	if (trace_state()) {
		/** Commented by Barun [07 March 2013]
	    fprintf(stdout, "%d %s cancel clk\n", bb_->bd_addr_,
		    __FUNCTION__);
		*/
	}
	s.cancel(&bb_->clk_ev_);
    } else if (!curPico->scoLink) {
	if (trace_state()) {
		/** Commented by Barun [07 March 2013]
	    fprintf(stdout, "%d %s cancel clk\n", bb_->bd_addr_,
		    __FUNCTION__);
		*/
	}
	s.cancel(&bb_->clk_ev_);
    }
    // handle_pending_act();
}

// Immediately suspend CurPico for page scan.
// Used by a scatternet formation algo.  All link should be put in sniff
// immediately
void LMP::tmpSuspendCurPiconetForPageScan()
{
    if (!curPico) {
	return;
    }
#if 1
    LMPLink *link = curPico->activeLink;
    int numLink = curPico->numActiveLink;
    for (int i = 0; i < numLink; i++) {
	if (link->_in_sniff) {
	    link->handle_sniff_suspend();
	}
	link = link->next;
    }

    if (!curPico) {
	return;
    }
#endif
    if (suspendPico == NULL) {
	suspendPico = curPico;
    } else {
	curPico->next = suspendPico;
	curPico->prev = suspendPico->prev;
	suspendPico->prev->next = curPico;
	suspendPico->prev = curPico;
    }
    curPico = NULL;
    Scheduler::instance().cancel(&bb_->clk_ev_);
}

void LMP::resumeTmpSuspendMasterPico()
{
    unsuspend(masterPico);
    bb_->setPiconetParam(masterPico);
    // startMasterClk();
}

void LMP::tmpSuspendCurPiconetForPage()
{
    if (!curPico) {
	return;
    }
#if 1
    LMPLink *link = curPico->activeLink;
    int numLink = curPico->numActiveLink;
    for (int i = 0; i < numLink; i++) {
	if (link->_in_sniff) {
	    link->handle_sniff_suspend();
	}
	link = link->next;
    }

    if (!curPico) {
	return;
    }
#endif
    if (suspendPico == NULL) {
	suspendPico = curPico;
    } else {
	curPico->next = suspendPico;
	curPico->prev = suspendPico->prev;
	suspendPico->prev->next = curPico;
	suspendPico->prev = curPico;
    }
    curPico = NULL;
    Scheduler::instance().cancel(&bb_->clk_ev_);
}

double LMP::lookupWakeupTime()
{
    double t = -1;
    if (curPico) {
	t = curPico->lookupWakeupTime();
    }
    if (!suspendPico) {
	return t;
    }
    Piconet *wk = suspendPico;
    do {
	double t1 = wk->lookupWakeupTime();
	if (t1 > 0) {
	    t = (t > 0 ? MIN(t, t1) : t1);
	}
    } while ((wk = wk->next) != suspendPico);
    return t;
}

void LMP::handle_pending_act()
{
    switch (_pending_act) {
    case Page:
	if (_pagereq) {
	    _page(_pagereq->pageTO);
	}
	break;
    case Inq:
	_inquiry(inquiry_length_);
	break;
    case PicoSwitch:
	// PicoSwitch is invoked by wake-up routines only ??
	switchPiconet(nextPico);
	nextPico = 0;
	break;
    case PageScan:
	_page_scan(0);
	break;
    case NoAct:
	break;
    default:
	if (_scan_mask) {
	    HCI_Write_Scan_Enable(_scan_mask);
	}
    }
    _pending_act = None;
}

// remove pico from the suspended list, set it to curPico.
void LMP::unsuspend(Piconet * pico)
{
    if (!suspendPico) {
	return;
    }
    // remove it from suspend list.
    Piconet *wk = suspendPico;
    do {
	if (wk == pico) {
	    if (wk == suspendPico) {
		if (wk->next == wk) {	// singleton
		    suspendPico = NULL;
		} else {
		    suspendPico = suspendPico->next;
		}
	    }
	    wk->prev->next = wk->next;
	    wk->next->prev = wk->prev;
	    curPico = pico;
	    curPico->next = curPico->prev = curPico;
	    curPico->suspended = 0;
	    return;
	}
    } while ((wk = wk->next) != suspendPico);
}

// Prereq:  ACL links should be suspened at this moment.
// Steps:
//      1. generate default BTSchedWord.
//      2. started at masterPico, walk through all other piconets, when 
//              a SCO link is encountered, BTSchedWord should be updated.
BTSchedWord *LMP::_prepare_bb_signal(int m, int *numSCO)
{
    // 1. check if ACLs are suspended.
    // 2. find num of SCO exists.
    // 3. compute sched_word.

    BTSchedWord *sw;

    if (curPico && !curPico->suspended && !curPico->suspendReqed) {
	suspendCurPiconetReq();
    }

    *numSCO = 0;
    // A HV2 link counts as 2 HV3 links here.
    sw = (m ? (new BTSchedWord(2)) : (new BTSchedWord(true)));

    Piconet *pico = curPico;
    int i = 0;
    if (pico) {
	i = 1;
	*numSCO += pico->updateSchedWord(sw, m,
					 (m && masterPico
					  && curPico == masterPico));
    }
    pico = suspendPico;
    for (; i < numPico(); i++) {
	*numSCO += pico->updateSchedWord(sw, m,
					 (m && masterPico
					  && curPico == masterPico));
	pico = pico->next;
    }
    return sw;
}

// Normally Link suspension is a two-way negotiation between the two ends.
// But sometime, these negotiations didn't occur, so we force it on hold
// one way, so the new request can be processed.
void LMP::forceCurPicoOnHold(int ht)
{
    if (!curPico) {
	return;
    }
    LMPLink *wk = curPico->activeLink;
    int numlink = curPico->numActiveLink;

    for (int i = 0; i < numlink; i++) {
	if (!wk->suspended) {
	    wk->force_oneway_hold(ht);
	}
	wk = wk->next;
    }
    _bb_cancel();
}

// put all link on hold (one way) immediately, then begin page process.
void LMP::force_page()
{
    forceCurPicoOnHold(100);
    _page(0);
}

// put all link on hold (one way) immediately, then begin scaning process.
void LMP::force_scan()
{
    forceCurPicoOnHold(100);
    if (_scan_mask == 1) {
	_page_scan(0);
    } else if (_scan_mask == 2) {
	_inquiry_scan(0);
    } else if (_scan_mask == 3) {
	_inquiry_scan(0);
	_page_scan(0);
    }
}

// put all link on hold (one way) immediately, then begin inquiry process.
void LMP::force_inquiry()
{
    forceCurPicoOnHold(100);
    _inquiry(inquiry_length_);
}

// length : discoverable_time
void LMP::_inquiry_scan(int discoverable_time)
{
    // check if sco exists and the spare resource
    // put acl on hold
    // set T_inquiry_scan <= 2.56s
    // set T_w_inquiry_scan: 0 SCO  16 slots
    //                       1 SCO  36 slots
    //                       2 SCO  54 slots

    Scheduler & s = Scheduler::instance();
    if (forceScan.uid_ > 0) {
	s.cancel(&forceScan);
    }
    int numSCO;
    BTSchedWord *sched_word = _prepare_bb_signal(0, &numSCO);
    bb_->inqAddr_ = giac_;
    bb_->inquiry_scan(sched_word);
}

// length: connectable_time
void LMP::_page_scan(int connectable_time)
{
    Scheduler & s = Scheduler::instance();
    if (forceScan.uid_ > 0) {
	s.cancel(&forceScan);
    }
    int numSCO;
    // prepare sched word when SCO link exists.
    BTSchedWord *sched_word = _prepare_bb_signal(0, &numSCO);
    bb_->page_scan(sched_word);
}

void LMP::_page(int length)
{
    if (bb_->state() == Baseband::PAGE) {
	return;
    }

    Scheduler & s = Scheduler::instance();
    if (forcePage.uid_ > 0) {
	s.cancel(&forcePage);
    }

    reqOutstanding = 1;

#if 0
    double now = s.clock();
    double timespan = lookupWakeupTime();
    int t = 0;

    if (timespan > now) {
	t = int ((timespan - now) / bb_->tick());
    }
#endif

    // NOTE:  length is ignored.  It is controled by pageTO_ at 
    // baseband.
    int numSCO;
    BTSchedWord *sched_word = _prepare_bb_signal(1, &numSCO);
    if (!NPage_manual_) {
	bb_->setNPage(numSCO);
    }
    if (_pagereq) {
	bb_->page(_pagereq->slave, _pagereq->sr_mode,
		  _pagereq->ps_mode, _pagereq->clock_offset, sched_word);
	_page_conn_hand = _pagereq->connhand;
    }
}

// master side
void LMP::page_complete(bd_addr_t remote, int succ)
{
    if (curPico && !curPico->isMaster()) {
	fprintf(stderr, "OOps, curPico is not a master.\n");
    }
    PageReq *curReq = _pagereq;

    if (masterPico && !masterPico->isMaster()) {
	fprintf(stderr, "OOps, masterPico is not a master.\n");
	masterPico->dump(stderr);
	dump(stderr, 1);
	dump(stdout, 1);
	abort();
    }

    assert(!(curPico && masterPico && curPico != masterPico));

    if (!curPico && masterPico) {
	unsuspend(masterPico);	// curPico = masterPico;
    }

    Piconet *pico = curPico;
    LMPLink *link = NULL;
    ConnectionHandle *connhand = _page_conn_hand;
    Bd_info *bd = lookupBdinfo(remote);

    // XXX: why this could happen?
    if (!bd) {
	bd = _add_bd_info(new Bd_info(remote, 0));
    }

    bd->lt_addr_ = bb_->slave_lt_addr_;

#ifdef BTDEBUG
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_, BTPREFIX1 "%d Paging %d %s. \n",
		bb_->bd_addr_, remote, (succ ? "succed" : "failed"));
	*/
	dump(BtStat::log_, 1);
    }
#endif

    if (!succ) {
	int continuePage = 1;

	_pagereq->cntr++;
	fprintf(stderr,
		"Warning: at %f. %d Paging %d %s %d times. To:%d.\n",
		Scheduler::instance().clock(),
		bb_->bd_addr_, remote, (succ ? "succed" : "failed"),
		_pagereq->cntr, _pagereq->pageTO);
	dump(stderr, 1);
	if (_pagereq->pageTO < 128) {
	    _pagereq->pageTO = 128;
	}

	if (_pagereq->next) {
	    _pagereq = _pagereq->next;
	    if (_pagereq->cntr < maxPageRetries_) {
		_pagereq_tail->next = curReq;
		_pagereq_tail = curReq;
		_pagereq_tail->next = NULL;
	    } else {
		delete curReq;
	    }
	} else if (_pagereq->cntr >= maxPageRetries_) {
	    delete _pagereq;
	    _pagereq = _pagereq_tail = 0;
	    continuePage = 0;
	    // return;
	}

	if (continuePage) {
	    _page(_pagereq->pageTO);
#if 0
	    if (_pagereq->cntr > 1) {
		Baseband *bb = bb_;
		while ((bb = bb->_next) != bb_) {
		    if (bb->bd_addr_ == remote) {
			bb->lmp_->HCI_Write_Page_Scan_Activity(4096, 4096);
			bb->lmp_->reqOutstanding = 1;
			bb->lmp_->HCI_Write_Scan_Enable(1);
			break;
		    }
		}
	    }
#endif
	    return;
	    //abort();
	}

    } else {

	if (pico) {
	    link = pico->add_slave(bd, this, connhand);
	} else {
	    link = add_piconet(bd, 1, connhand);
	    masterPico = link->piconet;
	    masterPico->suspended = 1;
	    pico = masterPico;
	}
	link->txBuffer->reset_seqn();

	link->piconet->slot_offset = 0;
	link->piconet->clk_offset = 0;

	if (_pagereq->next) {
	    _pagereq = _pagereq->next;
	    _page(_pagereq->pageTO);
	    delete curReq;
	    return;
	} else {

	    delete curReq;
	    _pagereq = _pagereq_tail = 0;
	    if (_pending_act == Page) {
		_pending_act = None;
	    }
	}
    }

    reqOutstanding = 0;
    int succed = 0;

    if (pico) {			// == curPico
	bb_->setPiconetParam(pico);
	LMPLink *wk = pico->activeLink;
	for (int i = 0; i < pico->numActiveLink; i++) {
	    if (!wk->connected) {
		succed = 1;
		link_setup(wk->connhand);
	    }
	    wk = wk->next;
	}
    }

    if (succed) {		// At least one new link is formed.
	pico->suspended = 0;
	// assert(pico->isMaster());
	if (!pico->isMaster()) {
	    fprintf(stderr, "OOps, not a master when page complete.\n");
	    dump(stdout, 1);
	    dump(stderr, 1);
	    abort();
	}
	// bb_->setPiconetParam(pico);

	// curPico = link->piconet;
    } else {
	// notify upper layer ?? upper layer can time out
	// handle_pending_act();
	if (!curPico) {
	    handle_pending_act();
	}
    }
}

void LMP::role_switch_bb_complete(bd_addr_t bd, int result)
{
    LMPLink *link = lookupLink(bd, 2);
    if (!link) {
	fprintf(stderr, "%d %f %s: can't find link for %d\n",
		bb_->bd_addr_, Scheduler::instance().clock(), __FUNCTION__,
		bd);
	abort();
    }

    if (result) {
	if (!link->piconet->isMaster()) {
	    _my_info->lt_addr_ = bb_->lt_addr_;
	    for (int i = 0; i < link->piconet->lt_table_len; i++) {
		link->piconet->lt_table[i] = 0;
	    }
	    link->piconet->lt_table[bb_->lt_addr_] = link;
	    link->piconet->lt_table_len =
		(bb_->lt_addr_ <
		 link->piconet->lt_table_len ? link->piconet->
		 lt_table_len : bb_->lt_addr_ + 1);

	    link->lt_addr_ = bb_->lt_addr_;
	}
	link->handle_role_switch_pico();
    } else {
	link->role_switch_tdd_failed();
    }
}

void LMP::link_setup(ConnectionHandle * conn)
{
    lmpCommand(LMP_HOST_CONNECTION_REQ, conn->link);
}

void LMP::changePktType(LMPLink * link)
{
    if (link->event != LMPLink::Connect) {
	link->event = LMPLink::ChangPktType;
	negotiate_link_param(link);
    }
}

/* negotiate
   1. packet type
   2. QoS
   3. Link encryption
   */
void LMP::negotiate_link_param(LMPLink * link)
{
    uchar slotnum;
    if ((slotnum = hdr_bt::slot_num(link->connhand->packet_type)) !=
	hdr_bt::slot_num(link->pt)) {
	lmpCommand(LMP_MAX_SLOT_REQ, slotnum, link);
	return;
    }
    if (link->connhand->packet_type != link->pt) {
	link->pt = (hdr_bt::packet_type) link->connhand->packet_type;
    }

    if ((slotnum = hdr_bt::slot_num(link->connhand->recv_packet_type)) !=
	hdr_bt::slot_num(link->rpt)) {
	lmpCommand(LMP_MAX_SLOT, slotnum, link);
	if (slotnum == 5) {
	    link->rpt = _defaultPktType5slot;
	} else if (slotnum == 3) {
	    link->rpt = _defaultPktType3slot;
	} else {
	    link->rpt = _defaultPktType1slot;
	}
	schedule_set_schedWord(link);
    }
    if (link->connhand->recv_packet_type != link->rpt) {
	link->rpt = (hdr_bt::packet_type) link->connhand->recv_packet_type;
    }

    if (link->connhand->chan && link->connhand->chan->_qosReq) {
	link->connhand->chan->_qos = link->connhand->chan->_qosReq;
	link->connhand->chan->_qosReq = NULL;
	QosParam *qos = new QosParam(*link->connhand->chan->_qos);
	L2CAPChannel *ch = link->connhand->chan->linknext;
	for (int i = 1; i < link->connhand->numChan; i++) {
	    if (ch->_qos) {
		qos->Token_Rate += ch->_qos->Token_Rate;
		qos->Peak_Bandwidth = (qos->Peak_Bandwidth >
				       ch->_qos->Peak_Bandwidth ?
				       qos->Peak_Bandwidth : ch->_qos->
				       Peak_Bandwidth);
		qos->Latency =
		    (qos->Latency <
		     ch->_qos->Latency ? qos->Latency : ch->_qos->Latency);
	    }
	    ch = ch->linknext;
	}

	HCI_Qos_Setup(link->connhand, qos->Flags,
		      qos->Service_Type, qos->Token_Rate,
		      qos->Peak_Bandwidth, qos->Latency,
		      qos->Delay_Variation);
	return;
    }

    switch (link->event) {
    case LMPLink::Connect:
	if (!tmpPico_ && link->connhand->chan) {
	    l2cap_->connection_complete_event(link->connhand, 0, 1);
	}
	// link->send_conn_ev = 0;
	link->event = LMPLink::None;
	break;
    case LMPLink::ChangPktType:
	link->event = LMPLink::None;
	break;
    default:
	break;
    }
}

void LMP::link_setup_complete(LMPLink * link)
{
    // link->send_conn_ev = 1;
    link->event = LMPLink::Connect;
    link->connhand->ready_ = 1;
    negotiate_link_param(link);
}

void LMP::unpark_req(int tick)
{
}

// Consequence of page scan
void LMP::connection_ind(bd_addr_t bd_addr_, int lt_addr_, int clk_offset,
			 int slot_offset)
{
    reqOutstanding = 0;

/*
    if (bb_->bd_addr_ == 167) {
	fprintf(stderr, ">>>> 167-%d %f %s\n", bd_addr_,
		Scheduler::instance().clock(), __FUNCTION__);
    }
*/

    Bd_info *bd = lookupBdinfo(bd_addr_);
    if (!bd) {
	bd = _add_bd_info(new Bd_info(bd_addr_, 0, clk_offset));
    } else {
	bd->offset_ = clk_offset;
    }

    _my_info->lt_addr_ = lt_addr_;

    hdr_bt::packet_type pt = hdr_bt::DH1;
    hdr_bt::packet_type rpt = hdr_bt::DH1;
    ConnectionHandle *connhand = new ConnectionHandle(pt, rpt);

    LMPLink *link = add_piconet(bd, 0, connhand);	// I'm slave
    // connhand->setLink(link);

    link->piconet->clk_offset = clk_offset;
    link->piconet->slot_offset = slot_offset;
    link->txBuffer->reset_seqn();

    bb_->setPiconetParam(link->piconet);

    if (role_ == MASTER) {
	HCI_Switch_Role(bd_addr_, 0x00);	//  becomes master
    }
}

int LMP::addInqCallback(Handler * c)
{
    if (_num_inq_callback == MAX_INQ_CALLBACK) {
	fprintf(stderr, "%d InqCallbackQ is full.\n", bb_->bd_addr_);
	return 0;
    }
    _num_inq_callback++;
    _callback_after_inq[_inq_callback_ind++] = c;
    if (_inq_callback_ind == MAX_INQ_CALLBACK) {
	_inq_callback_ind = 0;
    }
    return 1;
}

void LMP::inquiry_complete(int num)
{
    reqOutstanding = 0;

    if (_num_inq_callback > 0) {
	while (_num_inq_callback > 0) {

	    // side effect may not be desirable always.  Thank Guanhua Yan 
	    // <ghyan@lanl.gov> for spotting negative _num_inq_callback in 
	    // old code.
	    _num_inq_callback--;

	    _inq_callback_ind =
		(_inq_callback_ind ==
		 0 ? MAX_INQ_CALLBACK - 1 : _inq_callback_ind - 1);
	    _callback_after_inq[_inq_callback_ind]->handle(&intr_);
	}
    } else if (scan_after_inq_) {
	handle_pending_act();	// if nothing pending, perform scaning.
    }
}

Bd_info *LMP::getNeighborList(int *num)
{
    double now = Scheduler::instance().clock();
    Bd_info *wk = _bd;
    Bd_info *nb = NULL;
    // Bd_info *t;
    int cntr = 0;

    while (wk) {
	if (wk->last_seen_time_ >= 0
	    && now - wk->last_seen_time_ < nb_timeout_
	    && wk->dist_ >= 0 && wk->dist_ < nb_dist_) {
	    nb = new Bd_info(*wk, nb);
	    cntr++;
	}
	wk = wk->next_;
    }
    *num = cntr;
    return nb;
}

void LMP::destroyNeighborList(Bd_info * nb)
{
    Bd_info *wk = nb, *t;
    while (wk) {
	t = wk;
	wk = wk->next_;
	delete t;
    }
}

void LMP::_bb_cancel()
{
    bb_->page_scan_cancel();
    bb_->inquiryScan_cancel();
    _page_cancel();
    _inquiry_cancel();
}

void LMP::_inquiry_cancel()
{
    bb_->inquiry_cancel();
}

void LMP::_page_cancel()
{
    bb_->page_cancel();
}

void LMP::_inquiry(int length)
{
    Scheduler & s = Scheduler::instance();
    if (forceInquiry.uid_ > 0) {
	s.cancel(&forceInquiry);
    }
    _bb_cancel();
    int numSCO;
    BTSchedWord *sched_word = _prepare_bb_signal(1, &numSCO);

#if 0
    // if (numSCO >= 0) {
    if (numSCO >= 0) {
	fprintf(stderr, "*** %d %f %s: numHV3:%d -- ",
		bb_->bd_addr_, s.clock(), __FUNCTION__, numSCO);
	sched_word->dump(stderr);
    }
#endif

    if (!NInqury_manual_) {
	bb_->setNInquiry(numSCO);
    }
    bb_->inquiry(giac_, length, inq_num_responses_, sched_word);

    _inq_time = Scheduler::instance().clock();
    if (_pending_act == Inq) {
	_pending_act = NoAct;
    }
}

// BUG: should put existing piconet on hold first
// unit of xxx_length is 1.28 s
void LMP::HCI_Inquiry(int lap, int inquiry_length, int num_responses)
{
    reqOutstanding = 1;
    giac_ = lap;
    inq_num_responses_ = num_responses;
    inquiry_length_ = inquiry_length * INQUIRYHCIUNIT;	// 4096
    if (curPico && !curPico->suspended && !curPico->suspendReqed) {
	_pending_act = Inq;
	if (!suspendCurPiconetReq(inquiry_length * 2048)) {
	    Scheduler & s = Scheduler::instance();
	    if (forceInquiry.uid_ <= 0) {
		s.schedule(&_timer, &forceInquiry,
			   inqStartTO_ * bb_->slotTime());
	    }
	}
    } else {
	_inquiry(inquiry_length_);
    }
}

uchar LMP::HCI_Inquiry_Cancel()
{
    // bb_->inquiry_cancel();
    _inquiry_cancel();
    return 0;
}

// unit of xxx_length is 1.28 s
uchar LMP::HCI_Periodic_Inquiry_Mode(int max_period_length,
				     int min_period_length, int lap,
				     int inquiry_length, int num_responses)
{
    inq_max_period_length_ = max_period_length;
    inq_min_period_length_ = min_period_length;
    inquiry_length_ = inquiry_length * INQUIRYHCIUNIT;	// 4096
    inq_num_responses_ = num_responses;
    giac_ = lap;

    if (inq_periodic_) {	// Already in state
	return 0;
    }
    handle_PeriodicInq();

    return 0;
}

void LMP::handle_PeriodicInq()
{
    _inquiry(inquiry_length_);
    if (inq_periodic_) {
	double t =
	    (inq_min_period_length_ +
	     Random::integer(inq_max_period_length_ -
			     inq_min_period_length_)) *
	    INQUIRYHCIUNIT * BTSlotTime / 2;
	Scheduler::instance().schedule(&_timer, &periodInq_ev, t);
    }
}

uchar LMP::HCI_Exit_Periodic_Inquiry_Mode()
{
    inq_periodic_ = 0;
    return HCI_Inquiry_Cancel();
}

ConnectionHandle *LMP::HCI_Create_Connection(bd_addr_t bd,
					     uint16_t packet_type,
					     uint8_t
					     page_scan_repetition_mode,
					     uint8_t page_scan_mode,
					     //int16_t clock_offset,
					     int32_t clock_offset,
					     uint8_t allow_role_switch)
{
#ifdef BTDEBUG
    fprintf(BtStat::log_, BTPREFIX1 "%d cr-conn %d.\n", bb_->bd_addr_, bd);
    dump(BtStat::log_, 1);
#endif
    if (masterPico && !masterPico->isMaster()) {
	fprintf(stderr, "OOps, %s masterPico is not a master. "
		"The inconsistant state is probably caused by "
		"an incomplete Role Switch.\n", __FUNCTION__);
	masterPico->dump(stderr);
	dump(stderr, 1);
	dump(stdout, 1);
	abort();
    }

    LMPLink *lk = lookupLink(bd);
    if (lk) {
	if (lk->piconet->isMaster()) {
	    return lk->connhand;
	} else {
	    fprintf(BtStat::log_,
		    BTPREFIX1 "ACL exists, i'm slave. rs:%d\n",
		    allow_role_switch);
	    if (!allow_role_switch) {	// not ok being a slave
		HCI_Switch_Role(bd, 0);	// try to become master
		lk->connhand->ready_ = 0;
	    }
	    return lk->connhand;
	}
    }
#if 0
    if ((lk = lookupLink(bd, 0))) {	// bd is already a slave.
	return lk->connhand;
    } else if ((lk = lookupLink(bd, 1))) {	// I'm already a slave to bd.
	fprintf(BtStat::log_,
		BTPREFIX1 "ACL exists, i'm slave. rs:%d\n",
		allow_role_switch);
	if (!allow_role_switch) {	// not ok being a slave
	    HCI_Switch_Role(bd, 0);	// try to become master
	    lk->connhand->ready_ = 0;
	}
	return lk->connhand;
    }
#endif

    if (packet_type == 0) {
	packet_type = defaultPktType_;
    }
    ConnectionHandle *connhand = new ConnectionHandle(packet_type,
						      allow_role_switch);
    connhand->recv_packet_type = defaultRecvPktType_;

    Bd_info *bdinfo = lookupBdinfo(bd);
    if (!bdinfo) {
	bdinfo =
	    _add_bd_info(new
			 Bd_info(bd, page_scan_repetition_mode, 0,
				 page_scan_mode, clock_offset));
	// bdinfo = _add_bd_info(bdinfo);
    }
    // bdinfo->dump();

    if (clock_offset != bdinfo->offset_) {
	fprintf(BtStat::log_,
		BTPREFIX1
		"*** WARNING: clock_offset:%d %d doesn't match\n",
		bdinfo->offset_, clock_offset);
	clock_offset = bdinfo->offset_;
    }

    PageReq *pagereq =
	new PageReq(bd, page_scan_repetition_mode, page_scan_mode,
		    clock_offset, connhand);
    pagereq->pageTO = bb_->pageTO_;

    if (!_pagereq_tail) {
	_pagereq_tail = _pagereq = pagereq;
    } else {
	_pagereq_tail->next = pagereq;
	_pagereq_tail = pagereq;
	return connhand;	// PageReq is queued. Return to the caller.
    }

    if (curPico && !curPico->suspended && !curPico->suspendReqed) {
#if 0
	if (_pending_act != None && _pending_act != NoAct) {
	    fprintf(stderr, "*** Err: %s: request %d is pending.\n",
		    __FUNCTION__, _pending_act);
	    exit(-1);
	} else {
	    _pending_act = Page;
	}
#else
	_pending_act = Page;
#endif
	if (!suspendCurPiconetReq()) {
	    Scheduler & s = Scheduler::instance();
	    if (forcePage.uid_ <= 0) {
		s.schedule(&_timer, &forcePage,
			   pageStartTO_ * bb_->slotTime());
	    }
	    return connhand;
	}
    }

    _bb_cancel();
    _page(_pagereq->pageTO);

    // In reality, connhand is returned in the command complete event.
    // Here we return it immediately, but a flag will be set when the
    // connection is complete.
    return connhand;
}

void LMP::remove_page_req()
{
    PageReq *tmp;
    while (_pagereq) {
	tmp = _pagereq;
	_pagereq = _pagereq->next;
	delete tmp;
    }
    _pagereq_tail = _pagereq = NULL;
}

// 1a. master wait for 6 T_poll
// 1b. master wait for 3 T_poll, or
// 2. wait T_linksupervisiontimeout
void LMP::recv_detach(LMPLink * link, uint8_t reason)
{
/*
    fprintf(stderr, "I'm gonna detach link to %d\n",
	    link->remote->bd_addr_);
*/
    // notify upper layer at this moment??

    link->tobedetached = 1;
    int npoll = (link->piconet->isMaster()? 6 : 3);
    Scheduler & s = Scheduler::instance();
    if (link->ev) {
	s.cancel(link->ev);
	delete link->ev;
    }
    link->ev = new LMPEvent(link, LMPEvent::Detach);
    link->ev->reason = reason;
    s.schedule(&_timer, link->ev, link->T_poll_ * npoll * bb_->slotTime());
}

// 1. pause ACL-U
// 2. send LMP_DETACH
// 3. start a timer with 6 T_poll to wait a ACK
// 3a. wait 3 T_poll, if received ACK, or
// 3b. wait T_linksupervisiontimeout
//
// If in Hold,Sniff, Park state, need to exit these state first, or
// wait for LMP response timeout (30 sec)
void LMP::HCI_Disconnect(ConnectionHandle * connhand, uint8_t reason)
{
    if (!connhand->link) {
	fprintf(stderr, "%s: %d Link is not present.\n",
		__FUNCTION__, bb_->bd_addr_);
	abort();
    }
/*
    // assert(!connhand->link->suspended);
    if (connhand->link->suspended) {
	fprintf(stderr, "%s: %d Link is suspended.\n",
		__FUNCTION__, bb_->bd_addr_);
	abort();
    }
*/

    if (connhand->link->acl) {	// SCO connection
	LMPLink::SCODisconnReq req(connhand->link->sco_hand, reason);

	// Which link should we send this request over?
	//   the SCO link or the ACL link?
	lmpCommand(LMP_REMOVE_SCO_LINK_REQ, (uchar *) & req, sizeof(req),
		   2, connhand->link);
	connhand->link->tobedetached = 1;	// stop traffic
	connhand->link->sco_remv = connhand->link;
	connhand->link->acl->sco_remv = connhand->link;
	connhand->link->disconnReason = reason;
	// TODO: start a timer to force disconnection.
	return;
    }

    lmpCommand(LMP_DETACH, &reason, 1, connhand->link);

    connhand->link->tobedetached = 1;
    connhand->link->ev =
	new LMPEvent(connhand->link, LMPEvent::DetachFirstTimer);
    Scheduler::instance().schedule(&_timer, connhand->link->ev,
				   connhand->link->T_poll_ * 6 *
				   bb_->slotTime());

    // _detach(connhand->link, reason);
}

int LMP::_sco_req_pending(bd_addr_t addr)
{
    ScoAgent *wk = _agent;
    while (wk) {
	if (wk->daddr == addr) {
	    return 1;
	}
	wk = wk->lnext;
    }
    return 0;
}

int LMP::addReqAgent(ScoAgent * ag)
{
    ScoAgent *wk = _agent;
    while (wk) {
	if (wk->daddr == ag->daddr) {
	    return 0;
	}
	wk = wk->lnext;
    }
    ag->lnext = _agent;
    _agent = ag;
    return 1;
}

ScoAgent *LMP::removeReqAgent(bd_addr_t ad)
{
    ScoAgent *ret = NULL;
    if (!_agent) {
	return NULL;
    }
    if (ad == _agent->daddr) {
	ret = _agent;
	_agent = _agent->lnext;
	return ret;
    }
    ScoAgent *wk = _agent->lnext;
    ScoAgent *par = _agent;
    while (wk) {
	if (wk->daddr == ad) {
	    par->lnext = wk->lnext;
	    return wk;
	}
	par = wk;
	wk = wk->lnext;
    }
    return NULL;
}

// remove reqAgent
// send connection complete event to ScoAgent
void LMP::add_Sco_link_complete(LMPEvent * le)
{
    if (!_agent) {		// bogus request.  Probably because msg replayed.
	return;
    }
    bb_->set_sched_word(le->sched_word);
    le->link->acl->piconet->add_sco_table(le->link->D_sco,
					  le->link->T_poll_, le->link);
    bb_->setScoLTtable(le->link->acl->piconet->sco_table);
    ScoAgent *agent = removeReqAgent(le->link->remote->bd_addr_);
    if (agent) {
	agent->connection_complete_event(le->link->connhand);
	le->link->connhand->agent = agent;
    } else {
	fprintf(stderr, "*** %d @%f bogus SCO req: Message is replayed?\n",
		bb_->bd_addr_, Scheduler::instance().clock());
    }
    if (trace_state()) {
	fprintf(stdout,
		"TurnOnSCOLink: %d dsco %d tsco %d clk:%d %f t_clk_00_ %f\n",
		_my_info->bd_addr_, le->link->D_sco, le->link->T_poll_,
		bb_->clk_, Scheduler::instance().clock(), bb_->t_clk_00_);
    }
}

// schedule activating link
void LMP::_allocate_Sco_link(LMPLink * link)
{
    if (!link->sco_req) {	// bogus request.  Probably msg is replayed.
	return;
    }
    hdr_bt::packet_type pt = (link->sco_req->T_sco == 6 ? hdr_bt::HV3 :
			      (link->sco_req->T_sco ==
			       4 ? hdr_bt::HV2 : hdr_bt::HV1));
    if (!link->connhand->reqScoHand) {
	link->connhand->reqScoHand = new ConnectionHandle(pt);
    }
    ConnectionHandle *connh = link->connhand->reqScoHand;
    connh->sco_hand = link->sco_req->connhand;

    LMPLink *scolink = new LMPLink(link, this, connh, SCO);
    scolink->pt = pt;
    link->piconet->addScoLink(scolink);
    connh->setLink(scolink);

#if 0
    clk_t clk_base = bb_->clk_ & 0xffffff00;
    if (((clk_base >> 8) & 0x01) != (link->sco_req->flag & 0x01)) {
	if (((clk_base >> 26) & 0x01) !=
	    ((link->sco_req->flag >> 1) & 0x01)) {
	    clk_base += (0x01 << 26);
	}
	clk_base -= (0x01 << 8);
    }

    clk_t clk = clk_base + (link->sco_req->D_sco << 1);
#endif


    // scolink->D_sco = (clk >> 1) % 6;
    scolink->D_sco = link->sco_req->D_sco;
    scolink->T_poll_ = link->sco_req->T_sco;
    scolink->sco_hand = link->sco_req->connhand;
    // link->add_sco_table(scolink->D_sco, scolink->T_poll_, scolink);

    clk_t block_b = bb_->clk_ - (bb_->clk_ % 24);
    clk_t clk = block_b + scolink->D_sco * 2;

    double t =
	bb_->t_clk_00_ + (clk - (bb_->clk_ / 4 * 4)) * bb_->tick() -
	Scheduler::instance().clock();
    // t -= bb_->tick() * 4;
    // t -= 1e-7;                       // ??
    while (t < 0) {
	t += link->sco_req->T_sco * bb_->slotTime();
    }

    link->piconet->compute_sched();
    if (trace_state()) {
	link->piconet->_sched_word->dump();
    }
    LMPEvent *e = new LMPEvent(link->piconet->_sched_word, scolink);
    Scheduler::instance().schedule(&_timer, e, t);
}

void LMP::_remove_Sco_link(LMPLink * link)
{
    if (link->piconet->numScoLink == 1) {
	if (scoPico == link->piconet) {
	    scoPico = scoPico1;
	} else {
	    scoPico1 = 0;
	}
    }
    link->piconet->removeScoLink(link);
    bb_->freeTxBuffer(link->txBuffer);
    bb_->setScoLTtable(link->piconet->sco_table);
    link->piconet->setSchedWord();
    if (link->connhand && link->connhand->agent) {
	link->connhand->agent->linkDetached();
    }
    link->acl->removeScoLink(link);
}

// packet_type: HV1/HV2/HV3
ConnectionHandle *LMP::HCI_Add_SCO_Connection(ConnectionHandle * connhand,
					      hdr_bt::
					      packet_type packet_type)
{
    if (connhand->reqScoHand) {	// Add_SCO req pending. can't handle it.
	return NULL;
    }
    connhand->reqScoHand = new ConnectionHandle(packet_type);
    if (!connhand->ready_) {
	return connhand->reqScoHand;
    }
    return _add_sco_connection(connhand);
}

ConnectionHandle *LMP::_add_sco_connection(ConnectionHandle * connhand)
{
    connhand->ready_ = 1;
    hdr_bt::packet_type packet_type = (hdr_bt::packet_type)
	connhand->reqScoHand->packet_type;

    uchar T_sco;
    uchar D_sco;
    int sco_hand;

    if (packet_type == hdr_bt::HV1) {
	T_sco = 2;
    } else if (packet_type == hdr_bt::HV2) {
	T_sco = 4;
    } else {
	T_sco = 6;
    }

    if (!connhand->link->piconet->isMaster()) {

	LMPLink::SCOReq req(0, 0, 0, T_sco, packet_type, 0);
	lmpCommand(LMP_SCO_LINK_REQ, (uchar *) & req,
		   sizeof(LMPLink::SCOReq), connhand->link);
	return connhand->reqScoHand;
    }
    // check capacity

    double res_req;

    res_req = 2.0 / T_sco;
    connhand->link->piconet->compute_sched();

    // BUG: current method of computing _res makes T_sco == 2 impossible.
    // Anyway, this is not very interest.
    if (res_req > connhand->link->piconet->_res) {
	printf("No enough resource available for SCO connection.\n");
	delete connhand->reqScoHand;
	connhand->reqScoHand = 0;
	return NULL;
    }
    LMPLink *wk = connhand->link->piconet->activeLink;
    for (int i = 0; i < connhand->link->piconet->numActiveLink; i++) {
	if (hdr_bt::slot_num(wk->connhand->packet_type) >
	    (T_sco == 4 ? 1 : 3)) {
	    printf("Packet type conflict.\n");
	    delete connhand->reqScoHand;
	    connhand->reqScoHand = 0;
	    return NULL;
	}
	wk = wk->next;
    }

    // negotiating

    uchar flag;
    D_sco = connhand->link->piconet->_getDsco();
    sco_hand = connhand->link->piconet->_getScoHand();
    connhand->reqScoHand->sco_hand = sco_hand;	// ??
    if (connhand->link->sco_req) {
	delete connhand->link->sco_req;
    }
    connhand->link->sco_req =
	new LMPLink::SCOReq(sco_hand, flag, D_sco, T_sco, packet_type, 0);
    lmpCommand(LMP_SCO_LINK_REQ, (uchar *) connhand->link->sco_req,
	       sizeof(LMPLink::SCOReq), connhand->link);
    return connhand->reqScoHand;
}

// after get conn req event
void LMP::HCI_Accept_Connection_Request(bd_addr_t bd_addr_, uint8_t role)
{
    LMPLink *link = lookupLink(bd_addr_, 2);
    if (role == 0) {		// become the master for this connection. MS switch
	HCI_Switch_Role(bd_addr_, role);
    } else if (role == 1) {	//remain the slave, no MS switch
	lmpCommand(LMP_ACCEPTED, LMP_HOST_CONNECTION_REQ, link);
    }
}

void LMP::HCI_Reject_Connection_Request(bd_addr_t bd_addr_, uint8_t reason)
{
    LMPLink *link = lookupLink(bd_addr_, 2);
    uchar content[2];
    content[0] = LMP_HOST_CONNECTION_REQ;
    content[1] = reason;
    lmpCommand(LMP_NOT_ACCEPTED, content, 2, link);
}

void LMP::HCI_Qos_Setup(ConnectionHandle * connhand, uint8_t Flags,
			uint8_t Service_Type, int Token_Rate,
			int Peak_Bandwidth, int Latency,
			int Delay_Variation)
{
    uchar N_bc = 3;		// No. repetition for broadcast
    short int P_max = 9999;
    if (Latency > 0) {
	P_max = Latency / 625;
    }
    int pktsize = hdr_bt::payload_size(connhand->packet_type) * 8;
    int pktslot = hdr_bt::slot_num(connhand->packet_type);
    int rpktslot = hdr_bt::slot_num(connhand->recv_packet_type);
    int slotsPerSec = 1000000 / 625;
    // int maxRate = pktsize * (slotsPerSec / (pktslot + 1));

    // Rate = pktsize * slotsPerSec / T_poll
    short int T_poll = (short int) ((pktsize * slotsPerSec) / Token_Rate);
    if (T_poll & 0x01) {
	T_poll--;
    }

    connhand->link->piconet->compute_sched();
    if (T_poll > P_max) {
	printf("Latency cannot be satisfied.\n");
    } else if (T_poll < 2
	       || 1.0 * (pktslot + rpktslot) / T_poll <
	       connhand->link->piconet->_res) {
	printf("Token_Rate cannot be satisfied.\n");
    } else {
	connhand->link->res_req = 1.0 * (pktslot + rpktslot) / T_poll;
	connhand->link->T_poll_req = T_poll;
	connhand->link->N_bc_req = N_bc;

	uchar content[3];
	*(short int *) content = T_poll;
	content[2] = N_bc;
	lmpCommand(LMP_QOS_REQ, content, 3, connhand->link);
    }
}

void LMP::send_slot_offset(LMPLink * link)
{
    if (!link->piconet->isMaster()) {
	int offset = (int) ((bb_->t_clkn_00_ - bb_->t_clk_00_) * 1E6);
	if (offset < 0) {
	    offset += 1250;
	}
	LMPLink::SlotOffset req(offset, bb_->bd_addr_);
	lmpCommand(LMP_SLOT_OFFSET, (uchar *) & req, sizeof(req), 8, link);
    }
}

// role: 0x00 i becomes master
//       0x01 i becomes slave
void LMP::HCI_Switch_Role(bd_addr_t bd_addr_, int role)
{
    LMPLink *link = lookupLink(bd_addr_, 2);
    if (!link) {
	fprintf(stderr, "Error: %d %f %s: link to %d doesn't exist.\n",
		bb_->bd_addr_, Scheduler::instance().clock(),
		__FUNCTION__, bd_addr_);
	abort();
    }

    if (role != link->piconet->isMaster()) {	// Its Role is the same as req'd
	return;
    }

    send_slot_offset(link);	// Slave only

    uint32_t instant = MAX(link->T_poll_ * 2, 32);
    if (takeover_) {
	instant = MAX(instant, (uint32_t) link->T_poll_ * 7);
	if (link->piconet->isMaster()) {
	    link->send_slave_info();
	} else {
	    lmpCommand(LMP_SLAVE_INFO_REQ, link);
	}
    }

    Scheduler & s = Scheduler::instance();
    instant = (bb_->clk_ & 0xFFFFFFFC) + 4 + instant * 2;
    double t = (instant - (bb_->clk_ & 0xFFFFFFFC)) * bb_->tick()
	+ bb_->t_clk_00_ - s.clock();

    if (trace_state()) {
	fprintf(BtStat::log_, BTPREFIX1
		"%d LMP_SWITCH:instant: %d (%f) clk:%d diff:%d\n",
		bb_->bd_addr_, instant, t + s.clock(),
		bb_->clk_, instant - bb_->clk_);
    }

    s.schedule(&_timer, (new LMPEvent(link, LMPEvent::RoleSwitch)), t);
    lmpCommand(LMP_SWITCH, (uchar *) & instant, sizeof(uint32_t), link);
}

// return role: 0 -- i'm master; 1 -- i'm slave
int LMP::HCI_Role_Discovery(ConnectionHandle * connection_handle)
{
    return !connection_handle->link->piconet->isMaster();
}

#ifdef PARK_STATE
void LMP::HCI_Park_Mode(ConnectionHandle * connhand,
			uint16_t Beacon_Max_Interval,
			uint16_t Beacon_Min_Interval)
{
    connhand->link->beacon_Max_Interval = Beacon_Max_Interval;
    connhand->link->beacon_Min_Interval = Beacon_Min_Interval;
    connhand->link->request_park();
}

void LMP::HCI_Exit_Park_Mode(ConnectionHandle * connhand)
{
    connhand->link->request_unpark();
}
#endif

void LMP::HCI_Hold_Mode(ConnectionHandle * connhand,
			uint16_t Hold_Mode_Max_Interval,
			uint16_t Hold_Mode_Min_Interval)
{
    connhand->link->hold_Mode_Max_Interval = Hold_Mode_Max_Interval;
    connhand->link->hold_Mode_Min_Interval = Hold_Mode_Min_Interval;
    connhand->link->request_hold();
}

void LMP::HCI_Sniff_Mode(ConnectionHandle * connhand,
			 uint16_t Sniff_Max_Interval,
			 uint16_t Sniff_Min_Interval,
			 uint16_t Sniff_Attempt, uint16_t Sniff_Timeout)
{
    connhand->link->sniff_Max_Interval = Sniff_Max_Interval;
    connhand->link->sniff_Min_Interval = Sniff_Min_Interval;
    connhand->link->sniff_Attempt = Sniff_Attempt;
    connhand->link->sniff_Timeout = Sniff_Timeout;
    connhand->link->request_sniff();
}

void LMP::HCI_Exit_Sniff_Mode(ConnectionHandle * connhand)
{
    connhand->link->request_unsniff();
}

int LMP::HCI_Change_Local_Name(char *Name)	// 248 bytes, null-terminated
{
    _name = new char[strlen(Name) + 1];
    strcpy(_name, Name);
    return 0;
}

char *LMP::HCI_Read_Local_Name()
{
    return _name;
}

int LMP::HCI_Read_Num_Broadcast_Retransmissions()
{
    return _num_Broadcast_Retran;
}

int LMP::
HCI_Write_Num_Broadcast_Retransmissions(uint8_t Num_Broadcast_Retran)
{
    _num_Broadcast_Retran = Num_Broadcast_Retran;
    return 0;
}

void LMP::HCI_Change_Connection_Packet_Type(ConnectionHandle * connhand,
					    hdr_bt::packet_type ptype)
{
    // How to change receiving packet type ??

    if (hdr_bt::slot_num(ptype) != hdr_bt::slot_num(connhand->link->pt)) {
	uchar slotnum = hdr_bt::slot_num(ptype);
	lmpCommand(LMP_MAX_SLOT_REQ, slotnum, connhand->link);

    } else {
	connhand->link->pt = ptype;
    }
}

int LMP::HCI_Write_Page_Scan_Activity(uint16_t PS_interval,
				      uint16_t PS_window)
{
    bb_->T_page_scan_ = PS_interval;
    bb_->T_w_page_scan_ = PS_window;
    return 0;
}

int LMP::HCI_Read_Page_Scan_Activity(uint16_t * PS_interval,
				     uint16_t * PS_window)
{
    *PS_interval = bb_->T_page_scan_;
    *PS_window = bb_->T_w_page_scan_;
    return 0;
}

int LMP::HCI_Write_Inquiry_Scan_Activity(uint16_t intv, uint16_t wind)
{
    bb_->T_inquiry_scan_ = intv;
    bb_->T_w_inquiry_scan_ = wind;
    return 1;
}

int LMP::HCI_Read_Inquiry_Scan_Activity(uint16_t * intv, uint16_t * wind)
{
    *intv = bb_->T_inquiry_scan_;
    *wind = bb_->T_w_inquiry_scan_;
    return 1;
}

int LMP::HCI_Write_Scan_Enable(uint8_t w)
{
    _scan_mask = w;
    if (w < 1 || w > 3) {
	return 0;
    }

    if (curPico && !curPico->suspended && !curPico->suspendReqed) {
	if (_pending_act == None || _pending_act == NoAct ||
	    _pending_act == PageScan || _pending_act == Scan) {
	    if (w == 1) {
		_pending_act = PageScan;
	    } else {
		_pending_act = Scan;
	    }
	} else {
	    fprintf(stderr, "*** warning: %s: request %d is pending.\n",
		    __FUNCTION__, _pending_act);
	    // exit(-1);
	    if (w == 1) {
		_pending_act = PageScan;
	    } else {
		_pending_act = Scan;
	    }
	}
	if (!suspendCurPiconetReq()) {
	    Scheduler & s = Scheduler::instance();
	    if (forceScan.uid_ <= 0) {
		s.schedule(&_timer, &forceScan,
			   scanStartTO_ * bb_->slotTime());
	    }
	    return 0;
	}
    }

    _bb_cancel();
    if (w == 1) {		// page scan only
	_page_scan(0);

    } else if (w == 2) {	// inquiry scan only
	_inquiry_scan(0);

    } else if (w == 3) {	// page scan + inquiry scan
	if (bb_->T_w_page_scan_ >= bb_->T_page_scan_) {
	    fprintf(stderr,
		    "Warning: T_w_page_scan_(%d) >= T_page_scan_(%d).\n",
		    bb_->T_w_page_scan_, bb_->T_page_scan_);
	}
	if (bb_->T_w_inquiry_scan_ >= bb_->T_inquiry_scan_) {
	    fprintf
		(stderr,
		 "Warning: T_w_inquiry_scan_(%d) >= T_inquiry_scan_(%d).\n",
		 bb_->T_w_inquiry_scan_, bb_->T_inquiry_scan_);
	}
	_inquiry_scan(0);
	_page_scan(0);
    } else {
	return 0;
    }
    return 1;
}

uint8_t LMP::HCI_Read_Scan_Enable()
{
    uint8_t disc = (bb_->discoverable_ != 0);
    uint8_t conn = (bb_->connectable_ ? 2 : 0);
    return disc | conn;
}

//////////////////////////////////////////////////
//      Not implemented                         //
//////////////////////////////////////////////////
void LMP::HCI_Remote_Name_Request(bd_addr_t bd_addr_,
				  uint8_t page_scan_repetition_mode,
				  uint8_t page_scan_mode,
				  uint16_t clock_offset)
{
}

void LMP::HCI_Read_Remote_Supported_Features(ConnectionHandle * connhand)
{
}

void LMP::HCI_Read_Remote_Version_Information(ConnectionHandle * connhand)
{
}

void LMP::HCI_Read_Clock_Offset(ConnectionHandle * connhand)
{
}

int LMP::HCI_Read_Link_Policy_Settings(ConnectionHandle * connhand)
{
    return 0;
}

int LMP::HCI_Write_Link_Policy_Settings(ConnectionHandle * connhand,
					uint16_t Link_Policy_Settings)
{
    return 0;
}

int LMP::HCI_Set_Event_Mask(uint64_t Event_Mask)
{
    return 0;
}

int LMP::HCI_Reset()
{
    return 0;
}

int LMP::HCI_Set_Event_Filter(uint8_t Filter_Type,
			      uint8_t Condition_Type, uint64_t Condition)
{
    return 0;
}

int LMP::HCI_Flush(ConnectionHandle * connhand)
{
    return 0;
}

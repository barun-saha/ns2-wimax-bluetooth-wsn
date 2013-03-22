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

#include "scat-form-law.h"
#include "random.h"
#include "bt-node.h"
#include "bnep.h"
#include "lmp.h"
#include "lmp-link.h"
#include "lmp-piconet.h"


/*****************************************************************
* Tbis implementation follows the algorithms described in,       *
*    C. Law, AK Mehta and K.-Y. Siu, "A new Bluetooth scatternet *
*       formation protocol", Mobile Netw. Apps., vol. 8, no. 5,  *
*       Oct. 2003                                                *
*****************************************************************/

/*
 *  Note: This algorithm uses a hook called pagescan_after_inqscan_, which
 *  switches a device to page scan mode immediately after it answers a Inquiry.
 *  I'm not sure if it is realistic and I think the Spec itself is not
 *  clear about this.	If this is not realistic, the connection time may be
 *  increased.
 *
 *  The algorithm relies on the fact a leader has to know if it's
 *  slave is shared or not, that is, a bridge or non-bridge slave.
 *  Law's paper shows some _delta values:
 *     32 nodes 1.5 second
 *     64 nodes 3.2 second
 *    128 nodes 4.1 second
 */

ScatFormLaw::ScatFormLaw(BTNode * n):ScatFormator(n),
_P(SCAT_LAW_P), _delta(SCAT_LAW_DELTA), _K(MAX_SLAVE_PER_PICONET),
_term_schred(SCAT_LAW_FAIL_SCHRED),
_isLeader(true), _scanner(0), _fail_count(0), _succ(1), _numMoveAd(0)
{
    node_->bb_->N_page_ = 1;
    node_->bb_->pageTO_ = SCAT_LAW_PAGETO;
    node_->bb_->SR_mode_ = 0;
    node_->lmp_->scan_after_inq_ = 0;
}

void ScatFormLaw::fire(Event * e)
{
    // assert(e == &ev);
    if (e == &pageDelayEv_) {
	_moveBeginPage();
    } else if (e == &pageScanDelayEv_) {
	_beginPageScan();
    } else if (_status == SEEK || _status == SCAN
	       || _status == CONN_MASTER) {
	_main();
	// } else if (_status == CONN_MASTER_waitToPage) {
//      _moveBeginPage();
    } else if (_status == MERGE_wait) {
	_handleMerge_p2();
    } else if (_status == DISCONN_wait) {
	_handleDisconn_p2();
    } else if (_status == CASE3_wait) {
	_case3_p2();
    }
}

// clear the partial states of Seek and Scan
void ScatFormLaw::reset()
{
    node_->lmp_->HCI_Write_Scan_Enable(0);
    node_->lmp_->_bb_cancel();
    if (node_->lmp_->curPico) {
	node_->lmp_->curPico->suspended = 0;
	node_->bb_->setPiconetParam(node_->lmp_->curPico);
	if (node_->lmp_->curPico == node_->lmp_->masterPico &&
	    node_->bb_->clk_ev_.uid_ < 0) {
	    node_->lmp_->startMasterClk();
	}
    }
    node_->lmp_->disablePiconetSwitch_ = 0;
}

void ScatFormLaw::suspendMasterPicoForInqPage()
{
    // node_->lmp_->tmpSuspendCurPiconetForPageScan();
    // return;

    Scheduler & s = Scheduler::instance();
    // tmperorarily suspend curPico;
    if (node_->lmp_->curPico) {
	if (node_->lmp_->curPico == node_->lmp_->masterPico) {
	    node_->lmp_->curPico->suspended = 1;
	    // just disable polling slaves/bridges.
	    node_->lmp_->disablePiconetSwitch_ = 1;
	    // node_->lmp_->bb_->in_receiving_ = 0;
	    // node_->lmp_->bb_->in_transmit_ = 0;
	    node_->lmp_->bb_->trx_st_ = Baseband::TRX_OFF;
	    node_->lmp_->bb_->txSlot_ = 0;
	    s.cancel(&node_->bb_->clk_ev_);
	}
    }
}

void ScatFormLaw::_main()
{
    Scheduler & s = Scheduler::instance();

    fprintf(stdout, "*** %f %d %s\n", s.clock(), id_, __FUNCTION__);

#if 1
    if (!_isLeader && (_status == SCAN || _status == SEEK)) {
	if (_status == SCAN) {
	    // _cancel_scan();
	}
	fprintf(stdout, "*** %f %d %s: not a leader, exit _main().\n",
		s.clock(), id_, __FUNCTION__);
	reset();
	return;
    }
#endif

    if (!_succ) {
	if (_fail_count++ > _term_schred) {
	    _retire();
	    fprintf(stdout, "*** %f %d terminate.\n", s.clock(), id_);
	    fprintf(stderr, "*** %f %d terminate.\n", s.clock(), id_);
	    terminate();
	    reset();
	    return;
	}
    } else {
	_succ = 0;
	_fail_count = 0;
    }

    // schedule timeout event.
    s.schedule(&timer_, &ev_, _delta);

    if (_status == CONN_MASTER) {
	fprintf(stdout,
		"*** %f %d %s: CONN_MASTER. %d sl needed. skip this\n",
		s.clock(), id_, __FUNCTION__, _numOfSlaveToBeConnected);
	node_->lmp_->dump(stdout, 1);
	return;			// Skip this cycle?? increase _delta ??
    }
    reset();

    fprintf(stdout, "*** %f %s %d :", s.clock(), __FUNCTION__, id_);

    _t_main = s.clock();

    double p = Random::uniform();
    if (p < _P) {
	fprintf(stdout, "_seek\n");
	_seek();
    } else if (numSlave() == 0) {
	fprintf(stdout, "_scan\n");
	_scan();
    } else {
	// _cancel_scan();
	int sl = _getUnsharedSlave();

	fprintf(stdout, "tell sl %d to scan\n", sl);
	_askSlaveScan(sl, sl);

	_status = SCAN;
	_scanner = sl;
    }
}

void ScatFormLaw::_askSlaveScan(int s, int nhop)
{
    uchar cmdpl = numSlave();
    fprintf(stdout, "*** %f %d ask %d (via %d) to scan\n",
	    Scheduler::instance().clock(), id_, s, nhop);
    sendMsg(CmdScan, &cmdpl, 1, s, s);
}

void ScatFormLaw::_askToRetire(int s, int nhop)
{
    uchar cmdpl = 0;
    fprintf(stdout, "*** %f %d ask %d (via %d) to retire\n",
	    Scheduler::instance().clock(), id_, s, nhop);
    sendMsg(CmdRetire, &cmdpl, 1, s, nhop);
}

void ScatFormLaw::_askToBecomeLeader(int s, int nhop)
{
    uchar cmdpl = 0;
    fprintf(stdout, "*** %f %d ask %d (via %d) to become leader\n",
	    Scheduler::instance().clock(), id_, s, nhop);
    sendMsg(CmdLead, &cmdpl, 1, s, nhop);
}

void ScatFormLaw::_askToDisconnect(int target, int otherEnd, int nhop)
{
    fprintf(stdout, "*** %f %d ask %d (via %d) to disconnect from %d.\n",
	    Scheduler::instance().clock(), id_, target, nhop, otherEnd);
    sendMsg(CmdDisconn, (uchar *) & otherEnd, sizeof(int), target, nhop);
}

void ScatFormLaw::recv(Packet * p, int rmt)
{
    hdr_bt *bh = HDR_BT(p);
    SFmsg *msg = &bh->u.sf;
    int slot;
    int *ad;
    Scheduler & s = Scheduler::instance();

    // TODO: bridge do a 2-hop forwarding.
    if (msg->target != id_) {
	if ((slot = bnep_->findPortByIp(msg->target)) >= 0) {
	    fprintf(stdout, "*** %f %d fwd to %d\n",
		    s.clock(), id_, msg->target);
	    bnep_->_conn[slot]->cid->enque(p);
	} else {
	    fprintf(stderr,
		    "*** %f %d Don't know where to send SFcmd to %d.",
		    s.clock(), id_, msg->target);
	    Packet::free(p);
	}
	return;
    }

    assert(msg->type == type());

    switch (msg->code) {
    case CmdScan:
	fprintf(stdout, "*** %f %d recv CmdScan. %d has %d slaves\n",
		s.clock(), id_, rmt, msg->data[0]);
	_leaderMaster = rmt;
	_maNumSlave = msg->data[0];
	_scan();
	s.schedule(&timer_, &ev_, _delta * 8 / 10.0);
	break;

    case CmdInfo:
	if (_status == SEEK) {
	    SFLawSlaveInfo *info = (SFLawSlaveInfo *) msg->data;

	    fprintf(stdout,
		    "*** %f %d recv CmdInfo from %d. l:%d m:%d has %d slaves\n",
		    s.clock(), id_, rmt, info->isLeader, info->master,
		    info->MaNumSlave);
	    _connected(rmt, info->isLeader, info->master,
		       info->MaNumSlave);
	} else {
	    fprintf(stdout,
		    "*** %f %d recv CmdInfo from %d: not in SEEK *****\n",
		    s.clock(), id_, rmt);
	}
	break;

    case CmdRetire:
	fprintf(stdout, "*** %f %d recv CmdRetire from %d. \n",
		s.clock(), id_, rmt);
	_retire();
	break;

    case CmdLead:
	fprintf(stdout, "*** %f %d recv CmdLead from %d. \n",
		s.clock(), id_, rmt);
	_becomeLeader();
	break;

    case CmdMove:
	{
	    SFLawMoveInfo *info = (SFLawMoveInfo *) msg->data;
	    fprintf(stdout, "*** %f %d recv CmdMove from %d. %d slaves:",
		    s.clock(), id_, rmt, info->num);
	    _handleMove(info);
	}
	break;

    case CmdMerge:
	ad = (int *) msg->data;	// ad[0] is the target master.
	fprintf(stdout, "*** %f %d recv CmdMerge from %d via %d\n",
		s.clock(), id_, ad[0], ad[1]);
	_handleMerge(ad[0], ad[1]);
	break;

    case CmdDisconn:
	ad = (int *) msg->data;
	fprintf(stdout, "*** %f %d recv CmdDisconn (from %d)\n",
		Scheduler::instance().clock(), id_, ad[0]);
	_handleDisconn(ad[0]);
	break;

    default:
	fprintf(stdout, "*** %f %d recv unknown from %d. ******\n",
		s.clock(), id_, rmt);
	break;
    }
    Packet::free(p);
}

void ScatFormLaw::_handleMove(SFLawMoveInfo * info)
{
    Scheduler & s = Scheduler::instance();
    int i;
    for (i = 0; i < info->num; i++) {
	_moveAd[i] = info->ad[i];
	fprintf(stdout, " %d", info->ad[i]);
    }
    fprintf(stdout, "\n");
    _numMoveAd = info->num;

    // _status = CONN_MASTER_waitToPage;
    _status = CONN_MASTER;
    _numOfSlaveToBeConnected = info->num;
    _CONN_MASTER_st_t = s.clock();

    s.cancel(&pageDelayEv_);
    s.schedule(&timer_, &pageDelayEv_, SCAT_LAW_MOVE_PAGE_DELAY);
}

void ScatFormLaw::_moveBeginPage()
{
    _status = CONN_MASTER;
    // tmperorarily suspend curPico;
    suspendMasterPicoForInqPage();

    for (int i = 0; i < _numMoveAd; i++) {
	bnep_->connect(_moveAd[i]);
    }
}

void ScatFormLaw::_seek()
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);
    _status = SEEK;

    // tmperorarily suspend curPico;
    suspendMasterPicoForInqPage();

    node_->lmp_->HCI_Inquiry(node_->lmp_->giac_, int (_delta / 1.28), 1);
    node_->bb_->page_after_inq_ = 1;
}

void ScatFormLaw::_scan()
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);
    _status = SCAN;

    if (node_->lmp_->curPico) {
	node_->lmp_->tmpSuspendCurPiconetForPageScan();
	node_->lmp_->disablePiconetSwitch_ = 1;
    }

    node_->lmp_->HCI_Write_Inquiry_Scan_Activity(4096, 4096);
    node_->lmp_->HCI_Write_Scan_Enable(2);
    node_->bb_->pagescan_after_inqscan_ = 1;
    _scanner = id_;
}

void ScatFormLaw::_cancel_scan()
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);
    node_->lmp_->HCI_Write_Scan_Enable(0);
    node_->lmp_->_bb_cancel();
}

void ScatFormLaw::_beginPageScan()
{

	if (node_->lmp_->curPico) {
	    node_->lmp_->tmpSuspendCurPiconetForPageScan();
	    node_->lmp_->disablePiconetSwitch_ = 1;
	}

	node_->lmp_->HCI_Write_Page_Scan_Activity(4096, 4096);
	node_->lmp_->HCI_Write_Scan_Enable(1);

	_status = CONN_SLAVE;
	_CONN_SLAVE_st_t = Scheduler::instance().clock();
}

void ScatFormLaw::linkDetached(bd_addr_t rmt, uchar reason)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s from %d reason:%d. \n", s.clock(), id_,
	    __FUNCTION__, rmt, reason);

    if (reason == BTDISCONN_RECONN) {
	fprintf(stdout, "*** %f %d Reconn page scan. detached from %d\n",
		Scheduler::instance().clock(), id_, rmt);
	// s.schedule(&timer, &pageScanDelayEv_, 10E-6);
	// s.schedule(&timer, &pageScanDelayEv_, 0);
	_beginPageScan();
    }
}

// Low layer notifies a link has been set up.
// A slave's status as Shared/Unshared is needed to update too. !!
void ScatFormLaw::connected(bd_addr_t rmt)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s with %d: ", s.clock(), id_, __FUNCTION__,
	    rmt);
    _succ = 1;

    switch (_status) {
    case SEEK:
	fprintf(stdout, "SEEK: wait for msg\n");
	// do nothing, wait for info sent by the slave.

	// clear SEEK state
	reset();
	break;

    case SCAN:
	fprintf(stdout, "SCAN: send info.\n");
	if (!_isLeader) {
	    Scheduler::instance().cancel(&ev_);
	}
	reset();

	// send info to the master: isLeader, the other master,
	// number of slave in the other master.
	{
	    SFLawSlaveInfo info;
	    info.isLeader = _isLeader;
	    if (_isLeader) {	// I'm not connected.
		info.master = -1;
		info.MaNumSlave = 0;
	    } else {
		info.master = _leaderMaster;
		info.MaNumSlave = _maNumSlave;
	    }

	    sendMsg(CmdInfo, (uchar *) & info, sizeof(SFLawSlaveInfo), rmt,
		    rmt);
	}

	// We ask all slaves to sniff, although this is not required
	// in the original algorithm.
	if (node_->lmp_->numPico() == 1) {
	    node_->lmp_->rpScheduler->start(node_->lmp_->curPico->
					    activeLink);
	}
	break;

    case CONN_MASTER:		// resulted from merge/move procedure
	if (--_numOfSlaveToBeConnected == 0) {
	    reset();
	    _status = SEEK;
	} else {
	    // note that current LMP page all slave before L2CAPChannel
	    // is set up.  So, at this point, paging are likely finished
	    // for all slaves.
	    //  --
	    // suspendMasterPicoForInqPage();
	}
	fprintf(stdout,
		"CONN_MASTER: addSlave, %d remains, time passed: %f\n",
		_numOfSlaveToBeConnected, s.clock() - _CONN_MASTER_st_t);
	break;

    case CONN_SLAVE:		// resulted from merge/move procedure
	fprintf(stdout, "CONN_SLAVE, stayed %f\n",
		s.clock() - _CONN_SLAVE_st_t);
	_retire();

	// We ask all slaves to sniff, although this is not required
	// in the original algorithm.
	if (node_->lmp_->numPico() == 1) {
	    node_->lmp_->rpScheduler->start(node_->lmp_->curPico->
					    activeLink);
	}
	break;

    default:
	fprintf(stderr, "%s : unkown status: %d.\n", __FUNCTION__,
		_status);
    }
}

// w is the other master of v, with wNumSlave slaves.
void ScatFormLaw::_connected(int v, int visLeader, int w, int wNumSlave)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s: v:%d lead:%d w:%d wNumSlave:%d --- \n",
	    s.clock(), id_, __FUNCTION__, v, visLeader, w, wNumSlave);

    // Page process finishes. v is a slave now.
    // v is likely a S/S bridge, unless it's a singleton.

    if (visLeader) {		// singleton

	if (numSlave() < _K) {
	    fprintf(stdout, "*** --- v:%d joins and retires\n", v);
	    _askToRetire(v, v);

	} else {
	    _retire();

	    int y = _getUnsharedSlave();
	    char sh = 0;

	    // v becomes a M/S bridge and continues to be a leader.
	    fprintf(stdout, "*** --- move %d to v:%d, %d becoms M/S\n", y,
		    v, v);
	    _move(&y, 1, &sh, v, v);
	}

    } else {

	// Union of slaves of the two masters is numSlave() + wNumSlave - 1,
	// since there is a shared slave.
	if (numSlave() + wNumSlave < _K) {
	    fprintf(stdout, "*** --- merge %d with %d slaves\n", w,
		    wNumSlave);
	    _merge(v, w);

	} else if (numSlave() == 1) {
	    // me becomes a M/S bridge.
	    char sh = 1;
	    fprintf(stdout,
		    "*** --- move %d to %d and disconn %d from %d\n", id_,
		    w, v, w);
	    _move(&id_, 1, &sh, w, v);	// _moveTo(w);

	    // consider the timing.
	    _askToDisconnect(v, w, v);
	    // _askToDisconnect(w, v, v); ???

	} else if (numSlave() + wNumSlave == _K) {
	    _retire();
	    int y = _getUnsharedSlave();
	    char sh = 0;
	    fprintf(stdout,
		    "*** --- merge %d with %d slaves and move %d to %d\n",
		    w, wNumSlave, y, v);
	    _merge(v, w);

	    // need some delay
	    _case3_y = y;
	    _case3_sh = sh;
	    _case3_v = v;

	    _case3_p2();
#if 0
	    _status = CASE3_wait;
	    Scheduler & s = Scheduler::instance();
	    s.cancel(&ev);
	    s.schedule(&timer, &ev, SCAT_LAW_CASE3_DELAY);
#endif

	} else {
	    fprintf(stdout,
		    "*** --- migrate %d (%d sl) to %d (%d slaves)\n", id_,
		    numSlave(), w, wNumSlave);
	    _migrate(v, w, wNumSlave);
	}

	_askToRetire(w, v);
    }
}

void ScatFormLaw::_case3_p2()
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    _move(&_case3_y, 1, &_case3_sh, _case3_v, _case3_v);
    _askToBecomeLeader(_case3_v, _case3_v);
}

// Move some of my slaves to w. Use v as bridge to route the message.
void ScatFormLaw::_migrate(int v, int w, int wNumSlave)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    int slavetow = _K - wNumSlave;
    int slaveLeft = numSlave() - 2;
    int numToBeMoved = (slavetow < slaveLeft ? slavetow : slaveLeft);

    // reserve an unsharedSlave y
    // y and v are not moved.
    int y = _getUnsharedSlave();

    int ad[8];
    char sh[8];
    int ind = 0;

    int i;
    LMPLink *link = node_->lmp_->masterPico->activeLink;
    for (i = 0; i < node_->lmp_->masterPico->numActiveLink; i++) {
	if (link->remote->bd_addr_ != y && link->remote->bd_addr_ != v) {
	    ad[ind++] = link->remote->bd_addr_;
	}
	link = link->next;
    }
    link = node_->lmp_->masterPico->suspendLink;
    for (i = 0; i < node_->lmp_->masterPico->numSuspendLink; i++) {
	if (link->remote->bd_addr_ != y && link->remote->bd_addr_ != v) {
	    ad[ind++] = link->remote->bd_addr_;
	}
	link = link->next;
    }

    if (ind < numToBeMoved) {
	fprintf(stderr, "*** %d %s : not enough slaves.\n",
		id_, __FUNCTION__);
    }
    _move(ad, numToBeMoved, sh, w, v);

    _askToRetire(w, v);
    _becomeLeader();
}

void ScatFormLaw::_merge(int v, int w)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    // tell w to move everything to me.
    int ad[2] = { id_, v };
    sendMsg(CmdMerge, (uchar *) ad, sizeof(int) * 2, w, v);
}

// Move everyone in current piconet, except v, to w.
void ScatFormLaw::_handleMerge(int w, int v)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    SFLawMoveInfo info;
    int numMoveSlave = numSlave() - 1;

    int ind = 0;
    int i;
    LMPLink *link = node_->lmp_->masterPico->activeLink;
    for (i = 0; i < node_->lmp_->masterPico->numActiveLink; i++) {
	if (link->remote->bd_addr_ != v) {
	    info.ad[ind++] = link->remote->bd_addr_;
	    _disconnect(link->remote->bd_addr_, BTDISCONN_RECONN);
	}
	link = link->next;
    }
    link = node_->lmp_->masterPico->suspendLink;
    for (i = 0; i < node_->lmp_->masterPico->numSuspendLink; i++) {
	if (link->remote->bd_addr_ != v) {
	    info.ad[ind++] = link->remote->bd_addr_;
	    _disconnect(link->remote->bd_addr_, BTDISCONN_RECONN);
	}
	link = link->next;
    }

    if (ind != numMoveSlave) {
	fprintf(stderr, "*** %d %s : slave num doesn't match.\n",
		id_, __FUNCTION__);
    }
    info.ad[numMoveSlave] = id_;
    info.num = ++numMoveSlave;
    sendMsg(CmdMove, (uchar *) & info, sizeof(SFLawMoveInfo), w, v);

    _retire();

    // wait for a while, so the message is farwarded.
    _status = MERGE_wait;
    _v_merge = v;
    s.cancel(&ev_);
    s.schedule(&timer_, &ev_, SCAT_LAW_MERGE_DELAY);
}

void ScatFormLaw::_handleMerge_p2()
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    _disconnect(_v_merge, BTDISCONN_LINKDOWN);

    _status = CONN_SLAVE;
    //XXX ugly
    LMPLink *link = node_->lmp_->lookupLink(_v_merge, 2);
    link->disconnReason = BTDISCONN_RECONN;
}

void ScatFormLaw::_handleDisconn(int rmt)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    _status = DISCONN_wait;
    _v_disconn = rmt;
    s.cancel(&ev_);
    s.schedule(&timer_, &ev_, SCAT_LAW_DISCONN_DELAY);
}

void ScatFormLaw::_handleDisconn_p2()
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    _disconnect(_v_disconn, BTDISCONN_LINKDOWN);
    _status = SCAN;
    LMPLink *link = node_->lmp_->lookupLink(_v_disconn, 2);
    link->disconnReason = BTDISCONN_LINKDOWN;
}

// Move a list of slaves to w. Using v as bridge.
// If w is a slave, then w becomes a M/S bridge on its own.
void ScatFormLaw::_move(int *y, int num, char *sh, int w, int v)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s to %d. \n", s.clock(), id_, __FUNCTION__,
	    w);

    SFLawMoveInfo info;
    info.num = num;

    for (int i = 0; i < num; i++) {
	info.ad[i] = y[i];
	info.isShared[i] = sh[i];
	_disconnect(y[i], BTDISCONN_RECONN);
    }

    // Tell w to make connections to each of them.
    sendMsg(CmdMove, (uchar *) & info, sizeof(SFLawMoveInfo), w, v);
}

void ScatFormLaw::_disconnect(int v, uchar reason)
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s: %d reason:%d. \n",
	    s.clock(), id_, __FUNCTION__, v, reason);

    node_->bnep_->disconnect(v, reason);
}

void ScatFormLaw::_retire()
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    if (_isLeader) {
	Scheduler::instance().cancel(&ev_);
    }
    _isLeader = false;
}

void ScatFormLaw::_becomeLeader()
{
    Scheduler & s = Scheduler::instance();
    fprintf(stdout, "*** %f %d %s. \n", s.clock(), id_, __FUNCTION__);

    _isLeader = true;
    if (ev_.uid_ <= 0) {
	double t = s.clock() - _t_main;
	int num = int (t / _delta);
	t = _t_main + num * _delta;
	while (t < s.clock()) {
	    t += _delta;
	}
	s.schedule(&timer_, &ev_, t - s.clock());
    }
}

int ScatFormLaw::numSlave()
{
    if (!node_->lmp_->masterPico) {
	return 0;
    }
    return node_->lmp_->masterPico->numActiveLink +
	node_->lmp_->masterPico->numSuspendLink;
}

int ScatFormLaw::_getUnsharedSlave()
{
    if (!node_->lmp_->masterPico) {
	fprintf(stderr, "**** %d %s: not a master\n", id_, __FUNCTION__);
	abort();
    }
    int i;
    LMPLink *link = node_->lmp_->masterPico->activeLink;
    for (i = 0; i < node_->lmp_->masterPico->numActiveLink; i++) {
	// sort of cheating.  Can we just look at link itself to 
	// judge if it is a bridge?
	// A bridge should tell the masters if it is a bridge.
	BTNode *n = node_->lookupNode(link->remote->bd_addr_);
	if (n->lmp_->numPico() == 1) {
	    return link->remote->bd_addr_;
	}
	link = link->next;
    }
    link = node_->lmp_->masterPico->suspendLink;
    for (i = 0; i < node_->lmp_->masterPico->numSuspendLink; i++) {
	BTNode *n = node_->lookupNode(link->remote->bd_addr_);
	if (n->lmp_->numPico() == 1) {
	    return link->remote->bd_addr_;
	}
	link = link->next;
    }

    fprintf(stderr, "**** %d %s: fails. \n", id_, __FUNCTION__);
    abort();
}

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


#include "rendpoint.h"
#include "bt.h"
#include "baseband.h"
#include "lmp.h"
#include "lmp-link.h"
#include "lmp-piconet.h"
#include "bt-node.h"
#include "random.h"
#include "aodv-bt.h"
#include "scat-form.h"


RPSynMsg::RPSynMsg(RPSynMsgCacheEntry * e)
{
    type = e->type;
    hops = e->hops;
    bd_addr = e->bd_addr;
    id = e->id;
    rp = e->rp;
    timestamp = e->timestamp;
}

//////////////////////////////////////////////////////////
//                        RPSched                       //
//////////////////////////////////////////////////////////
void RPSched::start(LMPLink * link)
{
    BrReq brreq;
    lmp_->lookupRP(&brreq, link);
    brreq.algo = type();
    lmp_->lmpCommand(LMP::LMP_BR_REQ, (uchar *) & brreq, sizeof(BrReq),
		     (brreq.len + 1) * 2, link);

    // int ab = link->absclk(brreq.dsniff[0]);
    fprintf(BtStat::log_,
	    " %d-%d RP start: ",
	    lmp_->bb_->bd_addr_, link->remote->bd_addr_);
    //brreq.dsniff[0], ab, ab % (lmp_->defaultTSniff_ / 2));

    uint16_t T_sniff = lmp_->defaultTSniff_ / brreq.factor;
    int sniffoffset = ((lmp_->bb_->clk_ >> 2) << 1) % T_sniff;
    double sniffinst =
	lmp_->bb_->t_clk_00_ - sniffoffset * lmp_->bb_->slotTime();
    brreq.dump(BtStat::log_, sniffinst);
}

// When a piconet is removed, this function is invoked to check if this
// node is still a bridge node.  If not, it unsniff the link.
void RPSched::stop(Piconet * pico)
{
    // Turn it off temporarily
    if (lmp_->node_->scatFormator_ &&
	lmp_->node_->scatFormator_->type() == ScatFormator::SFLaw) {
	return;
    }

    if (lmp_->numPico() != 2
	|| (lmp_->masterPico && lmp_->masterPico != pico)) {
	return;
    }

    Piconet *picoleft = (lmp_->curPico == pico ? lmp_->suspendPico :
			 (lmp_->suspendPico ==
			  pico ? lmp_->suspendPico->next : lmp_->
			  suspendPico));
    if (!picoleft) {
	return;
    }
    LMPLink *link =
	(picoleft->activeLink ? picoleft->activeLink : picoleft->
	 suspendLink);

    if (link) {
	lmp_->HCI_Exit_Sniff_Mode(link->connhand);
    }
}

void RPSched::rpAdjustStart(uint32_t dst)
{
    lmp_->node_->ragent_->sendInBuffer(dst);
}

bool RPSched::is_dst(int32_t dst, LMPLink * fromLink)
{
    if (dst == lmp_->bb_->bd_addr_) {
	return true;
    }
    if (!fromLink || !fromLink->piconet->isMaster()) {
	return false;
    }
    int i;
    LMPLink *wk = fromLink->piconet->activeLink;
    for (i = 0; i < fromLink->piconet->numActiveLink; i++) {
	if (!wk->sniffreq && wk->remote->bd_addr_ == dst) {
	    return true;
	}
	wk = wk->next;
    }
    wk = fromLink->piconet->suspendLink;
    for (i = 0; i < fromLink->piconet->numSuspendLink; i++) {
	if (!wk->sniffreq && wk->remote->bd_addr_ == dst) {
	    return true;
	}
	wk = wk->next;
    }
    return false;
}


//////////////////////////////////////////////////////////
//                     RPSchedHold                      //
//                                                      //
//  Note: use Hold mode.                                //
//////////////////////////////////////////////////////////
/*
    Workings:
	1. bridge determines <hi, ht> by checking when other links will
	   wakeup and when all other links sleep again.
	2. Master accept hold request as it.

    Issues:
	For masters, bridges may be active at the same period to share
	bandwidth while at some period there is no bridge being active
	-- low utilization of piconet bandwidth.  Well, of course those
	idle period can be used to poll non-brdige slaves, if they exist.
	For M/S bridges, this coordination is necessary, because the master
	itself being on hold, can't grant certain slave's request.

	For bridges, how does it adapt to a sensible HT good for its
	queue size?  It should check for the queue sizes of all 
	sleeping links and choose a HT respondendent to it.

	Let's devide it into 2 classes:
	 1) Tree based: master decides <ht,hi> for each slave
	 2) S/S bridges (mesh): bridge decides <ht,hi>
*/

RPSchedHold::RPSchedHold(LMP * l)
:  RPSched(l)
{
    _sniffBased = 0;
}


//////////////////////////////////////////////////////////
//                      RPHoldSI                        //
//                                                      //
//  Note: Slave initialize Hold request.                //
//////////////////////////////////////////////////////////
RPHoldSI::RPHoldSI(LMP * l)
:  RPSchedHold(l)
{
}

void RPHoldSI::start(LMPLink * link)
{
    uint16_t ht;		// Hold Time
    uint32_t hi;		// Hold Instance
    Scheduler & s = Scheduler::instance();

/*
    if (lmp_->numPico() > 1 && lmp_->masterPico) {
	fprintf(stderr, "M/S bridge should not use RPHSI.\n");
	lmp_->dump(stderr, 1);
    }
*/
    if (link->piconet->isMaster()) {
	return;
    }
    // find out when should I sleep
    double t = lmp_->lookupWakeupTime();
    if (t > 0) {
	hi = int ((t - s.clock()) / lmp_->bb_->slotTime()) +
	    lmp_->bb_->clk_ >> 1;
    } else {
	hi = (lmp_->maxHoldTime_ + lmp_->minHoldTime_) / 2 +
	    lmp_->bb_->clk_ >> 1;
    }
    if ((hi & 0x01)) {
	hi--;
    }
    // find out HT
    // should be more intelligent, like trying to figure
    // queuing size of other links.
    ht = (lmp_->maxHoldTime_ + lmp_->minHoldTime_) / 2;
    if ((ht & 0x01)) {
	ht++;
    }

    link->request_hold(ht, hi);
}

//////////////////////////////////////////////////////////
//                      RPHoldMI                        //
//                                                      //
//////////////////////////////////////////////////////////
RPHoldMI::RPHoldMI(LMP * l)
:  RPSchedHold(l)
{
}

void RPHoldMI::start(LMPLink * link)
{
    fprintf(stderr, "RPHMI has not been implemented yet.\n");
    abort();
}

//////////////////////////////////////////////////////////
//                      DRPBcast                        //
//                                                      //
//  Note: S/S bridge with 2 roles only                  //
//////////////////////////////////////////////////////////
void DRPBcastTimer::handle(Event * e)
{
    _rp->timer();
}

double DRPBcast::synSendIntv = RPSYN_SEND_INTV;
double DRPBcast::synSendRandWindMin = RPSYN_RANDOM_WIND_MIN;
double DRPBcast::synSendRandWindMax = RPSYN_RANDOM_WIND_MAX;

DRPBcast::~DRPBcast()
{
    if (_sendSyn.uid_ > 0) {
	Scheduler::instance().cancel(&_sendSyn);
    }
}

void DRPBcast::_init()
{
    mid = 0;			// msg id.
    _lastSynRecvT = 0;
    _prpCacheEntry = NULL;
    _cache = NULL;
    _firstTime = 1;

    timer();
}

void DRPBcast::_sendRPSyn(short rp, LMPLink * exceptLink)
{
    RPSynMsg req(RPSYN_REQ, 0, lmp_->bb_->bd_addr_, mid++, rp,
		 lmp_->bb_->clk_);

    lmp_->fwdCommandtoAll(LMP::LMP_RP_SYN, (uchar *) & req,
			  sizeof(RPSynMsg), 16, exceptLink, 0);
    fprintf(BtStat::log_, BTPREFIX1
	    "%d init _sendRPSyn: ", lmp_->bb_->bd_addr_);
    req.dump(BtStat::log_);

    _insertInCache(&req);
    _prp = rp;
    _prpCacheEntry = _cache;
    _cache->hops = 2;
}

void DRPBcast::recvRPSyn(RPSynMsg * req, LMPLink * link)
{
    if (!link->piconet->isMaster()) {	// BR converts RP to new piconet.       
	if (!lmp_->suspendPico) {
	    return;
	}
	int clkdiff =
	    lmp_->suspendPico->clk_offset - lmp_->curPico->clk_offset;
	req->timestamp += clkdiff;
	clkdiff /= 2;
	req->rp += clkdiff;
	req->rp %= (lmp_->defaultTSniff_ / 2);
	if (req->rp < 0) {
	    req->rp += (lmp_->defaultTSniff_ / 2);
	}
	req->hops++;

	lmp_->fwdCommandtoAll(LMP::LMP_RP_SYN, (uchar *) req,
			      sizeof(RPSynMsg), 16, link, 0);
	return;
    }

    _lastSynRecvT = Scheduler::instance().clock();

    // cache the incoming msg, check duplication
    if (!_insertInCache(req)) {
	return;
    }

    bool oldwin = false;
    int old_prp = _prp;

    // handle request
    // 1. The req is the first one, set PRP = rp.
    if (_prp < 0) {
	_prp = req->rp;
	_prpCacheEntry = _cache;	// is the header

	// 2. not first one, compare the hops, bigger wins. bd_addr bigger win.
    } else if (req->hops < _prpCacheEntry->hops) {
	oldwin = true;

    } else if (req->hops == _prpCacheEntry->hops) {
	if (req->timestamp > _prpCacheEntry->timestamp) {
	    oldwin = true;

	} else if (req->timestamp == _prpCacheEntry->timestamp
		   && req->bd_addr < _prpCacheEntry->bd_addr) {
	    oldwin = true;
	}
    }

    LMPLink *exceptlink;
    if (oldwin) {
	req = new RPSynMsg(_prpCacheEntry);
	exceptlink = NULL;
	_cache = _prpCacheEntry;
    } else {
	exceptlink = link;
	_prp = req->rp;
	_prpCacheEntry = _cache;
    }

    fprintf(BtStat::log_, BTPREFIX1
	    "%d %f (old: %d fr:%d) new PRP %d %d ",
	    lmp_->bb_->bd_addr_, _lastSynRecvT,
	    old_prp, link->remote->bd_addr_, lmp_->bb_->clk_, _prp);

    // check the following printout to verify PRP are syn'd.
    uint16_t T_sniff = lmp_->defaultTSniff_;
    int sniffoffset = ((lmp_->bb_->clk_ >> 2) << 1) % T_sniff;
    double sniffinst =
	lmp_->bb_->t_clk_00_ - sniffoffset * lmp_->bb_->slotTime();
    fprintf(BtStat::log_, BTPREFIX1
	    "si %d | ", int (sniffinst / lmp_->bb_->slotTime() + _prp) %
	    (T_sniff / 2));

    req->dump(BtStat::log_);

    // 3. send out.
    lmp_->fwdCommandtoAll(LMP::LMP_RP_SYN, (uchar *) req,
			  sizeof(RPSynMsg), 16, exceptlink, 0);
}

// Return: false if the msg is already in the Cache.
//         true, otherwise
bool DRPBcast::_insertInCache(RPSynMsg * req)
{
    RPSynMsgCacheEntry *wk = _cache;
    if (wk) {
	do {
	    if (wk->match(req)) {
		return false;
	    }
	} while ((wk = wk->next) != _cache);
    }

    _cache = new RPSynMsgCacheEntry(req, _cache);
    return true;
}

void DRPBcast::timer()
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();
    double t = synSendRandWindMin +
	Random::uniform() * (synSendRandWindMax - synSendRandWindMin);

    if (_firstTime) {
	_firstTime = 0;
	t -= synSendRandWindMin;
    } else if (_lastSynRecvT <= 0 || (now - _lastSynRecvT) > synSendIntv) {
	BrReq rps;
	if (lmp_->curPico && lmp_->curPico->isMaster()) {
	    lmp_->curPico->lookupRP(&rps);
	    if (rps.len > 0) {
		int16_t rp = rps.dsniff[0];
		if (rp >= lmp_->defaultTSniff_ / 2) {
		    rp -= lmp_->defaultTSniff_ / 2;
		}
		_sendRPSyn(rp, NULL);
	    }
	}
    }

    s.schedule(&_timer, &_sendSyn, t);
}

//////////////////////////////////////////////////////////
//                  DichRPDynWind                       //
//////////////////////////////////////////////////////////
DichRPDynWind::DichRPDynWind(LMP * l, int fact)
:DichRP(l)
{
    factor = fact;
}

//////////////////////////////////////////////////////////
//                      DichRP                          //
//////////////////////////////////////////////////////////
DichRP::DichRP(LMP * l)
:  RPSched(l), _timer(this)
{
    _rpOptReqQue = NULL;
    _prp = -1;
    _lowPower = 0;
    factor = 1;
    _clkdrift_adj_t = CLKDRIFTADJINTV;
    _adjust_for_opt_RP = 0;
    _dst = -1;
    _rpOptReqPending = 0;
    // _processRPOptReqQue = 0;
    _rpOptReqTO = 4.0;		// second
    T_sniff_me = 0;
    T_sniff_global = 0;
}

DichRP::~DichRP()
{
    if (_clkdrfit_ev.uid_ > 0) {
	Scheduler::instance().cancel(&_clkdrfit_ev);
    }
}

void DichRP::start(LMPLink * link)
{
    T_sniff_global = lmp_->defaultTSniff_;
    T_sniff_me = T_sniff_global / factor;

    BrReq brreq;
    lmp_->lookupRP(&brreq, link);
    brreq.algo = type();
    brreq.factor = factor;
    lmp_->lmpCommand(LMP::LMP_BR_REQ, (uchar *) & brreq, sizeof(BrReq),
		     (brreq.len + 1) * 2, link);

    if (lmp_->bb_->driftType_ != BT_DRIFT_OFF) {
	sched_clkdrift_adjust();
    }
#if 0
    int ab = link->absclk(brreq.dsniff[0]);
    fprintf(BtStat::log_,
	    " %d-%d RP start: %d, %d, %d clk:%d\n",
	    lmp_->bb_->bd_addr_, link->remote->bd_addr_,
	    brreq.dsniff[0], ab, ab % (lmp_->defaultTSniff_ / 2),
	    lmp_->bb_->clk_);
#endif
    fprintf(BtStat::log_,
	    " %d-%d RP start: ", lmp_->bb_->bd_addr_,
	    link->remote->bd_addr_);

    int sniffoffset = ((lmp_->bb_->clk_ >> 2) << 1) % T_sniff_me;
    double sniffinst =
	lmp_->bb_->t_clk_00_ - sniffoffset * lmp_->bb_->slotTime();
    brreq.dump(BtStat::log_, sniffinst);
}

// check if the flow with optimal RP has terminated.
void DichRP::handle_fixRPTO()
{
}

// Entry point of phase 2.
void DichRP::rpAdjustStart(uint32_t dst)
{
    DSniffOptReq req(lmp_->bb_->bd_addr_, dst);
    recvRPOptReq(&req, NULL);
}

void DichRP::handle_rpOptReqTO()
{
    // _rpOptReqPending = 0;
    processQueuedRPOptReq();
}

void DichRP::processQueuedRPOptReq()
{
    _rpOptReqPending = 0;
    ReqQue *wk;
    while ((wk = _rpOptReqQue)) {
	_rpOptReqQue = _rpOptReqQue->next;
	recvRPOptReq(&wk->req, wk->fromLink);
	delete wk;
    }
}

// Send Req to the next hop node towards the dst.
void DichRP::recvRPOptReq(DSniffOptReq * req, LMPLink * fromLink)
{
    Scheduler & s = Scheduler::instance();

    if (_rpOptReqPending) {
	// queuing the request until a reply is received.
	_rpOptReqQue = new ReqQue(_rpOptReqQue, req, fromLink);
	return;
    } else {
	_rpOptReqPending = 1;
	// start a timer to clear it in case of failure.
	if (_rpOptReqTO_ev.uid_ > 0) {
	    s.cancel(&_rpOptReqTO_ev);
	}
	s.schedule(&_timer, &_rpOptReqTO_ev, _rpOptReqTO);
    }

    if (fromLink) {
	int absRP = fromLink->absclk(req->dsniff);
	fprintf(BtStat::log_, BTPREFIX1
		"%d LMP_DSNIFF_OPT_REQ: %d %d | %d - ",
		lmp_->bb_->bd_addr_, req->dsniff, absRP,
		absRP % (lmp_->defaultTSniff_ / 2));
    } else {
	fprintf(BtStat::log_, BTPREFIX1
		"%d SRC prepares DSNIFF_OPT_REQ: ", lmp_->bb_->bd_addr_);
    }
    req->dump(BtStat::log_);

    if (is_dst(req->dst, fromLink)) {
	DSniffOptDestReply(req, fromLink);
	return;
    }

    nsaddr_t nh = lmp_->node_->ragent_->nextHop(req->dst);
    LMPLink *tolink = lmp_->lookupLink(nh, 2);
    if (!tolink) {
	fprintf(stderr, "%d %s Link (->%d->%d) is not found.\n",
		lmp_->bb_->bd_addr_, __FUNCTION__, nh, req->dst);
	abort();
    }

    processDSniffOptReq(req, fromLink, tolink);

    int pl_len = 15 + req->prp_num * 2;
    lmp_->lmpCommand(LMP::LMP_DSNIFF_OPT_REQ, (uchar *) req,
		     sizeof(DSniffOptReq), pl_len, tolink);
}

// 1. check if this node has fixed DSniff, i.e. with active traffic.
// 2. If this node has fixed DSniff, record the one in req if exists, 
//    and replace it with this node's DSniff.
// 3. collect _prp
// link is the outgoing link toward dest. Unless I'm dest.
void DichRP::processDSniffOptReq(DSniffOptReq * req, LMPLink * fromlink,
				 LMPLink * tolink)
{
    if (!fromlink) {		// special case: this node is the src.
	masterProcessDSniffOptReq(req, tolink, 1);
	return;
    }

    if (fromlink->piconet->isMaster()) {
	masterProcessDSniffOptReq(req, fromlink, 0);
	if (fromlink->piconet != tolink->piconet) {
	    if (tolink->sniffreq) {
		update_DSniffOptReqMsg(req, tolink);
	    }
	}
	return;
    }
    // convert to another piconet CLK.
    if (tolink->sniffreq) {	// FIXME:This condition is necessary ??
	update_DSniffOptReqMsg(req, tolink);
	if (tolink->piconet->isMaster()) {
	    masterProcessDSniffOptReq(req, tolink, 1);
	}
    }
}

void DichRP::masterProcessDSniffOptReq(DSniffOptReq * req, LMPLink * link,
				       int isOutLink)
{
    if (!link->sniffreq) {
	return;
    }
    // fine tune request to correct clock drift
    if (req->flag == DRPCLKDRIFTADJ) {
	req->add(link->sniffreq->D_sniff);
	return;
    }
    // master
    if (link->piconet->rpFixed) {
	link->piconet->prevFixedRP = req->dsniff;
	if (req->dsniff >= 0) {
	    req->flag = 0;
	}
	// req->dsniff = link->sniffreq->D_sniff;
	req->dsniff = (isOutLink ? link->sniffreq->D_sniff :
		       ((link->sniffreq->D_sniff +
			 lmp_->defaultTSniff_ / 2) %
			lmp_->defaultTSniff_));
    } else if (req->dsniff >= 0) {
	req->dsniff =
	    (req->dsniff +
	     lmp_->defaultTSniff_ / 2) % lmp_->defaultTSniff_;
    } else {
	// if (link->piconet->prp >= 0) {
	if (_prp >= 0) {
	    req->add(_prp);
	    int absRP = link->absclk(_prp);
	    fprintf(BtStat::log_, BTPREFIX1
		    "%d add prp: %d %d | %d - ", lmp_->bb_->bd_addr_,
		    _prp, absRP, absRP % (lmp_->defaultTSniff_ / 2));
	}
    }
}

DSniffOptReq *DichRP::genDSniffOptRepMsg(DSniffOptReq * req,
					 LMPLink * link)
{
    if (!link->piconet->isMaster()) {
	return NULL;
    }

    if (req->flag == DRPCLKDRIFTADJ) {

	req->dsniff = req->goodPrp();
	int diff = link->sniffreq->D_sniff - req->dsniff;
	if (diff < 0) {
	    diff = -diff;
	}
	int tsniff = link->sniffreq->T_sniff;
	if (diff > tsniff / 4 && diff < 3 * tsniff / 4) {
	    req->dsniff = (req->dsniff + tsniff / 2) % tsniff;
	}
	req->prp_num = 0;
	return req;
    }

    if (link->piconet->prevFixedRP >= 0) {
	// Note it was set to req->dsniff at the destination before 
	// invoking this function.
	req->dsniff = link->piconet->prevFixedRP;

    } else {
	// only valid at dst. set to 0 after that
	req->dsniff = req->goodPrp();

	if (req->dsniff < 0) {
	    req->dsniff = (link->sniffreq ? link->sniffreq->D_sniff : 0);
	}
    }
/*
#if 0
    } else if (rpFixed) {
        // when there are two fixed RPs, pickup the dest's.
        // let the source end adjust.
        req->dsniff = link->sniffreq->D_sniff;
    } else if (prevFixedRP < 0) {
        req->dsniff = req->goodPrp();
        if (req->dsniff < 0) {
            req->dsniff = (link->sniffreq ? link->sniffreq->D_sniff : 0);
        }
    } else {
        req->dsniff = prevFixedRP;
    }
#endif
*/
    req->prp_num = 0;

    return req;
}


// link is fromlink
// The master acts as a proxy for its slaves, including bridge nodes.
void DichRP::DSniffOptDestReply(DSniffOptReq * req, LMPLink * link)
{
    if (link->piconet->isMaster()) {
	if (!link->sniffreq) {
	    fprintf(stderr, "Not a sniff link.\n");
	    // abort();
	} else {
	    link->piconet->prevFixedRP = req->dsniff;
	    req = genDSniffOptRepMsg(req, link);
	    DSniffUpdate(req->dsniff, NULL, link);
	    int sniff_attempt = link->sniffreq->T_sniff / 2;
	    // It is not ture if fixRP.
	    // if (sniff_attempt >= link->sniffreq->T_sniff / 2) {
	    if (sniff_attempt > lmp_->defaultTSniff_ / 2) {
		sniff_attempt = lmp_->defaultTSniff_ / 2;
	    }
#ifdef ADJSATT
	    if (sniff_attempt >= lmp_->defaultTSniff_ / 2) {
		sniff_attempt =
		    (lmp_->defaultTSniff_ / 2 - 1) & 0xfffffffe;
	    }
#endif
	    link->update_dsniff(req->dsniff, sniff_attempt, false);
	    lmp_->curPico->RP =
		(req->dsniff +
		 lmp_->defaultTSniff_ / 2) % lmp_->defaultTSniff_;
	    lmp_->bb_->linkSched_->reset();
	}

    } else {
	// special case -- only the slave at dest gen new msg.

	// I don't have freedom to change RP arbitratry.
	if (lmp_->suspendPico && lmp_->suspendPico->suspendLink
	    && lmp_->suspendPico->suspendLink->rpFixed) {

	    int clkdiff =
		lmp_->curPico->clk_offset - lmp_->suspendPico->clk_offset;
	    clkdiff /= 2;
	    clkdiff %= lmp_->defaultTSniff_;
	    if (clkdiff < 0) {
		clkdiff += lmp_->defaultTSniff_;
	    }

	    req->dsniff =
		(lmp_->suspendPico->suspendLink->sniffreq->D_sniff +
		 clkdiff + lmp_->defaultTSniff_ / 2)
		% lmp_->defaultTSniff_;
	} else if (req->dsniff < 0) {
	    req->dsniff = req->goodPrp();
	    if (req->dsniff < 0) {
		req->dsniff =
		    (link->sniffreq ? link->sniffreq->D_sniff : 0);
	    }
	} else {
	    ;			// kept req->dsniff
	}
	req->prp_num = 0;
    }

    int ab = link->absclk(req->dsniff);
    fprintf(BtStat::log_, BTPREFIX1
	    "   DEST DS_rply: %d, %d, %d   after update\n",
	    req->dsniff, ab, ab % (lmp_->defaultTSniff_ / 2));

    int pl_len = 15 + req->prp_num * 2;
    lmp_->lmpCommand(LMP::LMP_DSNIFF_OPT_REP, (uchar *) req,
		     sizeof(DSniffOptReq), pl_len, link);

    if (req->dst != lmp_->bb_->bd_addr_) {
	lmp_->node_->ragent_->sendInBuffer(req->dst);
    }
    if (req->src != lmp_->bb_->bd_addr_) {
	lmp_->node_->ragent_->sendInBuffer(req->src);
    }
}

// update RPs in terms of clk in next piconet.  
// If (shift), RP is shifted so that there is an 1/2 Tsniff distance.
void DichRP::update_DSniffOptReqMsg(DSniffOptReq * req, LMPLink * link,
				    int shift)
{
    // double now = Scheduler::instance().clock();
    int clkdiff =
	link->piconet->clk_offset - link->lmp_->curPico->clk_offset;
    int T_sniff = link->lmp_->defaultTSniff_;
    int tsniff = link->sniffreq->T_sniff;
    int Ts_shift, ts_shift;

    if (shift) {
	Ts_shift = T_sniff / 2;
	ts_shift = tsniff / 2;
    } else {
	Ts_shift = ts_shift = 0;
    }

    clkdiff /= 2;
    clkdiff %= T_sniff;
    if (clkdiff < 0) {
	clkdiff += T_sniff;
    }

    if (req->rp >= 0) {
	req->rp = (req->rp + clkdiff + Ts_shift) % T_sniff;
    }

    if (req->dsniff >= 0) {
	req->dsniff = (req->dsniff + clkdiff + ts_shift)
	    % tsniff;
    }

    for (int i = 0; i < req->prp_num; i++) {
	req->prp[i] = (req->prp[i] + clkdiff) % (T_sniff / 2);
    }
}

// 1. adjust RPs in current piconet.
// 2. propogate the reply back to the source.
void DichRP::masterProcessDSniffOptReply(DSniffOptReq * req,
					 LMPLink * flink,
					 LMPLink * rplytoLink)
{
    int16_t ds;			// toward the dst
    int16_t attempt;
    int16_t ds_replylink;	// toward the src
    int16_t attempt_replylink;

    if (!flink->sniffreq) {
	return;
    }
    LMPLink *masterlink = (flink->piconet->isMaster()? flink : rplytoLink);
    Piconet *pico = masterlink->piconet;
    int tsniff = (masterlink->sniffreq ? masterlink->sniffreq->T_sniff :
		  flink->sniffreq->T_sniff);
    // int tsniff_replylink = tsniff;   // supposed tsniff_replylink == tsniff.

    int dist;
    int RP_b;			// opposite of req->dsniff.

    if (pico->rpFixed) {	// a segment beginning point is reached.
	fprintf(BtStat::log_,
		BTPREFIX1 "processDSniffOptReply: rpFixed: ");
	req->dump(BtStat::log_);

	if (lmp_->numPico() > 1) {	// M/S br: the picone has 1 RP.
	    // TODO: may need more work
	    // Is it possible that DS shifts by Tsniff/2 somewhere along 
	    // the path, to render this totally failed ??
	    req->dsniff = pico->prevFixedRP;
	    if (req->dsniff < 0) {
		req->dsniff = (req->dsniff + tsniff / 2) % tsniff;
	    }
	    return;
	}
	// pick up the right RP for incoming (from the dst) link
	int rp1 = pico->RP % tsniff;
	int rp2 = (rp1 + tsniff / 2) % tsniff;

	ds = flink->sniffreq->D_sniff;	// rp toward dest
	if (ds != rp1 && ds != rp2) {
	    // May caused by different Tsniff. It is ok then.
	    fprintf(stderr,
		    "Warning: difference in RPs: %d %d, having %d\n", rp1,
		    rp2, ds);
	}
	// While RPs can't be change, the assignment may be adjusted if 2 RPs
	// are allocated for this piconet.
	// Decide the right RP for the link toward dst, which has to be
	// close to the RP specified in Reply message.
	// If the link has a reduced superframe.  It's ok to have either RP.
	dist = (rp1 < req->dsniff ? req->dsniff - rp1 : rp1 - req->dsniff);
	if (dist < tsniff / 4 || dist > tsniff * 3 / 4) {
	    // Requested RP is closed to rp1
	    ds = rp1;
	} else {
	    ds = rp2;
	}

	RP_b = (req->dsniff + tsniff / 2) % tsniff;
	if (ds > RP_b) {
	    attempt = tsniff - (ds - RP_b);
	} else {
	    attempt = RP_b - ds;
	}

	if (attempt > tsniff / 2) {
	    attempt = tsniff / 2;
	}
#ifdef ADJSATT
	if (attempt == tsniff / 2) {
	    attempt = (attempt - 1) & 0xfffffffe;
	}
#endif
	if (attempt < tsniff / 4) {
	    fprintf(stderr, "***att: %d\n", attempt);
	    // attempt = lmp_->defaultTSniff_ / 4;
	}
	flink->update_dsniff(ds, attempt, false);

	// update rplytoLink
	if (rplytoLink && rplytoLink->sniffreq) {
	    ds_replylink = (ds + tsniff / 2) % tsniff;
	    attempt_replylink = tsniff / 2;

	    // Now req->dsniff is updated to a new RP reference point.
	    // Check if it is right for the link toward src, or,
	    // its opposite has to be used.
	    req = genDSniffOptRepMsg(req, rplytoLink);
	    dist =
		(ds_replylink <
		 req->dsniff ? req->dsniff - ds_replylink : ds_replylink -
		 req->dsniff);
	    if (dist > tsniff / 4 && dist < tsniff * 3 / 4) {
		// req->dsniff is not close to ds_replylink
		req->dsniff = (req->dsniff + tsniff / 2) % tsniff;
	    }
	    RP_b = (req->dsniff + tsniff / 2) % tsniff;
	    if (ds_replylink > RP_b) {
		attempt_replylink = tsniff - (ds_replylink - RP_b);
	    } else {
		attempt_replylink = RP_b - ds_replylink;
	    }
	    if (attempt_replylink > tsniff / 2) {
		attempt_replylink = tsniff / 2;
	    }
#ifdef ADJSATT
	    if (attempt_replylink == tsniff / 2) {
		attempt_replylink = (attempt_replylink - 1) & 0xfffffffe;
	    }
#endif
	    if (attempt_replylink < lmp_->defaultTSniff_ / 4) {
		fprintf(stderr, "***att: %d\n", attempt_replylink);
		//attempt_replylink = lmp_->defaultTSniff_ / 4;
	    }

	    rplytoLink->update_dsniff(ds_replylink, attempt_replylink,
				      false);
	}
	// other links are not affected, since RPs are not changed.


    } else {			// The whole piconet is adjusted to the new RPs.

	ds = req->dsniff;
	pico->RP = (ds + lmp_->defaultTSniff_ / 2) % lmp_->defaultTSniff_;

	// int tsniff = flink->sniffreq->T_sniff;
	attempt = tsniff / 2;
#ifdef ADJSATT
	attempt = (attempt - 1) & 0xfffffffe;
#endif
	if (flink->piconet->isMaster()) {
	    flink->update_dsniff(req->dsniff, attempt, false);
	}

	if (lmp_->numPico() > 1) {	// M/S br: the picone has 1 RP.
	    if (rplytoLink && rplytoLink->piconet->isMaster()) {
		rplytoLink->update_dsniff(req->dsniff, attempt, false);
	    } else if (rplytoLink) {
		req->dsniff = (req->dsniff + tsniff / 2) % tsniff;
	    }
	    DSniffUpdate(ds, flink, rplytoLink, pico);
	    return;
	}

	if (rplytoLink && rplytoLink->sniffreq) {
	    int tsniff_replylink = rplytoLink->sniffreq->T_sniff;
	    ds_replylink = (ds + tsniff_replylink / 2) % tsniff_replylink;
	    attempt_replylink = tsniff_replylink / 2;
	    rplytoLink->update_dsniff(ds_replylink, attempt_replylink,
				      false);
	    req->dsniff = rplytoLink->sniffreq->D_sniff;
	}
	// update other links.
	DSniffUpdate(ds, flink, rplytoLink, pico);

	// lmp_->bb_->linkSched_->reset();
    }

    pico->rpFixed = 1;
    lmp_->bb_->linkSched_->reset();
}

void DichRP::fineTuneRPforClkDrift(DSniffOptReq * req, LMPLink * flink,
				   LMPLink * rplytoLink)
{
    int16_t ds;
    int16_t attempt;
    int16_t ds_replylink;
    int16_t attempt_replylink;

    ds = req->dsniff;
    int tsniff = flink->sniffreq->T_sniff;
    attempt = tsniff / 2;
#ifdef ADJSATT
    if (attempt == lmp_->defaultTSniff_ / 2) {
	attempt = (attempt - 1) & 0xfffffffe;
    }
#endif

    flink->update_dsniff(req->dsniff, attempt, false);

    if (rplytoLink) {
	int tsniff_replylink = rplytoLink->sniffreq->T_sniff;
	ds_replylink = (ds + tsniff_replylink / 2) % tsniff_replylink;
	attempt_replylink = tsniff_replylink / 2;
	rplytoLink->update_dsniff(ds_replylink, attempt_replylink, false);
    }
    // update other links.
    DSniffUpdate(ds, flink, rplytoLink);

    if (rplytoLink) {
	req->dsniff = rplytoLink->sniffreq->D_sniff;
    }
    lmp_->bb_->linkSched_->reset();
}

// change DSniff of all bridges in curPico except inlink and outlink
// master only
void DichRP::DSniffUpdate(int16_t ds, LMPLink * inlink, LMPLink * outlink,
			  Piconet * pico)
{
    if (!pico) {
	pico = lmp_->curPico;
    }
    LMPLink *wk = pico->activeLink;
    int i;

    for (i = 0; i < pico->numActiveLink; i++) {
	if (wk->_in_sniff && wk != inlink && wk != outlink) {
	    wk->update_dsniff(ds);
	}
	wk = wk->next;
    }
    wk = pico->suspendLink;
    for (i = 0; i < pico->numSuspendLink; i++) {
	if (wk->_in_sniff && wk != inlink && wk != outlink) {
	    wk->update_dsniff(ds);
	}
	wk = wk->next;
    }
}


void DichRP::processDSniffOptReply(DSniffOptReq * req, LMPLink * flink,
				   LMPLink * toLink)
{
    if (lmp_->curPico->isMaster()) {
	if (req->flag == DRPCLKDRIFTADJ) {
	    fineTuneRPforClkDrift(req, flink, toLink);
	} else {

	    masterProcessDSniffOptReply(req, flink, toLink);
	    if (!toLink) {
		return;
	    }
	    if (flink->piconet != toLink->piconet) {
		if (toLink->sniffreq) {
		    update_DSniffOptReqMsg(req, toLink, 0);
		}
	    }
	    return;

	}
    } else if (toLink) {
	update_DSniffOptReqMsg(req, toLink, 1);
	if (toLink->piconet->isMaster()) {
	    masterProcessDSniffOptReply(req, flink, toLink);
	}
    }
}

void DichRP::recvRPOptReply(DSniffOptReq * req, LMPLink * fromLink)
{
    if (_rpOptReqPending) {
	if (_rpOptReqTO_ev.uid_ > 0) {
	    (Scheduler::instance()).cancel(&_rpOptReqTO_ev);
	}
	processQueuedRPOptReq();
    }

    int absRP = fromLink->absclk(req->dsniff);
    fprintf(BtStat::log_, BTPREFIX1
	    "%d LMP_DSNIFF_OPT_REP: %d %d | %d ", lmp_->bb_->bd_addr_,
	    req->dsniff, absRP, absRP % (lmp_->defaultTSniff_ / 2));
    req->dump(BtStat::log_);

    // I'm the source
    if (req->src == lmp_->bb_->bd_addr_) {
	processDSniffOptReply(req, fromLink, NULL);
#if 0
	if (lmp_->curPico->isMaster()) {
	    if (req->flag == DRPCLKDRIFTADJ) {
		fineTuneRPforClkDrift(req, fromLink, NULL);
		return;
	    } else {
		processDSniffOptReply(req, fromLink, NULL);
	    }
	} else {
	    // link->update_DSniffOptReqMsg(req);       // ???
	}
#endif
	if (req->flag == DRPOPTRP) {
	    _adjust_for_opt_RP = 1;
	    _dst = req->dst;
	    // fprintf(stderr, "Set _adjust_for_opt_RP \n");
	    if (lmp_->bb_->driftType_ != BT_DRIFT_OFF) {
		sched_clkdrift_adjust();
	    }
	}
	lmp_->node_->ragent_->sendInBuffer(req->dst);
	return;
    }

    nsaddr_t nh = lmp_->node_->ragent_->nextHop(req->src);
    LMPLink *link = lmp_->lookupLink(nh, 2);
    if (!link) {
	fprintf(stderr, "%d %s Link (->%d->%d) is not found.\n",
		lmp_->bb_->bd_addr_, __FUNCTION__, nh, req->src);
	abort();
    }

    processDSniffOptReply(req, fromLink, link);

    int ab = link->absclk(req->dsniff);
    fprintf(BtStat::log_, BTPREFIX1
	    "   receive DS_rply: %d, %d, %d   after update\n",
	    req->dsniff, ab, ab % (lmp_->defaultTSniff_ / 2));

    int pl_len = 15 + req->prp_num * 2;
    lmp_->lmpCommand(LMP::LMP_DSNIFF_OPT_REP, (uchar *) req,
		     sizeof(DSniffOptReq), pl_len, link);

    if (req->dst != lmp_->bb_->bd_addr_) {
	lmp_->node_->ragent_->sendInBuffer(req->dst);
    }
    if (req->src != lmp_->bb_->bd_addr_) {
	lmp_->node_->ragent_->sendInBuffer(req->src);
    }
}

// Phase 1.
// The master decides a suitable RP for the Link.
void DichRP::handle_request(BrReq * brreq, LMPLink * link)
{
    int isNewRP = 0;
    uchar flags = 0;
    uint16_t T_sniff = lmp_->defaultTSniff_ / brreq->factor;
    int D_sniff;
    uint16_t sniff_attempt = T_sniff / 2;
    uint16_t sniff_timeout = lmp_->defaultSniffTimeout_;
    // uint16_t affectedDS;

    int ab;
#if 0
    int ab = link->absclk(brreq->dsniff[0]);
    fprintf(BtStat::log_,
	    " %d-%d RP_b : %d, %d, %d clk:%d\n",
	    lmp_->bb_->bd_addr_, link->remote->bd_addr_,
	    brreq->dsniff[0], ab, ab % (lmp_->defaultTSniff_ / 2),
	    lmp_->bb_->clk_);
#endif
    fprintf(BtStat::log_,
	    " %d-%d RP_b : ", lmp_->bb_->bd_addr_, link->remote->bd_addr_);

    int sniffoffset = ((lmp_->bb_->clk_ >> 2) << 1) % T_sniff;
    double sniffinst =
	lmp_->bb_->t_clk_00_ - sniffoffset * lmp_->bb_->slotTime();
    brreq->dump(BtStat::log_, sniffinst);

    // assert(brreq->len <= 1);
    // sniff_attempt is set to T_sniff / 2, unless changed.

    // The other RP of the BR in the other piconet.
    int RP_b = (brreq->len == 1 ? brreq->dsniff[0] % T_sniff : -1);

    int RP1 = link->piconet->RP;
    int RP2;

    int needRenegotiation = 0;

#if 0
    if (lmp_->numPico() > 1) {	// M/S bridge -- XXX: need work

	if (RP1 < 0) {
	    BrReq myRP;
	    lmp_->lookupRP(&myRP, link);
	    int msbrNeedReRP = 0;

	    if (RP_b < 0) {
		if (myRP.len >= 1) {
		    RP1 = myRP.dsniff[0];
		    if ((RP1 % 2)) {
			RP1 = (++RP1) % T_sniff;
		    }
		} else {
		    RP1 = 0;
		    msbrNeedReRP = 1;
		}
	    } else {
		msbrNeedReRP = 1;
		RP1 = (RP_b + T_sniff / 2) % T_sniff;
		if ((RP1 % 2)) {
		    RP1--;
		}
	    }
	    link->piconet->RP = RP1;

	    if (msbrNeedReRP) {
		Piconet *pico = lmp_->suspendPico;
		LMPLink *link = 0;
		if (pico) {
		    link = (pico->suspendLink ? pico->suspendLink :
			    pico->activeLink);
		}
		if (link) {
		    lmp_->rpScheduler->start(link);
		}
	    }
	}
	D_sniff = RP1;
	int dist = (RP1 < RP_b ? RP_b - RP1 : RP1 - RP_b);
	if (dist < T_sniff / 4 || dist > T_sniff * 3 / 4) {
	    // RP_b is closer to RP1
	    needRenegotiation = 1;
	}
	// adjust sniff_attempt
	if (D_sniff > RP_b) {
	    sniff_attempt = T_sniff - (D_sniff - RP_b);
	} else {
	    sniff_attempt = RP_b - D_sniff;
	}
	if (sniff_attempt > T_sniff / 2) {
	    sniff_attempt = T_sniff / 2;
	}
	if (sniff_attempt < T_sniff / 4) {
	    needRenegotiation = 1;
	}

    } else
#endif

    if (RP1 < 0) {		// I don't have a RP yet.

	isNewRP = 1;

	// In this case, sniff_attempt or RW is optimal. No need for 
	// recalculation. ie. sniff_attempt = T_sniff / 2

	// For ultra-low duty device, radomize the RW for different piconet.
	// For others, if the BR doesn't have a RP yet at the other
	// piconet, radomize RP for this piconet too.
	//if (_lowPower || RP_b < 0) {
	if (_lowPower) {
	    D_sniff = RP1 = Random::integer(sniff_attempt / 2) * 2;

	} else if (RP_b < 0) {
	    D_sniff = 0;

	} else {
	    D_sniff = (RP_b + T_sniff / 2) % T_sniff;
	    D_sniff = (D_sniff & 0xFFFFFFFE);
	}

    } else {

	RP1 %= sniff_attempt;
	RP2 = RP1 + sniff_attempt;

	if (RP_b >= 0) {
	    int dist = (RP1 < RP_b ? RP_b - RP1 : RP1 - RP_b);

	    if (dist < T_sniff / 4 || dist > T_sniff * 3 / 4) {
		// RP_b is closer to RP1
		D_sniff = RP2;
	    } else {
		// RP_b is closer to RP2
		D_sniff = RP1;
	    }

	    // adjust sniff_attempt
	    if (D_sniff > RP_b) {
		sniff_attempt = T_sniff - (D_sniff - RP_b);
	    } else {
		sniff_attempt = RP_b - D_sniff;
	    }
	    if (sniff_attempt > T_sniff / 2) {
		sniff_attempt = T_sniff / 2;
	    }

	} else {
	    if (rand() % 2) {	// Try to even out
		D_sniff = RP2;
	    } else {
		D_sniff = RP1;
	    }
	}
    }

#if 0
    if (sniff_attempt < T_sniff / 4) {
	fprintf(stderr, "Ooops, RW:%d < %d\n", sniff_attempt, T_sniff / 4);
	abort();
    }
#endif

    if (_lowPower) {		// LowPower Mode assumes RW is <= T_sniff / 4.
	sniff_attempt = T_sniff / 4;
	if (sniff_attempt > lmp_->defaultSniffAttempt_) {
	    sniff_attempt = lmp_->defaultSniffAttempt_;
	}
    }
#ifdef ADJSATT
    if (sniff_attempt == T_sniff / 2) {
	sniff_attempt = (sniff_attempt - 1) & 0xfffffffe;
    }
#endif
    link->sniffreq =
	new LMPLink::SniffReq(flags, D_sniff, T_sniff,
			      sniff_attempt, sniff_timeout);

    //XXX for M/S bridge, this flag should not be set when RP_b is not proper.
    if (!needRenegotiation) {
	link->sniffreq->setFlagTerm();
    }

    if (!_lowPower) {
	link->sniffreq->setFlagAtt();
    }

    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) link->sniffreq,
		     sizeof(LMPLink::SniffReq), 9, link);

    if (isNewRP) {
	link->piconet->RP = D_sniff % (lmp_->defaultTSniff_ / 2);
	// XXX check if this change affects !!!
	// link->piconet->RP = D_sniff % (lmp_->defaultTSniff_);
    }
    if (link->piconet->isMaster()) {
	link->lmp_->bb_->linkSched_->reset();
    }
    // clear brreq, following 3 lines for debug.
    brreq->len = 0;
    lmp_->lookupRP(brreq);
    brreq->dump(BtStat::log_, sniffinst);

    printf("MA %d, decide RP for %d: ts:%d att:%d ds:%d\n",
	   lmp_->bb_->bd_addr_, link->remote->bd_addr_, T_sniff,
	   sniff_attempt, D_sniff);

    ab = link->absclk(D_sniff);
    fprintf(BtStat::log_,
	    " %d-%d RP assign: %d, %d, %d\n",
	    lmp_->bb_->bd_addr_, link->remote->bd_addr_,
	    D_sniff, ab, ab % (lmp_->defaultTSniff_ / 2));

}

void DichRP::sched_clkdrift_adjust()
{
    if (_clkdrfit_ev.uid_ <= 0) {
	(Scheduler::instance()).schedule(&_timer, &_clkdrfit_ev,
					 _clkdrift_adj_t);
    }
}

void DichRP::handle_clk_drift()
{

    if (lmp_->node_->enable_clkdrfit_in_rp_) {
	// fprintf(stderr, "DichRP::handle_clk_drift \n");
	// If I'm the source of path with optimal RP.
	if (_adjust_for_opt_RP) {
	    DSniffOptReq req(lmp_->bb_->bd_addr_, _dst);
	    req.flag = DRPCLKDRIFTADJ;

	    recvRPOptReq(&req, NULL);

	    // I'm a bridge.  Check if one of RW is too small now.
	} else if (!lmp_->curPico || !lmp_->curPico->isMaster()) {
	}
    }
    sched_clkdrift_adjust();
}

void DichRPTimer::handle(Event * e)
{
    if (e == &_rp->_clkdrfit_ev) {
	_rp->handle_clk_drift();
    } else if (e == &_rp->_rpOptReqTO_ev) {
	_rp->handle_rpOptReqTO();
    } else if (e == &_rp->_fixRPTO_ev) {
	_rp->handle_fixRPTO();
    } else if (e == &_rp->_linkSchedReset_ev) {	// outdated.
	_rp->lmp_->bb_->linkSched_->reset();
    }
}


//////////////////////////////////////////////////////////
//                      MultiRoleDRP                    //
//////////////////////////////////////////////////////////

// This code is not complete.  It should work, without performance 
// guarantee, though.  The basic working is if a bridge participates
// in more than 2 piconets, it will switch to each of them in turn.
// one or two in each superframe and skip certain wakeup cycle 
// in certain piconet.  An efficient skipping and sharing mechanism 
// should be investigated.

void MultiRoleDRP::start(LMPLink * link)
{
    BrReq brreq;
    lmp_->lookupRP(&brreq, link);
    brreq.algo = type();
#if 0
    if (brreq.len > 1) {
	if (_anchor[0] < 0) {
	    _anchor[0] = brreq.dsniff[0];
	    _anchor[1] = brreq.dsniff[1];
	    _num_anchor[0] = 1;
	    _num_anchor[1] = 1;
	    // find the two links.
	}
	if (_num_anchor[1] < _num_anchor[0]) {
	    brreq.dsniff[0] = _anchor[1];
	    _num_anchor[1]++;
	    // link->anchor_ind = _num_anchor1 - 1;
	} else {
	    brreq.dsniff[0] = _anchor[0];
	    _num_anchor[0]++;
	    // link->anchor_ind = _num_anchor0 - 1;
	}
	brreq.len = 1;
    }
#endif

    lmp_->lmpCommand(LMP::LMP_BR_REQ, (uchar *) & brreq, sizeof(BrReq),
		     (brreq.len + 1) * 2, link);
}

bool MultiRoleDRP::skip(LMPLink * link)
{
    // If this link is dominant in this SF, it will be active.
    // Otherwise if there is no other link is active at this moment,
    // It should be active until be preempted by a dominant link.

    int sfind = (lmp_->bb_->clk_ / 2) / link->sniffreq->T_sniff;
    if (_sched[sfind % _numRole] == link) {
	_activeLink = link;
	return false;
    } else {
	if (!_activeLink || _activeLink->isAboutToSleep()) {
	    _activeLink = link;
	    return false;
	}
    }
    return true;
}

void MultiRoleDRP::handle_request(BrReq * brreq, LMPLink * link)
{
    if (brreq->len <= 1) {
	DichRP::handle_request(brreq, link);
	return;
    }
    // The following code is from DichRP::handle_request() with
    // minimum changes. -- more work is desired.
    int isNewRP = 0;
    uchar flags = 0;
    uint16_t T_sniff = lmp_->defaultTSniff_ / brreq->factor;
    int D_sniff;
    uint16_t sniff_attempt = T_sniff / 2;
    uint16_t sniff_timeout = lmp_->defaultSniffTimeout_;
    // uint16_t affectedDS;

    int sniffoffset = ((lmp_->bb_->clk_ >> 2) << 1) % T_sniff;
    double sniffinst =
	lmp_->bb_->t_clk_00_ - sniffoffset * lmp_->bb_->slotTime();
    brreq->dump(BtStat::log_, sniffinst);

    // sniff_attempt is set to T_sniff / 2, unless changed.

    int ind = Random::integer(brreq->len);

    // The other RP of the BR in the other piconet.
    int RP_b = brreq->dsniff[ind] % T_sniff;

    int RP1 = link->piconet->RP;
    int RP2;

    if (RP1 < 0) {		// I don't have a RP yet.
	isNewRP = 1;

	// For ultra-low duty device, radomize the RW for different piconet.
	// For others, if the BR doesn't have a RP yet at the other
	// piconet, radomize RP for this piconet too.
	//if (_lowPower || RP_b < 0) {
	if (_lowPower) {
	    D_sniff = RP1 = Random::integer(sniff_attempt);

	} else if (RP_b < 0) {
	    D_sniff = 0;

	} else {
	    D_sniff = (RP_b + T_sniff / 2) % T_sniff;
	}

    } else {

	RP1 %= sniff_attempt;
	RP2 = RP1 + sniff_attempt;

#if 0
	if (RP_b >= 0) {
	    int dist = (RP1 < RP_b ? RP_b - RP1 : RP1 - RP_b);

	    if (dist < T_sniff / 4 || dist > T_sniff * 3 / 4) {
		// RP_b is closer to RP1
		D_sniff = RP2;
	    } else {
		// RP_b is closer to RP2
		D_sniff = RP1;
	    }

	    // adjust sniff_attempt
	    if (D_sniff > RP_b) {
		sniff_attempt = T_sniff - (D_sniff - RP_b);
	    } else {
		sniff_attempt = RP_b - D_sniff;
	    }
	    if (sniff_attempt > T_sniff / 2) {
		sniff_attempt = T_sniff / 2;
	    }

	} else {
#endif
	    if (rand() % 2) {	// Try to even out
		D_sniff = RP2;
	    } else {
		D_sniff = RP1;
	    }
#if 0
	}
#endif
    }

    if (sniff_attempt < T_sniff / 4) {
	fprintf(stderr, "Ooops, RW:%d < %d\n", sniff_attempt, T_sniff / 4);
	abort();
    }

    if (_lowPower) {		// LowPower Mode assumes RW is <= T_sniff / 4.
	sniff_attempt = T_sniff / 4;
	if (sniff_attempt > lmp_->defaultSniffAttempt_) {
	    sniff_attempt = lmp_->defaultSniffAttempt_;
	}
    }
#ifdef ADJSATT
    if (sniff_attempt == T_sniff / 2) {
	sniff_attempt = (sniff_attempt - 1) & 0xfffffffe;
    }
#endif
    link->sniffreq =
	new LMPLink::SniffReq(flags, D_sniff, T_sniff,
			      sniff_attempt, sniff_timeout);
    link->sniffreq->setFlagTerm();
#if 0
    if (!_lowPower) {
	link->sniffreq->setFlagAtt();
    }
#endif

    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) link->sniffreq,
		     sizeof(LMPLink::SniffReq), 9, link);

    if (isNewRP) {
	link->piconet->RP = D_sniff % (lmp_->defaultTSniff_ / 2);
    }
    if (link->piconet->isMaster()) {
	link->lmp_->bb_->linkSched_->reset();
    }
    // clear brreq, following 3 lines for debug.
    brreq->len = 0;
    lmp_->lookupRP(brreq);
    brreq->dump(BtStat::log_, sniffinst);

    printf("MA %d, decide RP for %d: ts:%d att:%d ds:%d\n",
	   lmp_->bb_->bd_addr_, link->remote->bd_addr_, T_sniff,
	   sniff_attempt, D_sniff);
}

void MultiRoleDRP::postprocessRPsched(LMPLink * link)
{
    _numRole = lmp_->numPico();
    int i;
    for (i = 0; i < _numRole; i++) {
	if (!_sched[i] || _sched[i] == link) {
	    _sched[i] = link;
	    return;
	}
    }

    // Do we need to verify it??
}


//////////////////////////////////////////////////////////
//                      TreeDRP                         //
//////////////////////////////////////////////////////////
TreeDRP::TreeDRP(LMP * l):RPSched(l)
{
    _root = _parent = _myid = l->bb_->bd_addr_;
    _uplink = NULL;

    _rp[0] = _rp[1] = -1;
    _num_rp[0] = _num_rp[1] = 0;
}

// Each br has 2 RPs, one for uplink and the other is shared by all the
// down links.  Synchronization is done in a top-down manner.
void TreeDRP::handle_request(BrReq * brreq, LMPLink * link)
{
    Piconet *pico = link->piconet;
    if (!pico->isMaster()) {
	start(link);
	return;
    }

    uint16_t T_sniff = lmp_->defaultTSniff_;
    uint16_t sniff_attempt = T_sniff / 2;
    uint16_t sniff_timeout = lmp_->defaultSniffTimeout_;
    int D_sniff;
    uchar flags = 0;

    int rpind;

    int RP_b = (brreq->len >= 1 ? brreq->dsniff[0] % T_sniff : -1);

    if (_rp[0] < 0) {		// RPs have not allocated yet.  Assume Root.
	if (RP_b < 0) {
	    _rp[0] = 0;
	} else {
	    _rp[0] = RP_b;
	}
	_rp[1] = (_rp[0] + T_sniff / 2) % T_sniff;
	_num_rp[1] = 1;
	D_sniff = _rp[1];

    } else if (isRoot()) {
	rpind = (_num_rp[0] < _num_rp[1] ? 0 : 1);
	_num_rp[rpind]++;
	D_sniff = _rp[rpind];

    } else {
	rpind = 1 - _upRP_ind;
	_num_rp[rpind]++;
	D_sniff = _rp[rpind];
    }

    link->sniffreq =
	new LMPLink::SniffReq(flags, D_sniff, T_sniff,
			      sniff_attempt, sniff_timeout);
    link->sniffreq->setFlagTerm();
    link->sniffreq->setFlagTree();
    link->sniffreq->root = _root;

    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) link->sniffreq,
		     sizeof(LMPLink::SniffReq), 15, link);

#if 0
    if (isNewRP) {
	link->piconet->RP = D_sniff % (lmp_->defaultTSniff_ / 2);
    }
#endif
    if (link->piconet->isMaster()) {
	link->lmp_->bb_->linkSched_->reset();
    }
}

void TreeDRP::adjust(LMPLink * link, int rp, bd_addr_t root)
{
    _root = root;
    _uplink = link;
    _parent = _uplink->remote->bd_addr_;

    if (lmp_->numPico() <= 1) {	// non-br
	return;
    }

    if (lmp_->masterPico) {
	int clkdiff =
	    lmp_->masterPico->clk_offset - link->piconet->clk_offset;
	int T_sniff = lmp_->defaultTSniff_;

	int newrp = (rp + clkdiff / 2) % T_sniff;
	if (newrp < 0) {
	    newrp += T_sniff;
	}
	_rp[0] = newrp;
	_num_rp[0] = 1;
	_rp[1] = (newrp + T_sniff / 2) % T_sniff;
	_upRP_ind = 0;

	// _rp[0] is the RP for uplink (as Slave role to its parent).

	// DSniffUpdate(_rp[1]);
	LMPLink *wk = lmp_->masterPico->activeLink;
	int i;

	for (i = 0; i < lmp_->masterPico->numActiveLink; i++) {
	    if (wk->_in_sniff) {
		wk->update_dsniff_forTree(_rp[1], root);
	    }
	    wk = wk->next;
	}
	wk = lmp_->masterPico->suspendLink;
	for (i = 0; i < lmp_->masterPico->numSuspendLink; i++) {
	    if (wk->_in_sniff) {
		wk->update_dsniff_forTree(_rp[1], root);
	    }
	    wk = wk->next;
	}
    }
}


//////////////////////////////////////////////////////////
//                      MaxDistRP                       //
//////////////////////////////////////////////////////////
void MaxDistRP::handle_request(BrReq * brreq, LMPLink * link)
{
    BrReq brreq_copy(*brreq);
    uchar flags = 0;
    uint16_t T_sniff = lmp_->defaultTSniff_;
    int D_sniff;
    uint16_t sniff_attempt = T_sniff / 2;
    uint16_t sniff_timeout = lmp_->defaultSniffTimeout_;
    uint16_t affectedDS;

    int sniffoffset = ((lmp_->bb_->clk_ >> 2) << 1) % T_sniff;
    double sniffinst =
	lmp_->bb_->t_clk_00_ - sniffoffset * lmp_->bb_->slotTime();
    brreq->dump(BtStat::log_, sniffinst);

    lmp_->lookupRP(brreq, link);
    brreq->dump(BtStat::log_, sniffinst);
    D_sniff = brreq->mdrp(T_sniff, &sniff_attempt, &affectedDS);
    brreq->dump(BtStat::log_, sniffinst);
    fprintf(BtStat::log_, BTPREFIX1
	    "D_sniff=%d %f att=%d\n", D_sniff,
	    sniffinst + D_sniff * BTSlotTime, sniff_attempt);

    link->sniffreq =
	new LMPLink::SniffReq(flags, D_sniff, T_sniff,
			      sniff_attempt, sniff_timeout);
    link->sniffreq->setFlagTerm();

    // adjust RW 
    if (affectedDS < 65000) {

	// Ask BR to adjust RW with the other master
	if (brreq_copy.lookup(affectedDS) > -1) {
	    link->sniffreq->rpAlgm = RPSched::MDRP;
	    link->sniffreq->affectedDs = affectedDS;

	} else {
	    masterAdjustLinkDSniff(affectedDS, sniff_attempt);
	}
    }

    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) link->sniffreq,
		     sizeof(LMPLink::SniffReq), 9, link);
}

// for each sniff link with DSniff = affectedDS, set sniff_attemp to att.
void MaxDistRP::masterAdjustLinkDSniff(uint16_t affectedDS, uint16_t att)
{
    int i;
    LMPLink *link = lmp_->curPico->activeLink;
    for (i = 0; i < lmp_->curPico->numActiveLink; i++) {
	if (link->_in_sniff
	    && link->sniffreq->D_sniff == affectedDS
	    && link->sniffreq->sniff_attempt > att) {
	    link->sniffreq->flags = 0;
	    link->sniffreq->sniff_attempt = att;
	    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) link->sniffreq,
			     sizeof(LMPLink::SniffReq), 9, link);
	}
	link = link->next;
    }

    link = lmp_->curPico->suspendLink;
    for (i = 0; i < lmp_->curPico->numSuspendLink; i++) {
	if (link->_in_sniff
	    && link->sniffreq->D_sniff == affectedDS
	    && link->sniffreq->sniff_attempt > att) {
	    link->sniffreq->flags = 0;
	    link->sniffreq->sniff_attempt = att;
	    lmp_->lmpCommand(LMP::LMP_SNIFF_REQ, (uchar *) link->sniffreq,
			     sizeof(LMPLink::SniffReq), 9, link);
	}
	link = link->next;
    }
}

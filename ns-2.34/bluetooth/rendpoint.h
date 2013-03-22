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

#ifndef __BT_RENDPOINT_H__
#define __BT_RENDPOINT_H__

#include "bt.h"
#include "hdr-bt.h"
#include "bt-stat.h"

// need to reconstructed to fit in a LMP message
// each prp can be a single byte since DSniff is alway even and we can
// let it < TSniff / 2. Then TSniff can be anything <= 1024
// Or borrow bits from prp_weight.
#define DSNIFFREQ_PRP_MAX_NUM 5

// flags
#define DRPOPTRP 1
#define DRPCLKDRIFTADJ 2
struct DSniffOptReq {
    nsaddr_t src;
    nsaddr_t dst;
    int16_t dsniff;
    int16_t rp;			// RP in the orignal superframe
    char prp_num;
    char flag;
    uchar prp[DSNIFFREQ_PRP_MAX_NUM];
    uchar prp_weight[DSNIFFREQ_PRP_MAX_NUM];

    DSniffOptReq(nsaddr_t s, nsaddr_t d):src(s), dst(d), dsniff(-1),
	rp(-1), prp_num(0), flag(DRPOPTRP) { }

    DSniffOptReq(DSniffOptReq & a) {
	src = a.src;
	dst = a.dst;
	dsniff = a.dsniff;
	rp = a.rp;
	prp_num = a.prp_num;
	flag = a.flag;
	for (int i = 0; i < prp_num; i++) {
	    prp[i] = a.prp[i];
	    prp_weight[i] = a.prp_weight[i];
	}
    }

    void updateDSniff(int16_t ds) { dsniff = ds; }
    int16_t DSniff() { return dsniff; }

    // rp is supposed to be in [0, TSniff/2).
    void add(int16_t rp) {
	int i;
	for (i = 0; i < prp_num; i++) {
	    if (prp[i] == rp) {
		if (prp_weight[i] < 255) {
		    prp_weight[i]++;
		}
		return;
	    }
	}
	if (prp_num == DSNIFFREQ_PRP_MAX_NUM) {
	    return;
	}
	// prp[prp_num] = rp;
	// prp_weight[prp_num] = 1;
	prp[i] = rp;		// i == prp_num. to make g++ happy
	prp_weight[i] = 1;
	prp_num++;
    }

    int16_t goodPrp() {
	if (prp_num == 0) {
	    return -1;
	}
	int goodguy = 0;
	for (int i = 1; i < prp_num; i++) {
	    if (prp_weight[i] > prp_weight[goodguy]) {
		goodguy = i;
	    }
	}
	return prp[goodguy];
    }

    void dump() {
	dump(stdout);
    }

    void dump(FILE * f) {
	fprintf(f, "ds %d rp %d len %d: ", dsniff, rp, prp_num);
	for (int i = 1; i < prp_num; i++) {
	    fprintf(f, "%d ", prp[i]);
	}
	fprintf(f, "\n");
    }
};

class LMPLink;
class Piconet;
class LMP;
class BrReq;

#define CLKDRIFTADJINTV	60	// sec

#define RPSYN_REQ	1
#define RPSYN_REP	2
#define RPSYN_SYN	3

struct RPSynMsgCacheEntry;
struct RPSynMsg {
    uchar type;
    uchar hops;
    int bd_addr;
    short id;
    short rp;
    int timestamp;
    // const int size = 16;

    RPSynMsg(uchar t, uchar h, int ad, short i, short r, int ts)
    :type(t), hops(h), bd_addr(ad), id(i), rp(r), timestamp(ts) { }
    RPSynMsg(RPSynMsgCacheEntry *);

    void dump(FILE * f) {
	fprintf(f, "%d h %d ad %d id %d rp %d t %d\n",
		type, hops, bd_addr, id, rp, timestamp);
    }
};

struct RPSynMsgCacheEntry {
    RPSynMsgCacheEntry *next;
    uchar type;
    uchar hops;
    int bd_addr;
    short id;
    short rp;
    int timestamp;

    RPSynMsgCacheEntry(RPSynMsg * msg, RPSynMsgCacheEntry * n) {
	type = msg->type;
	hops = msg->hops;
	bd_addr = msg->bd_addr;
	id = msg->id;
	rp = msg->rp;
	timestamp = msg->timestamp;

	if (n == NULL) {
	    next = this;
	} else {
	    next = n->next;
	    n->next = this;
	}
    }

    bool match(RPSynMsg * msg) {
	return (msg->bd_addr == bd_addr && msg->id == id);
    }
};

class RPSched {
  public:
    enum Type {
	NORP,			// do nothing
	DRP,			// Dichotomized Rendezvous Point
	DRPB,			// DRP with Broadcasting
	DRPDW,			// DRP with Dynamic Window/Superframe
	LPDRP,			// Low Power DRP for sensor networks
	TDRP,			// Tree based DRP
	MRDRP,			// Multi-Role DRP
	MDRP,			// Maximum Distance Rendezvous Point
	RPHold,			// RP by Hold mode -- Hold mode base class
	RPHSI,			// Slave Initialize Hold Req
	RPHMI,			// Master Initialize Hold Req
	NumRPAlgo
    };

    RPSched(LMP * l):lmp_(l), _sniffBased(1) { }
    virtual ~RPSched() { }
    virtual char type() { return NORP; }

    virtual void start(LMPLink * link);
    virtual void stop(Piconet * p);
    virtual void handle_request(BrReq * brreq, LMPLink * link) { }
    virtual void rpAdjustStart(uint32_t dst);
    virtual void recvRPOptReq(DSniffOptReq * req, LMPLink * fromLink) { }
    virtual void recvRPOptReply(DSniffOptReq * req, LMPLink * fromLink) { }
    virtual void recvRPSyn(RPSynMsg * req, LMPLink * link) { }
    virtual void renegotiateRP(LMPLink * link) { }

    // should I skip this sniff cycle?
    virtual bool skip(LMPLink * link) { return false; }
    virtual void postprocessRPsched(LMPLink * link) {}

    bool is_dst(int32_t dst, LMPLink * fromLink);
    int sniffBased() { return _sniffBased; }

  protected:
    LMP * lmp_;
    int _sniffBased;
};

class RPSchedHold:public RPSched {
  public:
    RPSchedHold(LMP * l);
    virtual char type() { return RPHold; }
    virtual void start(LMPLink *) = 0;
    virtual void stop(Piconet * p) {};

  protected:
};

class RPHoldSI : public RPSchedHold {
  public:
    RPHoldSI(LMP * l);
    virtual char type() { return RPHSI; }
    virtual void start(LMPLink *);
};

class RPHoldMI : public RPSchedHold {
  public:
    RPHoldMI(LMP * l);
    virtual char type() { return RPHMI; }
    virtual void start(LMPLink *);
};

class DichRP;
class DichRPTimer:public Handler {
  public:
    DichRPTimer(DichRP * r):_rp(r) { } 
    void handle(Event *);

  private:
    DichRP * _rp;
};

class DichRP:public RPSched {
    friend class DichRPTimer;
    struct ReqQue {
	ReqQue *next;
	DSniffOptReq req;
	LMPLink *fromLink;
	
	ReqQue(ReqQue * n, DSniffOptReq * r, LMPLink * l)
	:next(n), req(*r), fromLink(l) {}
    };

    ReqQue *_rpOptReqQue;

  public:
    DichRP(LMP * l);
    virtual ~DichRP();
    virtual char type() { return DRP; }
    virtual void start(LMPLink *);

    virtual void handle_request(BrReq * brreq, LMPLink * link);
    virtual void rpAdjustStart(uint32_t dst);
    virtual void recvRPOptReq(DSniffOptReq * req, LMPLink * fromLink);
    virtual void recvRPOptReply(DSniffOptReq * req, LMPLink * fromLink);

    void processDSniffOptReq(DSniffOptReq * req, LMPLink * fl,
			     LMPLink * link);
    void masterProcessDSniffOptReq(DSniffOptReq * req, LMPLink * link,
				   int isOutLink);
    DSniffOptReq *genDSniffOptRepMsg(DSniffOptReq * req, LMPLink * link);
    void DSniffOptDestReply(DSniffOptReq *, LMPLink *);
    void update_DSniffOptReqMsg(DSniffOptReq * req, LMPLink * link,
				int shift = 1);
    void fineTuneRPforClkDrift(DSniffOptReq * req, LMPLink * flink,
			       LMPLink * rplytoLink);
    void processDSniffOptReply(DSniffOptReq * req, LMPLink * flink,
			       LMPLink * rplytoLink);
    void masterProcessDSniffOptReply(DSniffOptReq * req, LMPLink * flink,
				     LMPLink * rplytoLink);
    void DSniffUpdate(int16_t ds, LMPLink * inlink, LMPLink * outlink, 
				Piconet *pico = 0);
    virtual void renegotiateRP(LMPLink * link) {
	start(link);
    }
    void sched_clkdrift_adjust();
    void handle_clk_drift();
    void handle_rpOptReqTO();
    void handle_fixRPTO();
    void processQueuedRPOptReq();

  protected:
    DichRPTimer _timer;
    Event _clkdrfit_ev;
    double _clkdrift_adj_t;
    int _adjust_for_opt_RP;
    int _dst;
    Event _linkSchedReset_ev;
    Event _rpOptReqTO_ev;
    Event _fixRPTO_ev;
    double _rpOptReqTO;

    short _prp;

    int _rpOptReqPending;

    int _lowPower;
    int factor;			// 2^i.  = T_sniff/My_T_sniff
    int T_sniff_me;
    int T_sniff_global;
};

class DRPBcast;
class DRPBcastTimer:public Handler {
  public:
    DRPBcastTimer(DRPBcast * r):_rp(r) { }
    void handle(Event *);

  private:
    DRPBcast * _rp;
};

#define RPSYN_RANDOM_WIND_MIN 2
#define RPSYN_RANDOM_WIND_MAX 5
#define RPSYN_SEND_INTV 300
class DRPBcast:public DichRP {
  public:

    DRPBcast(LMP * l):DichRP(l), _timer(this), _sendSyn() {
	_init();
    }
    virtual ~DRPBcast();

    virtual char type() {
	return DRPB;
    }

    virtual void recvRPSyn(RPSynMsg * req, LMPLink * link);

    void timer();

    int mid;

  protected:
    DRPBcastTimer _timer;
    Event _sendSyn;

    double _lastSynRecvT;
    static double synSendIntv;
    static double synSendRandWindMin;
    static double synSendRandWindMax;

    // short _prp;
    RPSynMsgCacheEntry *_prpCacheEntry;
    RPSynMsgCacheEntry *_cache;
    int _firstTime;

    void _init();
    void _sendRPSyn(int16_t, LMPLink *);
    bool _insertInCache(RPSynMsg * req);

};

// Dichotomized RP with Dynamic Window/Superframe
class DichRPDynWind:public DichRP {
  public:
    DichRPDynWind(LMP * l, int fact);

    virtual char type() { return DRPDW; }
};

#define MRDRP_NUM_ANCHOR 2
#define BTMAXROLES 8
// Extend to an multi-Role Bridge (>=2 roles)
class MultiRoleDRP:public DichRP {
  public:
    MultiRoleDRP(LMP * l):DichRP(l) {
	int i;
	for (i = 0; i < MRDRP_NUM_ANCHOR; i++) {
	    _anchor[i] = -1;
	    _num_anchor[i] = 0;
	    _cur_anchor_ind[i] = 0;
	}

	_activeLink = 0;
	_numRole = 0;
	for (i = 0; i < BTMAXROLES; i++) {
	    _sched[i] = 0;
	}
    } 
    virtual char type() { return MRDRP; }

    virtual void start(LMPLink * link);
    virtual void handle_request(BrReq * brreq, LMPLink * link);
    virtual bool skip(LMPLink * link); 
    virtual void postprocessRPsched(LMPLink * link);

  public:
    int16_t _anchor[MRDRP_NUM_ANCHOR];
    int8_t _num_anchor[MRDRP_NUM_ANCHOR];
    int8_t _cur_anchor_ind[MRDRP_NUM_ANCHOR];

    LMPLink * _sched[BTMAXROLES];
    int _numRole;
    LMPLink *_activeLink; 
};

// Tree base DRP
class TreeDRP:public RPSched {
  public:
    TreeDRP(LMP * l); 
    virtual char type() { return TDRP; }

    virtual void handle_request(BrReq * brreq, LMPLink * link);

    inline int isRoot() { return _root == _myid; }
    virtual void adjust(LMPLink * link, int rp, bd_addr_t root);

  protected:
    bd_addr_t _root;
    bd_addr_t _parent;
    bd_addr_t _myid;
    LMPLink * _uplink;	// Link to the parent
    
    int _rp[2];
    int _num_rp[2]; 
    int _upRP_ind;
};

// Adapted from P. Johansson, et al, Rendezvous Scheduling in Bluetooth
// Scatternets, ICC 2002, New York.  The original paper assumes all piconets
// are synchronized to the same CLK.
//
// 1. send to master my RPs
// 2. master checks its own RPs, then assign new one
// 3. master send it back to br.
class MaxDistRP:public RPSched {
  public:
    MaxDistRP(LMP * l):RPSched(l) { }
    virtual char type() { return MDRP; }

    virtual void handle_request(BrReq * brreq, LMPLink * link);
    void masterAdjustLinkDSniff(uint16_t affectedDS, uint16_t att);
};

#endif				// __BT_RENDPOINT_H__

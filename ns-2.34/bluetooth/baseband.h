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

#ifndef __ns_baseband_h__
#define __ns_baseband_h__

#include "packet.h"
#include "queue.h"
#include "bi-connector.h"
#include "bt.h"
#include "hdr-bt.h"
#include "math.h"
#include "bt-lossmod.h"
#include "bt-linksched.h"

using namespace std;

class Baseband;
class BTNode;
class BTChannel;

class BTCLKHandler:public Handler {
  public:
    BTCLKHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTCLKSCOHandler:public Handler {
  public:
    BTCLKSCOHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTCLKNHandler:public Handler {
  public:
    BTCLKNHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTRESYNCHandler:public Handler {
  public:
    BTRESYNCHandler(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTTRXTOTimer:public Handler {
  public:
    BTTRXTOTimer(Baseband * bb):bb_(bb) {} 
    void handle(Event * e);
  private:
    Baseband * bb_;
};

class BTRxUnlockHandler:public Handler {
  public:
    BTRxUnlockHandler(Baseband * bb):bb_(bb) {}
    inline void handle(Event * e);

  private:
    Baseband * bb_;
};

class BTslaveSendnonConnHandler:public Handler {
  public:
    BTslaveSendnonConnHandler(class Baseband * bb):bb_(bb) {} 
    void handle(Event * ev);
  private:
    Baseband * bb_;
};

class BTChangeStHander:public Handler {
  public:
    BTChangeStHander(class Baseband * bb):bb_(bb) {} 
    void handle(Event * ev);
  private:
    Baseband * bb_;
};

class BTTRXoffEvent : public Event {
  public:
    BTTRXoffEvent():st(0) {}
    // setTX() { act = TXOff;}
    // setRX() { act = RXOff;}
    void clearSt() { st = 0;}
    int st;
};

struct Bd_info {
  public:
    Bd_info * next_;
    Bd_info *lnext_;
    bd_addr_t bd_addr_;
    uchar sr_;
    uchar sp_;
    int bt_class_;
    uchar page_scan_mode_;

    clk_t clkn_;
    int lt_addr_;
    int pm_;
    int offset_;
    double drift_;
    double last_seen_time_;
    double dist_;
    int active_;
    hdr_bt::packet_type packetType_;
    hdr_bt::packet_type recvPacketType_;

    Bd_info(bd_addr_t ad, clk_t c, int offs = 0)
    :next_(0), lnext_(0), bd_addr_(ad), clkn_(c), lt_addr_(0), pm_(0), 
	offset_(offs),
	drift_(0), last_seen_time_(-1), dist_(-1), packetType_(hdr_bt::DH1),
	recvPacketType_(hdr_bt::DH1) {} 

    Bd_info(bd_addr_t ad, uchar r, uchar p, uchar ps, int off)
    :next_(0), lnext_(0), bd_addr_(ad), sr_(r), sp_(p), page_scan_mode_(ps),
	offset_(off), last_seen_time_(-1), dist_(-1) {}

    Bd_info(Bd_info &a, Bd_info* n)
    :next_(n), lnext_(0), bd_addr_(a.bd_addr_), clkn_(a.clkn_), 
	lt_addr_(a.lt_addr_),
	pm_(a.pm_), offset_(a.offset_), drift_(a.drift_), 
	last_seen_time_(a.last_seen_time_), dist_(a.dist_), 
	packetType_(a.packetType_), recvPacketType_(a.recvPacketType_) {}

    void dump(FILE *out = 0) {
	if (!out) { out = stdout; }
	fprintf(out, "Bd:ad:%d clk:%d off:%d lt_addr_:%d\n",
	       bd_addr_, clkn_, offset_, lt_addr_);
    }
};

class LMPLink;
class Piconet;
class ConnectionHandle;
class LMP;

struct BTSchedWord {
    uint8_t *word;
    uint8_t in_use;
    int len;

    BTSchedWord(bool b);
    BTSchedWord(BTSchedWord & b);
    BTSchedWord(BTSchedWord & b, int l);
    BTSchedWord(int l);
    BTSchedWord(int l, bool b);
    BTSchedWord(int l, int master, int dummy);
    ~BTSchedWord() { delete word; } 
    void dump(FILE *out = 0);
    void expand(int n) {
	if (len >= n) {
	    return;
	}
	if (n % len) {
	    return;
	}
	uint8_t *nword = new uint8_t[n];
	for(int i = 0; i < n / len; i++) {
	    for (int j = 0; j < len; j++) {
		nword[i * len + j] = word[j];
	    }
	}
	delete [] word;
	word = nword;
	len = n;
    }
};


class TxBuffer {
    // friend class LMPLink;
    friend class Baseband;
    // friend class BTFCFS;
  public:
    enum SchedPrioClass { Low, High, Tight };

    // enum Type {ACL, SCO, BBSIG};	// BBSIG is not used currently.
    TxBuffer(class Baseband * bb, class LMPLink * l, int s);
    Packet *curPkt() { return current_; }
    inline int slot() { return slot_; } 
    inline int dstTxSlot() { return dstTxSlot_; }
    inline void dstTxSlot(int s) { dstTxSlot_ = s; }
    inline int type();
    inline LMPLink *link() { return link_; }
    Packet *transmit();
    void update_T_poll();
    int handle_recv(Packet *);
    inline void ack(int a);
    int push(Packet * p);
    void flush();
    inline void switch_reg();
    inline void mark_ack() { send_ack_ = 1; }
    inline int available() { return next_ == NULL || current_ == NULL; }
    // inline int available() { return _next == NULL; }
    void reset_seqn();
    inline void suspend() { suspended_ = 1; }
    // inline void resume() { suspended_ = 0; T_poll = T_poll_default; }
    inline void resume() { suspended_ = 0; }
    bool suspended() { return suspended_ == 1; }
    inline void session_reset() {
	suspended_ = 0;
	T_poll_ = T_poll_default_;
	fails_ = 0;
	nullCntr_ = 0;
	txType_ = rxType_ = hdr_bt::Invalid;
	hasData_ = 1;
    }
    void pauseCurrentPkt() {
	if (current_ && current_->uid_ > 0) { 
	    Scheduler::instance().cancel(current_); 
	}
    }
    int hasDataPkt();
    inline int hasBcast();
    inline int nullCntr() { return nullCntr_; }
    clk_t lastPktRecvSlot() { return lastPktRecvT_; }
    clk_t lastDataPktRecvSlot() { return lastDataPktRecvT_; }
    void setLink(LMPLink *l) { link_ = l; }
    void setPrioClass(SchedPrioClass c) { prioClass_ = c; }
    void setPriority(int p) { prio_ = p; }

  private:
    Packet * current_, *next_;
    class Baseband *bb_;
    class LMPLink *link_;
    short int slot_;
    short int dstTxSlot_;
    uchar send_ack_;
    uchar seqn_rx_;
    uchar seqn_tx_;
    uchar arqn_;
    // hdr_bt::ARQN _arqn;
    bd_addr_t bd_addr_;
    int afhEnabled_;

    // flag try to sync with the master. In case the master starts a
    // SCO link before a master, the slave needs some idea if the
    // SCO link has been started at the master side.  When a slave
    // receives a SCO packet with the correct D_sco, it considers that
    // the master has started the SCO link and set _scoStarted to 1. 
    int scoStarted_;	

    #define TPOLLUPDATE_PKTNUM      8
	// record clk for last TPOLLUPDATE_PKTNUM pkt recv'd
    int32_t T_[TPOLLUPDATE_PKTNUM];		
    int t_index_;
    int t_cntr_;

    int32_t lastPktRecvT_; 	// use for Link supervision or other purpose.
    int32_t lastDataPktRecvT_;
    int lastSynT_;

    int ttlFails_;
    int fails_;
    int ttlSent_;
    int nullCntr_;
    int suspended_;
    int hasData_;

  public:

    SchedPrioClass prioClass_;
    int prio_;

    int deficit_;	//Initailized to 0, decreased by the no. of used slots
    int ttlSlots_;	// Total slots used for this link.

    // A link scheduler may set T_poll to [T_poll_default, T_poll_max].
    int32_t T_poll_max_;
    int32_t T_poll_default_;
    int32_t T_poll_;

    int32_t lastPollClk_;
    int32_t lastPollClk_highclass_;	// used by DRR

    bd_addr_t remote_bd_addr_;
    hdr_bt::packet_type txType_;
    hdr_bt::packet_type rxType_;
};

class BTChannelUseSet {
  public:
    BTChannelUseSet() { reset(); }
    BTChannelUseSet(const char *ch) { importMapByHexString(ch); }
    BTChannelUseSet(uint16_t map_h, uint32_t map_m, uint32_t map_l);

    void importMapByHexString(const char *ch);
    void importMap(uchar * map);
    uchar * exportMap(uchar * map = 0);
    inline int notUsed(int ch) { return _notUsedCh[ch]; }
    inline int numUsedCh() { return _numUsedCh; }
    inline int usedCh(int ind) { return _usedCh[ind]; }
    char * toHexString(char *s = NULL);
	
    void dump() { char s[21]; printf("%s\n", toHexString(s)); }
    void reset();

  private:
    union {
	uchar map[10];
	struct {
	    uint32_t map_l;
	    uint32_t map_m;
	    uint16_t map_h;
	} s;
    } _u;

    int _numUsedCh;
    char _usedCh[80];
    char _notUsedCh[80];

    void _expand();
};


/*
 *	ucbt energy model
 *	 -- ucbt record ttl trx ON time (activeTime_) and
 *		the number of turn-ons.
 *       duty cycle is computed as
 *            
 *              (activeTime_ + warmUpTime_ * numTurnOn_) /
 *                  (now - startTime_)
 */
struct BbEnergyRec {
    double trxTurnOnTime_;		// the time when trx is turned on

    double energyMin_;			// threshold to turn off the node
    double energy_;			// energy left
    double activeEnrgConRate_;		// energy consumption rate
    double activeTime_;			// ttl trx ON time
    double startTime_;			// record start time
    double warmUpTime_;			// the time from OFF to ON
    // double trxTurnarndTime_;		// not used
    int numTurnOn_;

    BbEnergyRec(): trxTurnOnTime_(0), energyMin_(0.1), energy_(1), 
	activeEnrgConRate_(1E-4), activeTime_(0), startTime_(0),
	warmUpTime_(0.0002), numTurnOn_(0) {
	// trxTurnarndTime_ = 0.00022;
    }
};


class Baseband:public BiConnector {
    friend class LMPLink;
    friend class LMP;
    friend class BTNode;
    friend class ScatFormPL;
    friend class ScatFormator;
    friend class ScatFormLaw;

    struct PrioLevelReq {
	struct PrioLevelReq *next;
	bd_addr_t addr;
	int prio;

	PrioLevelReq(bd_addr_t b, int p, struct PrioLevelReq *n)
	:next(n), addr(b), prio(p) {}
    } *_prioLevelReq;

    enum TrainType { Train_A, Train_B };

    enum FH_sequence_type { FHChannel, FHPage, FHInquiry,
	FHPageScan, FHInqScan, FHMasterResp, FHSlaveResp,
	FHInqResp, FHAFH
    };

  public:

    enum BBState {
	STANDBY,
	PARK_SLAVE,
	UNPARK_SLAVE,
	CONNECTION,
	NEW_CONNECTION_MASTER,
	NEW_CONNECTION_SLAVE,
	PAGE,
	PAGE_SCAN,
	SLAVE_RESP,
	MASTER_RESP,
	INQUIRY,
	INQUIRY_SCAN,
	INQUIRY_RESP,
	ROLE_SWITCH_MASTER,
	ROLE_SWITCH_SLAVE,
	RS_NEW_CONNECTION_MASTER,
	RS_NEW_CONNECTION_SLAVE,
	RE_SYNC,

/* extenstion */
	PASSIVE_LISTEN,
/* end extenstion */

	INVALID			// not a real State
    };

    static const char *state_str(BBState st) {
	static const char *const str[] = {	// the order is hard coded.
	    "STANDBY",
	    "PARK_SLAVE",
	    "UNPARK_SLAVE",
	    "CONNECTION",
	    "NEW_CONNECTION_MASTER",
	    "NEW_CONNECTION_SLAVE",
	    "PAGE",
	    "PAGE_SCAN",
	    "SLAVE_RESP",
	    "MASTER_RESP",
	    "INQUIRY",
	    "INQUIRY_SCAN",
	    "INQUIRY_RESP",
	    "ROLE_SWITCH_MASTER",
	    "ROLE_SWITCH_SLAVE",
	    "RS_NEW_CONNECTION_MASTER",
	    "RS_NEW_CONNECTION_SLAVE",
	    "RE_SYNC",

	    "PASSIVE_LISTEN",

	    "Invalid_state"
	};
	return str[st];
    }

    static const char *state_str_s(BBState st) {
	static const char *const str[] = {	// the order is hard coded.
	    "STB",
	    "PRK",
	    "UPK",
	    "CON",
	    "NCM",
	    "NCS",
	    "PAG",
	    "PSC",
	    "SLR",
	    "MAR",
	    "INQ",
	    "ISC",
	    "IRE",
	    "RSM",
	    "RSS",
	    "RCM",
	    "RCS",
	    "SYN",

	    "PLN",

	    "Inv"
	};
	return str[st];
    }

    bool isScanState() {
	return state() == PAGE_SCAN || state() == INQUIRY_SCAN
		|| state() == INQUIRY_RESP || state() == PASSIVE_LISTEN;
    }

    class ChangeStEvent : public Event {
      public:
	ChangeStEvent(): state_(INVALID) {}
	void setState(BBState st) { state_ = st; }
	BBState getState() { return state_; }
      private:
	BBState state_;
    };

    enum SCOTDDState { SCO_IDLE, SCO_SEND, SCO_RECV};

    enum Slot {
	BasebandSlot,		// transmitting Baseband message
	RecvSlot,
	ScoRecvSlot,
	ScoPrioSlot,
	NotAllowedSlot,
	ReserveSlot,
	DynamicSlot,
	BcastSlot,
	BeaconSlot,
	MinTxBufferSlot		// make sure it < maxNumTxBuffer set to 16
    };

    enum trx_st_t { 
	TRX_OFF, 
	TX_ON, 
	RX_ON, 	// listen only
	RX_RECV, 

	// case: A master transmits a 630 bit DH3 pkt.  it needs to wait
	// idly about 2 slots, then try to receive the response.
	TX_POST_WAIT,	// when a short multislot pkt transmitted, 
			// an artificial TX time apprended so that transceiver
			// will not used for something else. 
	RX_POST_WAIT 
    } trx_st_; 

    void lockTx() { trx_st_ = TX_POST_WAIT; }
    void unlockTx() { trx_st_ = (trx_st_ == TX_POST_WAIT ? TRX_OFF : trx_st_); }
    void lockRx() { trx_st_ = RX_POST_WAIT; }
    void unlockRx() { trx_st_ = (trx_st_ == RX_POST_WAIT ? TRX_OFF : trx_st_); }
    void setRxRecv() { trx_st_ = RX_RECV; }

    // enum TRXState {OFF, LISTEN, RX, TX} trxState_;	

    //////////////////////////////////////////
    Baseband();
    void setup(bd_addr_t ad, clk_t c, BTChannel *ch, LMP * lmp, BTNode * n) {
	bd_addr_ = ad; clkn_ = c; ch_ = ch; lmp_ = lmp; node_ = n;
    }
    void on();
    void reset();

    void turn_on_tx();
    void turn_on_rx();
    void turn_on_rx_to() {
	turn_on_rx();
	// trxoff_ev_.st = trx_st_;
	trxoff_ev_.st = RX_ON;
	Scheduler::instance().schedule(&trxtoTimer_, &trxoff_ev_, 
				MAX_SLOT_DRIFT);
    }
    bool trxIsOff() { return trx_st_ == TRX_OFF; }
    void turn_off_trx();
    void turn_off_trx(BTTRXoffEvent *e);
    void turn_off_rx_to();
    inline void set_trx_off(double t) {
	Scheduler & s = Scheduler::instance();
	s.cancel(&trxoff_ev_);
	trxoff_ev_.st = trx_st_;
	s.schedule(&trxtoTimer_, &trxoff_ev_, t);
    }
    void off();
    int isBusy();

    int pollReserveClass() { return pollReserveClass_; }
    int useDynamicTpoll() { return useDynamicTpoll_; }

    inline int isMaster() { return isMaster_; }

    int command(int, const char *const *);
    void sendDown(Packet *, Handler *h = 0);
    void sendUp(Packet *, Handler *);

    int recv_filter(Packet *);
    void slave_send_nonconn(Packet *);
    Packet *stamp(Packet *, TxBuffer *);
    inline void polled(int slot);

    void setPiconetParam(class Piconet * pico);
    void set_prio(bd_addr_t remote, int prio);
    void _try_to_set_prio(TxBuffer * txBuffer);

    void set_sched_word(BTSchedWord * sw) {
	sw->in_use = 1;
	sched_word_ = sw;
    }

    void setScoLTtable(LMPLink **);
    TxBuffer *allocateTxBuffer(LMPLink * link);
    void freeTxBuffer(TxBuffer *);
    inline TxBuffer *txBuffer(int slot) { return txBuffer_[slot]; }
    inline TxBuffer *lookupTxBuffer(hdr_bt *);
    inline TxBuffer *lookupScoTxBuffer();

    void handle_clkn(Event * e);
    void handle_clk(Event * e);

    void handle_re_synchronize(Event * e);
    void re_sync();
    void enter_re_sync(double);
    int synToChannelByGod(hdr_bt *bh);

    void putLinkIntoSleep(LMPLink *link);

    inline BBState state() { return state_; }
    void change_state(BBState st);
    inline const char *state_str() { return state_str(state_); }
    inline const char *state_str_s() { return state_str_s(state_); }

    void clearStateLock () { stateLocked_ = 0; }
    void setStateLock () { stateLocked_ = 1; }
    bool stateLocked() { return stateLocked_ == 1; }

    void clearConnectState();
    void standBy();

    void inquiry_scan(BTSchedWord *);
    void page_scan(BTSchedWord *);
    void page(bd_addr_t slave, uint8_t ps_rep_mod, uint8_t psmod,
	      // int16_t clock_offset, BTSchedWord * sched_word, int to = 0);
	      int32_t clock_offset, BTSchedWord * sched_word, int to = 0);
    void inquiry(int, int, int, BTSchedWord *);
    void inquiry_cancel();
    void inquiryScan_cancel();
    void page_scan_cancel();
    void page_cancel();
    void setNInquiry(int numSco);
    void setNPage(int numSco);

    void setdest(double destx, double desty, double destz, double speed);

    bool trace_null() { return trace_me_null_ || trace_all_null_; }
    bool trace_poll() { return trace_me_poll_ || trace_all_poll_; }
    bool trace_tx() { return trace_all_tx_ || trace_me_tx_; }
    bool trace_rx() { return trace_all_rx_ || trace_me_rx_; }
    bool trace_inAir() { return trace_all_in_air_ || trace_me_in_air_; }
    bool trace_state() { return trace_all_stat_ || trace_me_stat_; }

    int test_fh(bd_addr_t);
    void test_afh(int clk, int addr, const char* map, const char *s);
    signed char *seq_analysis(bd_addr_t bdaddr, FH_sequence_type fs, int clk, 
			int clkf, int len, int step, signed char *buf = NULL);

    void energyReset();
    void dumpEnergy(FILE * f = 0);
    void dumpTtlSlots(FILE * f = 0);

    double tick() { return Tick; }
    double slotTime() { return SlotTime; }
    clk_t clkn() { return clkn_; }
    clk_t clk() { return clk_; }

private:
    double SlotTime;
    double Tick;

    int isMaster_;		// my role
    bd_addr_t receiver_;
    int afhEnabled_;
    uchar transmit_fs_;

    int polling_clk_;

    int pollReserveClass_;
    int useDynamicTpoll_;

    int inSleep_;
    clk_t clkn_suspend_;
    double t_clkn_suspend_;
    double X_suspend_;
    double Y_suspend_;
    double Z_suspend_;

public:

    bd_addr_t bd_addr_;		// my bd_addr
    int lt_addr_;		// my logical transport address
    bd_addr_t master_bd_addr_;
    bd_addr_t old_master_bd_addr_;	// use in RS to clear old pico channel

    clk_t clkn_;
    clk_t clk_;

    char inRS_;

#ifdef PARK_STATE
    int ar_addr_;		// access request address
    int access_request_slot_;
    int inBeacon_;
    double beacon_instant_;	// s.clock() at beacon instant
#endif

    uchar polled_lt_addr_;		// master use it to validate receiving
    double poll_time_;
    int polling_;		// flag for master to do normal polling

    double t_clkn_00_;		// s.clock() at clkn_10 == 00
    double t_clk_00_;		// s.clock() at clk_10 == 00
    int reset_as_;		// reset (the slot to not-allowed) after sending

    // used to adjust clk drift.
    double drift_clk_;

    // int _need_syn;

    // int lastSchedSlot;
    // int lastSchedSlot_highclass;
    // int drrPass;

    BbEnergyRec energyRec_;

    // paging
    bd_addr_t slave_;
    // clk_t _slave_clk_;               // used for role switch
    int slot_offset_;		// used for role switch
    int clock_offset_;
    uint8_t page_sr_mod_;
    uint8_t page_ps_mod_;
    uint8_t slave_lt_addr_;
    int pageTO_;
    int page_timer_;
    int N_page_;
    int page_train_switch_timer_;

    int ver_;		// bluetooth spec version: 10, 11, 12, etc.

    enum ScanType {Standard, Interlaced, InterlacedSecondPart};

    // page scan
    int Page_Scan_Type_;
    int T_w_page_scan_;
    int T_w_page_scan_timer_;
    int T_page_scan_;
    int T_page_scan_timer_;
    int SR_mode_;		//R0, R1, R2
    // int SP_mode_;		//P0, P1, P2
    int connectable_;
    char pscan_fs_;
    char pscan_interlaced_fs_;
    int pscan_fs_clk_;

    int pageRespTO_;
    int pageRespTimer_;
    int newConnectionTO_;
    int newConnectionTimer_;

    // inquiry scan
    int Inquiry_Scan_Type_;
    int T_w_inquiry_scan_;	// T_GAP_101 34 slots
    int T_w_inquiry_scan_timer_;
    int T_inquiry_scan_;	// T_GAP_102  2.56s
    int T_inquiry_scan_timer_;
    int discoverable_;		// 1 or 0
    bd_addr_t inqAddr_;
    bd_addr_t iscan_addr_;
    char iscan_fs_;
    char iscan_interlaced_fs_;
    int iscan_fs_clk_;
    int iscan_N_;

    int pagescan_after_inqscan_;
    int page_after_inq_;

    // inquiry
    int N_inquiry_;
    int inquiry_train_switch_timer_;	// set to N_inquiry * 32
    int inquiryTO_;
    int inq_timer_;
    int inq_max_num_responses_;
    int inq_num_responses_;
    // Bd_info *discoved_bd_;

    int inqRespTO_;		// 128 slots  - 256
    int inqRespTimer_;
    int inBackoff_;
    int backoffTimer_;
    int backoffParam_;		// 2046
    int backoffParam_small_;		// 2046

    double page_start_time_;
    double inq_start_time_;

#define BT_DRIFT_OFF	0
#define BT_DRIFT	1
#define BT_DRIFT_NORMAL	2
#define BT_DRIFT_USER	3
    int driftType_;
    int clkdrift_;


    int passiveListenFs_;

    double resyncWind_;
    int resyncCntr_;
    int resyncWindSlotNum_;
    int maxResync_;
    double resyncWindStartT_;

    // handlers
    BTCLKNHandler clkn_handler_;
    BTCLKHandler clk_handler_;
    BTRESYNCHandler resync_handler_;
    BTTRXTOTimer trxtoTimer_; 
    BTRxUnlockHandler unlockRxHandler_;
    BTslaveSendnonConnHandler slaveSendnonConnHandler_;
    BTChangeStHander slaveChangeStHandler_;

    // Events 
    Event clk_ev_;
    Event clkn_ev_;
    BTTRXoffEvent trxoff_ev_;
    Event resync_ev_;
    ChangeStEvent slaveChangeStEv_;
    Event unlockRxEv_;

    static int trace_all_stat_;
    int trace_me_stat_;
    static int trace_all_tx_;
    int trace_me_tx_;
    static int trace_all_rx_;
    int trace_me_rx_;
    static int trace_all_null_;
    int trace_me_null_;
    static int trace_all_poll_;
    int trace_me_poll_;
    static int trace_all_in_air_;
    int trace_me_in_air_;

    static BTLossMod *lossmod;

    BTLinkScheduler *linkSched_;

    static int useSynByGod_;

    static int32_t T_poll_max_;
    static int32_t T_poll_default_;

    class BTChannel *ch_;
    class LMP *lmp_;
    class BTNode *node_;

    class BTNode * getNode() { return node_; }
    bd_addr_t getCurAccessCode();
    int channel_filter(Packet * p);

    int packetType_filter(Packet * p);
    char getCurRxFreq(int *clk, int *fsaddr);
    int handle_fs_mismatch(Packet * p, char fs, int clk, int accesscode);

    TxBuffer **txBuffer_;
  private:
    BTSchedWord *sched_word_;
    int maxNumTxBuffer_;
    int numTxBuffer_;
    TxBuffer *activeBuffer_;

    int started_;

    Packet *rxPkt_;
    double lastRecvT_;
    int txSlot_;
    int txClk_;

    int suspendClkn_;

    clk_t clke_;		// for paging
    clk_t clkf_;		// frozed clkn reading
    TrainType train_;
    BTChannelUseSet usedChSet_;

    double dXperTick_;		// det_X_per_Tick  -- update pos every tick
    double dYperTick_;		// det_Y_per_Tick
    double dZperTick_;		// det_Z_per_Tick

    int componentId_;

    /* mapping lt_addr to TxBuffer and Link and bd_addr */
    // TxBuffer **_lt_table;
    // int _lt_table_len;
    TxBuffer *lt_sco_table_[6];

    // double _sco_slot_start_time;
    SCOTDDState scoState_;

    BBState state_;
    BBState stableState_;	// STANDBY or CONNECTION
    int stateLocked_;

    /* used to control the timing to increase 'slave_rsps_count_' */
    clk_t slave_rsps_count_incr_slot_;
    int master_rsps_count_;	// used for fh kernel, aka -- N
    int slave_rsps_count_;	// used for fh kernel
    int inquiry_rsps_count_;	// used for fh kernel

  private:
    void _init();
    int sched(int, BTSchedWord *);

    inline void sendBBSig(Packet * p);	// sending bb pkt (page/inq/etc)
    int handle_recv_lastbit(Packet * p);
    void handle_recv_1stbit(Packet * p);

    void handleFhsMsg_Pscan(hdr_bt *bh);
    void handleFhsMsg_RS(hdr_bt *bh);
    void handleMsg_RS_MA(hdr_bt *bh);
    void handleMsg_NC_SL(hdr_bt *bh);
    void handleMsg_NC_MA(hdr_bt *bh);
    void handleMsg_IScan(hdr_bt *bh);
    int handleMsg_ConnState(Packet *p);

    int trySuspendClknSucc();
    int wakeupClkn();

    // Used for checking connectivity.  If there is only a single
    // Componenet of the Graph, all devices are connected.
    void clearConnMark() { componentId_ = -1; }
    int getComponentId() { return componentId_; }
    void setComponentId(int c) { componentId_ = c; }

    inline int tranceiveSlots(hdr_bt *bh);
    // inline int collision(hdr_bt * bh);
    void _slave_reply(hdr_bt * bh);
    void handle_slaveresp();
    inline int ltAddrIsValid(uchar ltaddr);
    inline int recvTimingIsOk(double, double, double *);
    int comp_clkoffset(double timi, int *clkoffset, int *sltoffset);

    inline void change_train() {
	train_ = (train_ == Train_A ? Train_B : Train_A);
    }
    void handle_inquiry_response(hdr_bt *);
    Packet *_allocPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			 bd_addr_t recv, hdr_bt::packet_type);
    inline Packet *genPollPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			  bd_addr_t recv);
    inline Packet *genNullPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			  bd_addr_t recv);
    inline Packet *genIdPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			bd_addr_t recv);
    Packet *genFHSPacket(FH_sequence_type fs, bd_addr_t addr, clk_t clk,
			 bd_addr_t myaddr, clk_t myclk, int am, bd_addr_t recv);
    void _transmit(Packet *);
    int FH_kernel(clk_t clk, clk_t clkf, FH_sequence_type FH_seq,
		  bd_addr_t addr);

    clk_t _interlaceClk(clk_t clk)
    {
	int clk_16_12 = (clk >> 12) & 0x1F;
	clk_16_12 = (clk_16_12 + 16) % 32;
	return ((clk & 0xFFFE0FFF) | (clk_16_12 << 12));
    }

    inline void computePScanFs() {
	pscan_fs_ = FH_kernel(clkn_, 0, FHPageScan, bd_addr_);
	pscan_fs_clk_ = (clkn_ | 0x0FFF);
	pscan_interlaced_fs_ =
	        FH_kernel(_interlaceClk(clkn_), 0, FHPageScan, bd_addr_);
    }
    inline void computeIScanFs() {
	iscan_fs_ = FH_kernel(clkn_, 0, FHInqScan, inqAddr_);
	iscan_fs_clk_ = (clkn_ | 0x0FFF);
	iscan_N_ = inquiry_rsps_count_;
	iscan_addr_ = inqAddr_;
	iscan_interlaced_fs_ =
		FH_kernel(_interlaceClk(clkn_), 0, FHInqScan, inqAddr_);
    }
};


///////////////////////////////////////////////////
void BTRxUnlockHandler::handle(Event * e)
{
    bb_->unlockRx();
}

#endif				// __ns_baseband_h__

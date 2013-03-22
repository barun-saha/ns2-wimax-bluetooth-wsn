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

#ifndef __ns_lmp_h__
#define __ns_lmp_h__

#include "queue.h"
#include "baseband.h"
#include "lmp-link.h"
#include "rendpoint.h"
#include "bt-channel.h"
// #include "lmp-piconet.h"


class L2CAPChannel;
class ConnectionHandle;
class LMP;
class LMPLink;

struct QosParam {
    uint8_t Flags;
    uint8_t Service_Type;
    int Token_Rate;
    int Peak_Bandwidth;
    int Latency;
    int Delay_Variation;

    QosParam(uint8_t f, uint8_t st, int tr, int pb, int l, int dv)
    :Flags(f), Service_Type(st), Token_Rate(tr),
	Peak_Bandwidth(pb), Latency(l), Delay_Variation(dv) {} 

    QosParam(QosParam & b)
    :Flags(b.Flags), Service_Type(b.Service_Type),
	Token_Rate(b.Token_Rate), Peak_Bandwidth(b.Peak_Bandwidth),
	Latency(b.Latency), Delay_Variation(b.Delay_Variation) {}
};

class LMPTimer:public Handler {
  public:
    LMPTimer(LMP * l):_lmp(l) {} 
    void handle(Event *);

  private:
    LMP * _lmp;
};

struct LMPEvent:public Event {
    enum Type {
	CheckLink,
	PeriodicInq,
	TurnOnSCOLink,
	SetSchedWord,
	ScoWakeup,	// Schedule SCO link in bridge settings.
	ScoSuspend,
	SniffWakeup,
	SniffSuspend,
	Hold,
	HoldWakeup,
	EnterPark,
	Park,
	RoleSwitch,
	RSTakeOver,
	ForcePage,
	ForceScan,
	ForceInquiry,
	DetachFirstTimer,
	Detach,
	SlaveStartClk,
	NumEvent
    };

    Type type;
    BTSchedWord *sched_word;
    LMPLink *link;
    Piconet *pico;
    uchar reason;

    LMPEvent(BTSchedWord * sw, LMPLink * l):type(TurnOnSCOLink),
	sched_word(sw), link(l), pico(0) {}
    LMPEvent(LMPLink * l, Type t):type(t), link(l), pico(0) {}
    LMPEvent(Piconet * p, Type t):type(t), link(0), pico(p) {}
    LMPEvent(Type t):type(t), link(0), pico(0) {}
};

// local interface between Host and Host-Controller
// used to connect LMP and L2CAP
//
//                                     /- L2CAPChannel
//         LMPLink -- ConnectionHandle -- L2CAPChannel
//                                     \- L2CAPChannel
//                                     \- ...
//
//         LMPLink -- ConnectionHandle -- SCO Channel
//         
class ConnectionHandle {
  public:
    ConnectionHandle(uint16_t, uint8_t rs = 1);
    void add_channel(L2CAPChannel *);
    void remove_channel(L2CAPChannel *);
    inline void setLink(LMPLink * l) { link = l; } 
    int isMaster();
    // L2CAPChannel *lastChannel();

    ConnectionHandle *next;
    ConnectionHandle *reqScoHand;

    uint16_t packet_type;	// request. Actual pt is recorded in LMPLink. 
    uint16_t recv_packet_type;

    uint8_t allow_role_switch;

    int ready_;
    LMPLink *link;
    class ScoAgent *agent;
    int sco_hand;		// ??
    L2CAPChannel *chan;
    L2CAPChannel *head;
    L2CAPChannel *_last;
    L2CAPChannel *reqCid;	// current requesting Cid asso. with this connh.
    int numChan;
    int pktRecvedLen;		// for L2CAP reassembling.
    int pktLen;			// for L2CAP reassembling.
};

struct PageReq {
    PageReq *next;
    bd_addr_t slave;
    uint8_t sr_mode;
    uint8_t ps_mode;
    uint16_t clock_offset;
    int cntr;

    ConnectionHandle *connhand;
    int pageTO;

    PageReq(bd_addr_t s, uint8_t sr, uint8_t ps, uint16_t off,
	     ConnectionHandle * c)
    :next(0), slave(s), sr_mode(sr), ps_mode(ps), clock_offset(off),
	cntr(0), connhand(c), pageTO(PAGETO) {}
};

class LMP;
class L2CAP;

#define BRREQ_TERM 1
struct BrReq {
    char algo;
    char len;
    uint16_t dsniff[7];
    char flag;
    char factor;	// Used by DRPDW

    BrReq():algo(0), len(0), flag(0), factor(1) {} 
    BrReq(BrReq &a):algo(a.algo), len(a.len) {
	for (int i = 0; i < len; i++) {
	    dsniff[i] = a.dsniff[i];
	}
	flag = a.flag;
	factor = a.factor;
    }

    int add(uint16_t d) {
	int i;
	if (len >= 7) {
	    printf("Too many RPs. max is 7.");
	    abort();
	    return 0;
	}
	for (i = 0; i < len; i++) {
	    if (dsniff[i] == d) {
		return 0;
	    }
	}
	dsniff[i] = d;	// i == len
	len++;
	return 1;
    }

    void sort(uint16_t * a, int len) {
	uint16_t tmp;
	for (int i = 0; i < len; i++) {
	    for (int j = i + 1; j < len; j++) {
		if (a[i] > a[j]) {
		    tmp = a[j];
		    a[j] = a[i];
		    a[i] = tmp;
		}
	    }
	}
    }

    void sort() {
	sort(dsniff, len);
    }

    int lookup(uint16_t ds) {
	for (int i = 0; i < len; i++) {
	    if (dsniff[i] == ds) {
		return i;
	    }
	}
	return -1;
    }

    uint16_t mdrp(int tsniff, uint16_t * sniff_attempt,
		  uint16_t * affectedDS) {
	*sniff_attempt = tsniff / 2;
	*affectedDS = 65000;
	if (len == 0) {
	    return 0;
	} else if (len == 1) {
	    return (tsniff / 2 + dsniff[0]) % tsniff;
	}

	sort(dsniff, len);
	uint16_t dist[8];
	int i;
	for (i = 0; i < len - 1; i++) {
	    dist[i] = dsniff[i + 1] - dsniff[i];
	}
	dist[len - 1] = dsniff[0] + tsniff - dsniff[len - 1];

	sort(dist, len);
	*sniff_attempt = dist[len - 1] / 2;
	for (i = 0; i < len - 1; i++) {
	    if (dsniff[i + 1] - dsniff[i] == dist[len - 1]) {
		*affectedDS = dsniff[i];
		return ((dist[len - 1] / 2 + dsniff[i]) % tsniff) & 0xFFFE;
	    }
	}
	assert(dist[len - 1] == dsniff[0] + tsniff - dsniff[len - 1]);
	*affectedDS = dsniff[len - 1];
	return ((dist[len - 1] / 2 + dsniff[len - 1]) % tsniff) & 0xFFFE;
    }

    void dump(FILE *f, double inst) {
	fprintf(f, "RP: %d [", len);
	for (int i = 0; i < len; i++) {
	    fprintf(f, "%d %f ", dsniff[i],
		   inst + dsniff[i] * BTSlotTime);
	}
	fprintf(f, "]\n");
    }
};

// Connection --  map to Channel
// Channel      -- l2cap
// Link         -- lmp
class BTChannel;
class LMP:public BiConnector {
    friend class BTNode;
    friend class Piconet;

  public:
    enum opcode {
	LMP_unknown,
	LMP_NAME_REQ,
	LMP_NAME_RES,
	LMP_ACCEPTED,
	LMP_NOT_ACCEPTED,
	LMP_CLKOFFSET_REQ,
	LMP_CLKOFFSET_RES,
	LMP_DETACH,
	LMP_SWITCH,
	LMP_HOLD,
	LMP_HOLD_REQ,
	LMP_SNIFF_REQ,
	LMP_UNSNIFF_REQ,
	LMP_PARK_REQ,
	LMP_BROADCAST_SCAN_WINDOW,
	LMP_MODIFY_BEACON,
	LMP_UNPARK_BD_ADDR_REQ,
	LMP_UNPARK_PM_ADDR_REQ,
	LMP_INCR_POWER_REQ,
	LMP_DECR_POWER_REQ,
	LMP_MAX_POWER,
	LMP_MIN_POWER,
	LMP_AUTO_RATE,
	LMP_PREFERRED_RATE,
	LMP_VERSION_REQ,
	LMP_VERSION_RES,
	LMP_FEATURES_REQ,
	LMP_FEATURES_RES,
	LMP_QOS,
	LMP_QOS_REQ,
	LMP_SCO_LINK_REQ,
	LMP_REMOVE_SCO_LINK_REQ,
	LMP_MAX_SLOT,
	LMP_MAX_SLOT_REQ,
	LMP_TIMING_ACCURACY_REQ,
	LMP_TIMING_ACCURACY_RES,
	LMP_SETUP_COMPLETE,
	LMP_HOST_CONNECTION_REQ,
	LMP_SLOT_OFFSET,
	LMP_PAGE_MODE_REQ,
	LMP_PAGE_SCAN_MODE_REQ,
	LMP_SUPERVISION_TIMEOUT,
	LMP_SLAVE_INFO_REQ,
	LMP_SLAVE_INFO,
	LMP_BR_REQ,
	LMP_RP_SYN,
	LMP_DSNIFF_OPT_REQ,
	LMP_DSNIFF_OPT_REP
    };

    static const char *opcode_str(int code) {
	// The order is hard coded.
	const char *const str[] = {
	    "LMP_unknown",
	    "LMP_NAME_REQ",
	    "LMP_NAME_RES",
	    "LMP_ACCEPTED",
	    "LMP_NOT_ACCEPTED",
	    "LMP_CLKOFFSET_REQ",
	    "LMP_CLKOFFSET_RES",
	    "LMP_DETACH",
	    "LMP_SWITCH",
	    "LMP_HOLD",
	    "LMP_HOLD_REQ",
	    "LMP_SNIFF_REQ",
	    "LMP_UNSNIFF_REQ",
	    "LMP_PARK_REQ",
	    "LMP_BROADCAST_SCAN_WINDOW",
	    "LMP_MODIFY_BEACON",
	    "LMP_UNPARK_BD_ADDR_REQ",
	    "LMP_UNPARK_PM_ADDR_REQ",
	    "LMP_INCR_POWER_REQ",
	    "LMP_DECR_POWER_REQ",
	    "LMP_MAX_POWER",
	    "LMP_MIN_POWER",
	    "LMP_AUTO_RATE",
	    "LMP_PREFERRED_RATE",
	    "LMP_VERSION_REQ",
	    "LMP_VERSION_RES",
	    "LMP_FEATURES_REQ",
	    "LMP_FEATURES_RES",
	    "LMP_QOS",
	    "LMP_QOS_REQ",
	    "LMP_SCO_LINK_REQ",
	    "LMP_REMOVE_SCO_LINK_REQ",
	    "LMP_MAX_SLOT",
	    "LMP_MAX_SLOT_REQ",
	    "LMP_TIMING_ACCURACY_REQ",
	    "LMP_TIMING_ACCURACY_RES",
	    "LMP_SETUP_COMPLETE",
	    "LMP_HOST_CONNECTION_REQ",
	    "LMP_SLOT_OFFSET",
	    "LMP_PAGE_MODE_REQ",
	    "LMP_PAGE_SCAN_MODE_REQ",
	    "LMP_SUPERVISION_TIMEOUT",
	    "LMP_SLAVE_INFO_REQ",
	    "LMP_SLAVE_INFO",
	    "LMP_BR_REQ",
	    "LMP_RP_SYN",
	    "LMP_DSNIFF_OPT_REQ",
	    "LMP_DSNIFF_OPT_REP"
	};
	return str[(opcode) code];
    }

  protected:
    void sendUp(Packet *, Handler *);

  public:

    Baseband * bb_;
    L2CAP *l2cap_;
    BTNode *node_;
    Bd_info *_my_info;
    Bd_info *_bd;		// bt device database
    // Piconet *piconet;        // curPico + suspendPico
    int numPico_;
    Piconet *masterPico;
    Piconet *masterPico_old;
    Piconet *scoPico;
    Piconet *scoPico1;
    Piconet *curPico;
    Piconet *nextPico;
    Piconet *suspendPico;

    int _root;	// root piconet for Tree based scatternet.

    double nb_timeout_;
    double nb_dist_;
    int takeover_;
    int scan_after_inq_;

    int defaultPktType_;
    int defaultRecvPktType_;

    clk_t wakeupClk_;

    static int useReSyn_;

    static hdr_bt::packet_type _defaultPktType1slot;
    static hdr_bt::packet_type _defaultPktType3slot;
    static hdr_bt::packet_type _defaultPktType5slot;

    typedef enum {l2RR, l2FCFS} L2capChPolicy;
    L2capChPolicy l2capChPolicy_;
    
    enum Rate {DM, DH, DH_2, DH_3};
    static void setRate(int r) {
	if (r < DM || r > DH_3) { r = DH; }
	setRate((Rate) r);
    }

    static void setRate(Rate r) {
	switch (r) {
	case DM:
	    _defaultPktType1slot = hdr_bt::DM1;
	    _defaultPktType3slot = hdr_bt::DM3;
	    _defaultPktType5slot = hdr_bt::DM5;
	    break;

	case DH:
	    _defaultPktType1slot = hdr_bt::DH1;
	    _defaultPktType3slot = hdr_bt::DH3;
	    _defaultPktType5slot = hdr_bt::DH5;
	    break;

	case DH_2:
	    _defaultPktType1slot = hdr_bt::DH1_2;
	    _defaultPktType3slot = hdr_bt::DH3_2;
	    _defaultPktType5slot = hdr_bt::DH5;
	    break;

	case DH_3:
	    _defaultPktType1slot = hdr_bt::DH1_3;
	    _defaultPktType3slot = hdr_bt::DH3_3;
	    _defaultPktType5slot = hdr_bt::DH5_3;
	    break;
	}
    }

    int allowRS_;
    enum Role { DontCare, MASTER, SLAVE } role_;

    uchar _scan_mask;
    enum Act { None, Scan, Inq, Page, PageScan, PicoSwitch, NoAct };
    Act _pending_act;
    int reqOutstanding;

    RPSched *rpScheduler;

    double _inq_time;

    ScoAgent *_agent;

    bd_addr_t giac_;
    int _scan_act_finish_cntr;

    /* for inquiry */
    int inq_max_period_length_;
    int inq_min_period_length_;
    int inq_periodic_;
    int inquiry_length_;
    int inq_num_responses_;

    /* for page */
    int maxPageRetries_;
    PageReq *_pagereq;		// head of the request queue
    PageReq *_pagereq_tail;	// tail of the request queue
    // connhand for the intended link set up after paging.
    ConnectionHandle *_page_conn_hand;

    double supervisionTO_;
    int supervisionEnabled_;
    double idleSchred_;
    int defaultHoldTime_;
    int minHoldTime_;
    int maxHoldTime_;
    int autoOnHold_;
    int idleCheckEnabled_;
    double idleCheckIntv_;

    int NPage_manual_;
    int NInqury_manual_;

    int failTriggerSchred_;
    int nullTriggerSchred_;

    LMPEvent _checkLink_ev;
    void checkLink();

    int defaultTSniff_;
    int defaultSniffAttempt_;
    int defaultSniffTimeout_;

    char *_name;
    int _num_Broadcast_Retran;
    int _max_num_retran;
    int _on;
    int scanWhenOn_;
    int lowDutyCycle_;

    // used for SFPL
    int tmpPico_;
    int disablePiconetSwitch_;
    int RsFailCntr_;

    LMPTimer _timer;
    LMPEvent periodInq_ev;
    LMPEvent forcePage;
    LMPEvent forceScan;
    LMPEvent forceInquiry;
    int pageStartTO_;
    int inqStartTO_;
    int scanStartTO_;

    Handler *_callback_after_inq[MAX_INQ_CALLBACK];
    int _inq_callback_ind;
    int _num_inq_callback;
    Event intr_;

    int addInqCallback(Handler *);

    void forceCurPicoOnHold(int);
    void force_page();
    void force_scan();
    void force_inquiry();
    void _page(int);
    void _page_scan(int);
    void _inquiry(int);
    void _inquiry_scan(int);
    void _page_cancel();
    void _inquiry_cancel();
    void _bb_cancel();

    void handle_PeriodicInq();
    Bd_info *getNeighborList(int *);
    void destroyNeighborList(Bd_info * nb);
    void handle_pending_act();

    LMP();
    int isRoot() { return _root; }
    LMPLink *add_piconet(Bd_info * remote, int myrole, ConnectionHandle *);
    LMPLink *add_piconet(Piconet *np, int myrole);
    void add_slave_piconet(LMPLink * link);
    Piconet *_create_piconet(Bd_info *master, Bd_info *slave, LMPLink * link);
    void remove_piconet(Piconet *p);

    void _allocate_Sco_link(LMPLink *);
    void _remove_Sco_link(LMPLink *);
    ConnectionHandle *_add_sco_connection(ConnectionHandle *);
    void on();
    void reset();
    void _init();
    void freeBdinfo();
    int numPico() { return numPico_; }
    bool trace_state() { return bb_->trace_state(); }
    void schedule_set_schedWord(LMPLink * link);
    int suspendCurPiconetReq(int suspend_slots = 0);
    void suspendCurPiconet();
    void unsuspend(Piconet * pico);
    double lookupWakeupTime();
    void tmpSuspendCurPiconetForPageScan();
    void tmpSuspendCurPiconetForPage();
    void resumeTmpSuspendMasterPico();

    void wakeup(Piconet *);
    void dump(FILE * out = 0, int dumpCurPico = 0);
    int computeDegree(int *role, int *num_br, int *numSlave);
    int computeNumMasterOfBridge(LMPLink *link, int *ma, int *numMa);
    void _addMaforBr(int *ma, int *numMa, int newma);
    int computeGeometryDegree();

    void unpark_req(int tick);
    void link_setup_complete(LMPLink * link);
    Bd_info *lookupBdinfo(bd_addr_t addr);
    // hdr_bt::packet_type lookupPacketType(bd_addr_t addr,
    //                                   hdr_bt::packet_type *);

    void setup(int bdaddr, BTChannel *ch, Baseband *bb, L2CAP *l2cap, 
			BTNode *n);
    void setClock(int clk);
    TxBuffer *getTxBuffer(ConnectionHandle * conn) {
	return conn->link->txBuffer;
    }
    int get_lt_addr(bd_addr_t slave);

    int _sco_req_pending(bd_addr_t addr);
    int addReqAgent(ScoAgent * ag);
    ScoAgent *removeReqAgent(bd_addr_t);
    void add_Sco_link_complete(LMPEvent * e);

    void lmpCommand(unsigned char, uchar *, int, int, LMPLink *);
    void lmpCommand(uchar opcode, uchar * content, int len, LMPLink * link) {
	lmpCommand(opcode, content, len, len, link);
    }
    void lmpCommand(uchar opcode, uchar content, LMPLink * link) {
	lmpCommand(opcode, &content, 1, 1, link);
    }
    void lmpCommand(uchar opcode, LMPLink * link) {
	lmpCommand(opcode, (uchar *) 0, 0, 0, link);
    }

    LMPLink *lookupLink(bd_addr_t bd, int type = 0);
    void link_setup(ConnectionHandle * conn);
    Piconet *masterPiconet() {
	return masterPico;
    };

    BTSchedWord *_prepare_bb_signal(int m, int *nSCO);
    void page_complete(bd_addr_t, int);
    void inquiry_complete(int);

    void connection_ind(bd_addr_t bd_addr, int lt_addr, int offset,
			int sltoffset);
    Bd_info *_add_bd_info(Bd_info * bd, int *newbd = 0);
    Piconet *lookupPiconet(int id);
    Piconet *lookupPiconetByMaster(bd_addr_t master);
    Bd_info *looupBdinfo(bd_addr_t);
    void clear_skip();

    // void _registerLink(LMPLink *);

    void recv_detach(LMPLink *, unsigned char);
    void remove_page_req();

    void startMasterClk(double timingOffset = 0.0);
    void slaveStartClk(Piconet * pico, double timingOffset = 0.0);
    void doSlaveStartClk(Piconet * pico);
    void switchPiconet(Piconet *, int keepClk = 0, double timingOffset = -1.0);
    void role_switch_bb_complete(bd_addr_t bd, int result);

    void changePktType(LMPLink *);
    void negotiate_link_param(LMPLink *);
    void send_slot_offset(LMPLink *);

    // void compute_sniff_param(LMPLink *);
    void setup_bridge(LMPLink *);
    int lookupRP(BrReq * brreq, LMPLink * exceptLink = 0);
    void setNeedAdjustSniffAttempt(LMPLink *);
    void fwdCommandtoAll(unsigned char, uchar *, int, int, LMPLink *,
			  int);
    void recvRoleSwitchReq(uint32_t instant, LMPLink *link);
    void adjustSlotNum(int nslotnum, hdr_bt::packet_type * pt_ptr, 
			LMPLink *link);


    ////////////////////////////////////
    //          HCI Interface         //
    ////////////////////////////////////
    void HCI_Inquiry(int lap, int inquiry_length, int num_responses);
    uchar HCI_Inquiry_Cancel();
    uchar HCI_Periodic_Inquiry_Mode(int max_period_length,
				    int min_period_length, int lap,
				    int inquiry_length, int num_responses);
    uchar HCI_Exit_Periodic_Inquiry_Mode();
    ConnectionHandle *HCI_Create_Connection(bd_addr_t bd_addr,
					    uint16_t packet_type,
					    uint8_t
					    page_scan_repetition_mode,
					    uint8_t page_scan_mode,
					    // int16_t clock_offset,
					    int32_t clock_offset,
					    uint8_t allow_role_switch);
    void HCI_Disconnect(ConnectionHandle *, uint8_t reason);
    ConnectionHandle *HCI_Add_SCO_Connection(ConnectionHandle *
					     connection_handle,
					     hdr_bt::
					     packet_type packet_type);
    void HCI_Accept_Connection_Request(bd_addr_t bd_addr, uint8_t role);
    void HCI_Reject_Connection_Request(bd_addr_t bd_addr, uint8_t reason);
    void HCI_Change_Connection_Packet_Type(ConnectionHandle *,
					   hdr_bt::packet_type);
    void HCI_Remote_Name_Request(bd_addr_t bd_addr,
				 uint8_t page_scan_repetition_mode,
				 uint8_t page_scan_mode,
				 uint16_t clock_offset);
    void HCI_Read_Remote_Supported_Features(ConnectionHandle *);
    void HCI_Read_Remote_Version_Information(ConnectionHandle *);
    void HCI_Read_Clock_Offset(ConnectionHandle *);
    void HCI_Hold_Mode(ConnectionHandle *,
		       uint16_t Hold_Mode_Max_Interval,
		       uint16_t Hold_Mode_Min_Interval);
    void HCI_Sniff_Mode(ConnectionHandle *,
			uint16_t Sniff_Max_Interval,
			uint16_t Sniff_Min_Interval,
			uint16_t Sniff_Attempt, uint16_t Sniff_Timeout);
    void HCI_Exit_Sniff_Mode(ConnectionHandle *);
    void HCI_Park_Mode(ConnectionHandle *,
		       uint16_t Beacon_Max_Interval,
		       uint16_t Beacon_Min_Interval);
    void HCI_Exit_Park_Mode(ConnectionHandle *);
    void HCI_Qos_Setup(ConnectionHandle *, uint8_t Flags,
		       uint8_t Service_Type, int Token_Rate,
		       int Peak_Bandwidth, int Latency,
		       int Delay_Variation);
    void HCI_Switch_Role(bd_addr_t bd_addr, int role);
    int HCI_Role_Discovery(ConnectionHandle * connection_handle);

    int HCI_Read_Link_Policy_Settings(ConnectionHandle *);
    int HCI_Write_Link_Policy_Settings(ConnectionHandle *,
				       uint16_t Link_Policy_Settings);
    int HCI_Set_Event_Mask(uint64_t Event_Mask);
    int HCI_Reset();
    int HCI_Set_Event_Filter(uint8_t Filter_Type,
			     uint8_t Condition_Type, uint64_t Condition);
    int HCI_Flush(ConnectionHandle *);
    int HCI_Change_Local_Name(char *Name);	// 248 bytes, null-terminated
    char *HCI_Read_Local_Name();

    int HCI_Read_Num_Broadcast_Retransmissions();
    int HCI_Write_Num_Broadcast_Retransmissions(uint8_t
						Num_Broadcast_Retran);
    int HCI_Write_Page_Scan_Activity(uint16_t PS_interval,
				     uint16_t PS_window);
    int HCI_Read_Page_Scan_Activity(uint16_t * PS_interval,
				    uint16_t * PS_window);
    int HCI_Write_Inquiry_Scan_Activity(uint16_t intv, uint16_t wind);
    int HCI_Read_Inquiry_Scan_Activity(uint16_t * intv, uint16_t * wind);
    int HCI_Write_Scan_Enable(uint8_t w);
    uint8_t HCI_Read_Scan_Enable();
};

#endif				// __ns_lmp_h__

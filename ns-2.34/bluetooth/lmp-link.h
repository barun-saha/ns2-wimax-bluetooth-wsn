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

#ifndef __ns_lmp_link_h__
#define __ns_lmp_link_h__

#include "queue.h"
#include "baseband.h"
// #include "lmp.h"


class L2CAPChannel;
class ConnectionHandle;
class LMP;

class LMPTimer;
struct LMPEvent;

// Phyical link
// each pair of device has only one ACL link and maybe 0-3 SCO links
//  per piconet??
class LMPLink {
  public:
    struct SCOReq {
//      ConnectionHandle *connhand;
	int connhand;
	uchar flag, D_sco, T_sco, airMode;
	hdr_bt::packet_type packet_type;

	SCOReq(int c, uchar f, uchar d, uchar t,
		hdr_bt::packet_type pt, uchar a):connhand(c), flag(f),
	    D_sco(d), T_sco(t), airMode(a), packet_type(pt) {}
	SCOReq(SCOReq &n): connhand(n.connhand), flag(n.flag),
	    D_sco(n.D_sco), T_sco(n.T_sco), airMode(n.airMode), 
	    packet_type(n.packet_type) {}
    };

    struct SCODisconnReq {
	// ConnectionHandle *connhand;	// uint8_t
	int connhand;
	uchar reason;
	// SCODisconnReq (ConnectionHandle *c, uchar r): connhand(c), reason(r) {}
	SCODisconnReq (uchar c, uchar r): connhand(c), reason(r) {}
    };

    struct SlotOffset {
	static const int size = 8;
	uint16_t offset;
	bd_addr_t bd_addr;
	SlotOffset *next;

	SlotOffset(uint16_t o, bd_addr_t a):offset(o), bd_addr(a), next(0) {}
    };

    struct Slave_info {
	bd_addr_t addr;
	uchar lt_addr_;
	uchar active;

	Slave_info() {} 
	Slave_info(Slave_info & a):addr(a.addr), lt_addr_(a.lt_addr_),
	    active(a.active) {
	}
    };

    struct Slave_info_msg {
	uchar num;
	Slave_info s1;
	Slave_info s2;

	Slave_info_msg() {}
    };

    struct SwitchReq {
	uint32_t instant;
    };

    struct HoldReq {
	uint16_t hold_time;
	uint32_t hold_instant;

	HoldReq(uint16_t ht, uint32_t hi):hold_time(ht), hold_instant(hi) {} 
	HoldReq(HoldReq & b):hold_time(b.hold_time),
	    hold_instant(b.hold_instant) {
	}
    };

/*
 *      |<------ T_sniff ---------->|
 *     -+---------------------------+---------------------------+-
 *      |<-att->|
 *      ^       |<-to->|            ^
 *      |              |<--absent-->|
 *      D_sniff                     D_sniff
 */
    struct SniffReq {
#define SNIFFFLAG_DS	0x02
#define SNIFFFLAG_ATT	0x04
#define SNIFFFLAG_TERM	0x08
#define SNIFFFLAG_TREE	0x10
#define SNIFFFLAG_FIXRP	0x20
#define SNIFFFLAG_MASK	0xFF
	// bit 0 is used for timing control as per spec says.
	// bits 1-7 can be used to carry other information
	uchar flags;

	uint16_t D_sniff;
	uint16_t T_sniff;
	uint16_t sniff_attempt;
	uint16_t sniff_timeout;

// #define MDRP	1
// #define DRP	2
	char rpAlgm;
	uint16_t affectedDs;
	bd_addr_t root;

	SniffReq(uchar f, uint16_t d, uint16_t t, uint16_t at,
		  uint16_t to):flags(f), D_sniff(d), T_sniff(t),
	    sniff_attempt(at), sniff_timeout(to), rpAlgm(0), affectedDs(0) {} 

	SniffReq(SniffReq & a):flags(a.flags), D_sniff(a.D_sniff),
	    T_sniff(a.T_sniff), sniff_attempt(a.sniff_attempt),
	    sniff_timeout(a.sniff_timeout), rpAlgm(a.rpAlgm), 
	    affectedDs(a.affectedDs)  {}

	bool flagTerm() {
	    return (flags & SNIFFFLAG_TERM);
	}
	void setFlagTerm() {
	    flags |= SNIFFFLAG_TERM;
	}
	void clearFlagTerm() {
	    flags &= (SNIFFFLAG_MASK ^ SNIFFFLAG_TERM);
	}

	bool flagDs() {
	    return (flags & SNIFFFLAG_DS);
	}
	void setFlagDs() {
	    flags |= SNIFFFLAG_DS;
	}
	void clearFlagDs() {
	    flags &= (SNIFFFLAG_MASK ^ SNIFFFLAG_DS);
	}

	bool flagAtt() {
	    return (flags & SNIFFFLAG_ATT);
	}
	void setFlagAtt() {
	    flags |= SNIFFFLAG_ATT;
	}
	void clearFlagAtt() {
	    flags &= (SNIFFFLAG_MASK ^ SNIFFFLAG_ATT);
	}

	bool flagTree() { return (flags & SNIFFFLAG_TREE); }
	void setFlagTree() { flags |= SNIFFFLAG_TREE; }
	void clearFlagTree() { flags &= (SNIFFFLAG_MASK ^ SNIFFFLAG_TREE); }

	bool flagFixRP() { return (flags & SNIFFFLAG_FIXRP); }
	void setFlagFixRP() { flags |= SNIFFFLAG_FIXRP; }
	void clearFlagFixRP() { flags &= (SNIFFFLAG_MASK ^ SNIFFFLAG_FIXRP); }

	// void clearFlagAll() { flags &= 0x02; }
	void clearFlagAll() { flags = 0; }
    };

    struct ParkReq {
	uchar flags;
	uint16_t D_B;		// offset of beacon instant
	uint16_t T_B;		// Beacon interval
	uchar N_B;		// Beacon slot repete N_B times
	uchar Det_B;		// beacon slots in the train seperated by Det_B
	uchar pm_addr;
	uchar ar_addr;
	uchar N_Bsleep;		// slave sleep T_B * N_Bsleep slots
	uchar D_Bsleep;		// D_Bsleep'th N_Bsleep, it wakes up once.
	uchar D_access;		// offset of beginning access window
	uchar T_access;		// access window width
	uchar N_acc_slots;
	uchar N_poll;		// slave's Poll interval
	uchar M_access;		// number of access windows
	uchar access_scheme;	// only POLL is defined in specs

	ParkReq(uchar f, uint16_t db, uint16_t tb, uchar nb, uchar detb,
		 uchar pa, uchar aa, uchar nbs, uchar dbs, uchar da,
		 uchar ta, uchar nas, uchar np, uchar ma, uchar as) {
	    flags = f;
	    D_B = db;
	    T_B = tb;
	    N_B = nb;
	    Det_B = detb;
	    pm_addr = pa;
	    ar_addr = aa;
	    N_Bsleep = nbs;
	    D_Bsleep = dbs;
	    D_access = da;
	    T_access = ta;
	    N_acc_slots = nas;
	    N_poll = np;
	    M_access = ma;
	    access_scheme = as;
	} 

	ParkReq(ParkReq & a) {
	    flags = a.flags;
	    D_B = a.D_B;
	    T_B = a.T_B;
	    N_B = a.N_B;
	    Det_B = a.Det_B;
	    pm_addr = a.pm_addr;
	    ar_addr = a.ar_addr;
	    N_Bsleep = a.N_Bsleep;
	    D_Bsleep = a.D_Bsleep;
	    D_access = a.D_access;
	    T_access = a.T_access;
	    N_acc_slots = a.N_acc_slots;
	    N_poll = a.N_poll;
	    M_access = a.M_access;
	    access_scheme = a.access_scheme;
	}
    };

    struct ModBeacon {
	uchar flags;
	uint16_t D_B;
	uint16_t T_B;
	uchar N_B;
	uchar Det_B;
	uchar D_access;
	uchar T_access;
	uchar N_acc_slots;
	uchar N_poll;
	uchar M_access;
	uchar access_scheme;
    };

    struct BcastScanWin {
	uchar flags;		// timing control flags
	uint16_t D_B;
	uint16_t scanWin;	// broadcast scan window
    };

    struct UnparkBDADDRreq {
	uchar flags;
	uint16_t D_B;
	uchar lt_addr1;
	uchar lt_addr2;
	uint32_t bd_addr1;
	uint32_t bd_addr2;
    };

    struct UnparkPMADDRreq {
	uchar flags;
	uint16_t D_B;
	uchar lt_addr[7];
	uchar pm_addr[7];
    };

    enum HCIEvent { None, ChangPktType, Connect } event;

    LMPLink(Bd_info * rm, LMP * lmp, ConnectionHandle * con);
    LMPLink(LMPLink * acl, LMP * lmp, ConnectionHandle * con, int type);
    LMPLink(LMP * lmp, int txslot);
    ~LMPLink();
    int type() { return _type; }
    Packet * getAL2capPktFragment();
    void enqueue(Packet * p);
    void flushL2CAPPkt();
    int callback();
    void sendACK();
    void sendNull();
    void sendPoll(int ack = 0);
    Packet *genPollPkt(int ack = 0);

    uchar get_pm_addr();
    uchar get_ar_addr();

    void sendBeacon();
    void wakeUpForBeacon();
    // void detach() {} //TODO: remove connh etc.
    double lastPktRecvTime();
    double lastDataPktRecvTime();
    void failTrigger();
    void nullTrigger();

    // RoleSwitch
    void handle_role_switch_tdd();
    void handle_rs_takeover();
    void role_switch_tdd_failed();
    void handle_role_switch_pico();
    void take_over_slave(Bd_info *);
    void send_slave_info();
    uint16_t lookup_slot_offset(bd_addr_t addr);
    void add_slot_offset(SlotOffset * off);

    // HOLD
    void request_hold();
    void request_hold(uint16_t ht, uint32_t hi);
    void schedule_hold();
    void handle_hold();
    void handle_hold_wakeup();
    void force_oneway_hold(int ht);
    void recvd_hold_req(HoldReq * req);
    void clear_hold_ev();

    // PARK
#ifdef PARK_STATE
    void request_park();
    void schedule_park();
    void handle_park();
    void enter_park();
    void recvd_park_req(LMPLink::ParkReq * req);
    void request_unpark();
#endif

    // SNIFF
    void clear_sniff_ev();
    void request_sniff();
    void recvd_sniff_req(LMPLink::SniffReq * req);
    void schedule_sniff();
    void adjustSniffAttempt();
    void adjustSniffAttempt(uint16_t attmpt);
    double compute_sniff_instance();
    void handle_sniff_wakeup();
    void handle_sniff_suspend();
    void recvd_in_sniff_attempt(Packet *p);
    void update_dsniff(int16_t);
    void update_dsniff(int16_t ds, uint16_t sniff_attempt, bool chooseDs);
    void update_dsniff_forTree(int16_t ds, bd_addr_t root);
    void request_unsniff();
    void handle_unsniff();
    bool isAboutToSleep();

    void handle_sco_wakeup();
    void handle_sco_suspend();

    int absclk(int16_t t);
    void recv_detach_ack();
    void handle_detach_firsttimer();
    void handle_detach();
    void qos_request_accepted();

    void addSCOLink(LMPLink *s);
    void removeScoLink(LMPLink *s);

    inline int DataQlen() { return _l2capQ.length(); }

    LMPLink *prev, *next;
    // LMPLink *next;
    LMPLink *acl;		// set if _type == SCO
    LMPLink *sco[3];
    int num_sco;

    LMP *lmp_;
    int id;
    uchar lt_addr_;
    uchar pm_addr;
    uchar ar_addr;
    Bd_info *remote;
    TxBuffer *txBuffer;
    int reset_as_;		// reset (the slot to not-allowed) after sending
    Piconet *piconet;
    ConnectionHandle *connhand;
    ConnectionHandle *reqScoHand;
    hdr_bt::packet_type pt;
    hdr_bt::packet_type rpt;
    uint8_t allow_rs;
    uint8_t rs;
    // uint16_t slot_offset;
    SlotOffset *_slot_offset;
    // other link policy ??
    int slot_offset;

    int clk_offset;

    int skip_;
    clk_t _lastSynClk;

    int send_conn_ev;
    int sent_setup_complete;
    int connected;
    int readyToRelease;
    int tobedetached;
    uchar disconnReason;
    LMPLink *sco_remv;	// The SCO is requested to be removed.

    int _noQos;
    short int T_poll_;
    uchar N_bc;
    short int T_poll_req;
    uchar N_bc_req;
    double res_req;

    uchar D_sco;		// SCO link offset within a 6-slot block
    SCOReq *sco_req;
    int sco_hand;

    LMPEvent *ev;

    Event *_hold_ev;
    Event *_hold_ev_wakeup;
    Event *_sniff_ev;
    Event *_sniff_ev_to;
    Event *_park_ev;
    Event *_park_ev_en;

    uint16_t hold_Mode_Max_Interval;
    uint16_t hold_Mode_Min_Interval;

    uint16_t sniff_Max_Interval;
    uint16_t sniff_Min_Interval;
    uint16_t sniff_Attempt;
    uint16_t sniff_Timeout;

    uint16_t beacon_Max_Interval;
    uint16_t beacon_Min_Interval;

    ParkReq *parkreq;
    HoldReq *holdreq;
    SniffReq *sniffreq;
    int rpFixed;

    double _wakeupT;
    int suspended;
    int _parked;
    int _unpark;
    int _on_hold;
    // int _hold_scheded;
    int _in_sniff;
    int _in_sniff_attempt;
    int _sniff_started;
    int needAdjustSniffAttempt;
    double delayedT;

    int Tsniff;
    int Dsniff;
    int Nattempt;
    int Ntimeout;

    int numSchWind;
    int curUse;		// slot used in current shedule window
    int curSchedWind;
    int indexInSchedWind;

    Packet *_curL2capPkt;	// the l2capPkt being fragmented and tx'd.
    int _curL2capPkt_remain_len;
    int _fragno;

  private:
    int _type;			// SCO/ACL
    PacketQueue _lmpQ;
    PacketQueue _l2capQ;

    void _init();
};

#endif				// __ns_lmp_link_h__

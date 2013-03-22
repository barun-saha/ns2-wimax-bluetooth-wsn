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

#ifndef __ns_bnep_h__
#define __ns_bnep_h__

// #include "bi-connector.h"
#include "mac.h"
#include "ip.h"
#include "baseband.h"
#include "lmp.h"
#include "l2cap.h"
#include "bt-node.h"

#define BNEP_GENERAL_ETHERNET			0x00
#define BNEP_CONTROL				0x01
#define BNEP_COMPRESSED_ETHERNET		0x02
#define BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY	0x03
#define BNEP_COMPRESSED_ETHERNET_DEST_ONLY	0x04

// bnep_type = 0 (15bytes), 
struct bnep_ether {
    int daddr;
    int saddr;
    uint16_t prot_type;
};

struct bnep_cntrl {
    uchar type;
    uint16_t pack;
};

#define BNEP_CONTROL_COMMAND_NOT_UNDERSTOOD	0x00
#define BNEP_SETUP_CONNECTION_REQUEST_MSG	0x01
#define BNEP_SETUP_CONNECTION_RESPONSE_MSG	0x02
#define BNEP_FILTER_NET_TYPE_SET_MSG		0x03
#define BNEP_FILTER_NET_TYPE_RESPONSE_MSG	0x04
#define BNEP_FILTER_MULTI_ADDR_SET_MSG		0x05
#define BNEP_FILTER_MULTI_ADDR_RESPONSE_MSG	0x06

struct bnep_conn_req {
    uchar type;
    uchar uuid_size;
    uint32_t duuid;
    uint32_t suuid;
};

struct bnep_conn_resp {
    uchar type;
    uint16_t resp;
};

#define BNEP_CONN_RESP_SUCC			0x00
#define BNEP_CONN_RESP_INVALID_D_UUID		0x01
#define BNEP_CONN_RESP_INVALID_S_UUID		0x02
#define BNEP_CONN_RESP_INVALID_UUID_SIZE	0x03
#define BNEP_CONN_RESP_CONN_NOT_ALLOWED		0x04

struct bnep_filter_entry {
    uint16_t start;
    uint16_t end;
};

struct bnep_filter_set {
    uchar type;
    uint16_t list_length;
    bnep_filter_entry *list;	// this can mapped to packet::data
};

struct bnep_filter_resp {
    uchar type;
    uint16_t resp;
};

#define BNEP_FILTER_RESP_SUCC			0x00
#define BNEP_FILTER_RESP_UNSUPPORT_REQ		0x01
#define BNEP_FILTER_RESP_INVALID_RANGE		0x02
#define BNEP_FILTER_RESP_LIMIT_REACHED  	0x03
#define BNEP_FILTER_RESP_SECURITY		0x04

struct bnep_filter_multi_entry {
    uint32_t start;
    uint32_t end;
};

struct bnep_filter_multi_set {
    uchar type;
    uint16_t list_length;
    bnep_filter_multi_entry *list;	// this can mapped to packet::data
};

struct bnep_filter_multi_resp {
    uchar type;
    uint16_t resp;
};

#define BNEP_FILTER_MULTI_RESP_SUCC		0x00
#define BNEP_FILTER_MULTI_RESP_UNSUPPORT_REQ	0x01
#define BNEP_FILTER_MULTI_RESP_INVALID_ADDR	0x02
#define BNEP_FILTER_MULTI_RESP_LIMIT_REACHED  	0x03
#define BNEP_FILTER_MULTI_RESP_SECURITY		0x04

struct bnep_ext_header {
    uchar type;			// only 0x00 is defined. BNEP_EXTENSION_CONTROL
    uchar ext_bit;
    uchar length;
    uint16_t payload;
};

struct hdr_bnep {
    uchar type;
    uchar ext_bit;
    union {
	bnep_ether ether;
	bnep_conn_req conn_req;
	bnep_conn_resp conn_resp;
	bnep_filter_set filter_set;
	bnep_filter_resp filter_resp;
	bnep_filter_multi_set multi_set;
	bnep_filter_multi_resp mult_resp;
    } u;

#ifdef ENABLE_BNEP_EXT_HDR
    bnep_ext_header ext[BNEP_MAX_EXT_HEADER];

    int ext_hdr_len() {
	if (!ext_bit) {
	    return 0;
	}
	int len = 0;
	for (int i = 0; i < BNEP_MAX_EXT_HEADER; i++) {
	    len += ext[i].length + 2;
	    if (!ext[i].ext_bit) {
		break;
	    }
	}
	return len;
    }
#endif

    int hdr_len() {
	int len;
	switch (type) {
	case BNEP_GENERAL_ETHERNET:
	    len = 15;
	    break;
	case BNEP_COMPRESSED_ETHERNET:
	    len = 3;
	    break;
	case BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY:
	case BNEP_COMPRESSED_ETHERNET_DEST_ONLY:
	    len = 9;
	    break;
	case BNEP_CONTROL:
	default:
	    len = 0;
	}

#ifdef ENABLE_BNEP_EXT_HDR
	if (ext_bit) {
	    len += ext_hdr_len();
	}
#endif
	return len;
    }
    static int offset_;
    inline static int &offset() { return offset_; }
    inline static hdr_bnep *access(Packet * p) {
	return (hdr_bnep *) p->access(offset_);
    }
};

class BNEP;
class BNEPTimer:public Handler {
  public:
    BNEPTimer(BNEP * b):_bnep(b) { }
    void handle(Event *);

  private:
    BNEP * _bnep;
};

struct BNEPSchedEntry {
    double length;
    Piconet *pico;		// NULL is for scanning
    BNEPSchedEntry *next, *prev;

    BNEPSchedEntry(Piconet * p, double l)
    :length(l), pico(p), next(this), prev(this) { }
};

class BNEPSendTimer:public Handler {
  public:
    BNEPSendTimer(BNEP * b):_bnep(b) { }
     void handle(Event *);

  private:
    BNEP * _bnep;
};

class BNEPInqCallback:public Handler {
  public:
    BNEPInqCallback(BNEP * b):_bnep(b) { }
    void handle(Event *);

  private:
    BNEP * _bnep;
};

struct BrTableEntry {
    struct BrTableEntry *next;
    int addr;			// macDA()
    int port;
    double ts;

    BrTableEntry(int a, int p, double t):addr(a), port(p), ts(t) { }
};

// Please note that bridge interface at BNEP is the general bridge 
// interface defined for 802.x family.  That is, something located 
// between LLC and MAC.  It is not the bridge node to interconnect 
// piconets into a scatternet.
//   Just think of a bridge port as a physical link.  Each port represents
// an ACL link.  So, a master and a bridge has multiple ports while a
// slave has a single port to its master.
class BridgeTable {
  public:
    BridgeTable():_table(0) { }
    ~BridgeTable() {
	while (_table) {
	    BrTableEntry *t = _table;
	    _table = _table->next;
	    delete t;
	}
    }

    void add(int addr, int port);
    void remove(int addr);
    void remove(double t);
    int lookup(int addr);
    void dump();

  private:
    BrTableEntry * _table;
};

#define PANU	0x01
#define GN 	0x02
#define NAP	0x04
#define BR	0x08
#define ROLEMASK (PANU|GN|NAP|BR)

class BNEP:public Mac {
  public:
    static int trace_all_bnep_;
    int trace_me_bnep_;

    // enum Role { PANU, GN, NAP, BR } role_;
    // Role primaryRole_;
    uchar rolemask_;
    uchar role_;
    int numRole_;		// == num of piconets ??

    int canBeBridge() { return 1 && (rolemask_ & BR); } 
    int canBeGN() { return 1 && (rolemask_ & GN); }
    int canBeNAP() { return 1 && (rolemask_ & NAP); }
    int canBePANU() { return 1 && (rolemask_ & PANU); }
    int canBeMaster() { return canBeGN() || canBeNAP(); }
    int canBePANUOnly() { return (rolemask_ == PANU); }

    void enableBridge() { rolemask_ |= BR; }
    void enableGN() { rolemask_ |= GN; }
    void enableNAP() { rolemask_ |= NAP; }
    void enablePANU() { rolemask_ |= PANU; }

    void disableBridge() { rolemask_ &= (ROLEMASK ^ BR); }
    void disableGN() { rolemask_ &= (ROLEMASK ^ GN); }
    void disableNAP() { rolemask_ &= (ROLEMASK ^ NAP); }
    void disablePANU() { rolemask_ &= (ROLEMASK ^ PANU); }

    int isBridge() { return 1 && (role_ & BR); }
    int isGN() { return 1 && (role_ & GN); }
    int isNAP() { return 1 && (role_ & NAP); }
    int isPANU() { return 1 && (role_ & PANU); }
    int isMaster() { return isNAP() || isGN(); }

    void becomeBridge() { role_ |= BR; }
    void becomeGN() { role_ |= GN; }
    void becomeNAP() { role_ |= NAP; }
    void becomePANU() { role_ |= PANU; }

    void removeBridge() { role_ &= (ROLEMASK ^ BR); }
    void removeGN() { role_ &= (ROLEMASK ^ GN); }
    void removeNAP() { role_ &= (ROLEMASK ^ NAP); }
    void removePANU() { role_ &= (ROLEMASK ^ PANU); }

    class Connection {
	Connection *_next;

      public:
	L2CAPChannel * cid;
	int daddr;
	int port;
	int _master;
	int _isBNEP;		// false -> interface to external network.
	// int ready;
	int ready_;
	char *_nscmd;

	void next(Connection * n) { _next = n; } 
	Connection *next() { return _next; }
	void setCmd(char *c) { _nscmd = c; }

	Connection(L2CAPChannel * c):cid(c) {
	    _nscmd = 0;
	    ready_ = 0;
	    port = -1;
	    daddr = cid->remote();
	    _master = 0;
	    _isBNEP = 1;
	}

	void dump(FILE *out = 0) {
	    if (out == 0) {
		out = stdout;
	    }
	    fprintf(out, "<%d %d>", daddr, port);
	}
    };

    // PANU has a single bnep connection
    // GN has 0-7 bnep connections
    // NAP = GN + 802.11/wired 
    // BR = PANU/GN/NAP + PANU + ...

    L2CAPChannel *_chan;
    BTNode *node_;
    LMP *lmp_;
    L2CAP *l2cap_;
    class SDP *sdp_;
    // Mac802_15_1 *mac_;
    bd_addr_t bd_addr_;
    bd_addr_t _master_bd_addr;
    // int _ondemand;
    // int _master;

    // int _inScan;
    Event _ev;
    BNEPTimer _timer;
    // double _scanIntv;
    // double _scanWind;

    Event send_ev;
    BNEPSendTimer sendTimer;

    BNEPInqCallback inqCallback;
    Bd_info *nb_;
    int nb_num;
    int waitForInq_;
    int numConnReq_;
    int schedsend;

    int onDemand_;
    int _in_make_pico;

    Connection **_conn;
    int num_conn;
    int num_conn_max;

    BridgeTable _br_table;

    int _masterPort;
    BNEPSchedEntry *_current;
    int _numSchedEntry;

    PacketQueue _q;
    // Packet * _curPkt;                // the packet to be transmited.

  public:

    BNEP();
    void setup(bd_addr_t ad, LMP * l, L2CAP * l2, SDP *, BTNode * node);

    void removeSchedEntry(Piconet * pico);
    void disableScan();
    void enableScan(double len);
    void addSchedEntry(Piconet * pico, double len);

    void inq(int to, int num);

    int findPort(int macDA);
    int findPortByIp(int ip);
    void portLearning(int, Packet *);
    Connection *connect(bd_addr_t addr,
			hdr_bt::packet_type pt = hdr_bt::NotSpecified,
			hdr_bt::packet_type rpt = hdr_bt::NotSpecified,
			Queue * ifq = 0);
    void disconnect(bd_addr_t addr, uchar reason);
    Connection *lookupConnection(bd_addr_t addr);
    Connection *lookupConnection(L2CAPChannel *);
    L2CAPChannel *lookupChannel(bd_addr_t addr);
    Connection *addConnection(L2CAPChannel *);
    void removeConnection(Connection *);
    void removeConnection(L2CAPChannel * ch);
    void channel_setup_complete(L2CAPChannel * ch);

    void inq_complete();
    void make_connections();
    void make_piconet();
    void addNeighbor(Bd_info * nb);
    void piconet_sched();
    void schedule_send(int);
    void handle_send();
    void _send();
    void bcast(Packet * p);

    void sendUp(Packet * p, Handler * h);
    void sendUp(Packet * p) {
	sendUp(p, (Handler *) 0);
    }
    void sendDown(Packet * p, Handler * h);
    // To override sendDown(Packet * p) in class MAC
    void sendDown(Packet * p) {
	sendDown(p, (Handler *) 0);
    }
    void dump_energy(FILE * out);
};

#endif				// __ns_bnep_h__

/*
 * Copyright (c) 2004,2005 University of Cincinnati, Ohio.
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

#ifndef __ns_hdr_bt__
#define __ns_hdr_bt__

#include "packet.h"
#include "bt.h"

struct SFmsg {
    uchar type;
    uchar code;
    uint16_t length;
    int target;
    uchar data[92];
};

struct L2CAPcmd {
    uchar code;
    uchar id;
    uint16_t length;
    uchar data[48];
};

struct hdr_l2cap {
    int length;
    class L2CAPChannel *cid;    // in spec, cid is 2 octets.
    int psm;
    // L2CAPcmd cmd;
};

struct PLSF_hello {
    bd_addr_t bd_addr_;
    int clk;
    int nextScan;
    char so;	// scan interval order
    char numSlave;

    void dump() {printf("%d clk:%d nscan:%d so:%d nslave: %d\n", 
		 bd_addr_, clk, nextScan, so, numSlave);}
};

struct hdr_bt {
    struct payload_hdr {
	uchar l_ch;		// 0: undefined 1: L2CAP-cont 2: L2CAP 3: LMP 
	uchar flow;		// at l2cap level
	short int length;
    };

    struct FHS_payload {
	bd_addr_t bd_addr_;
	int clk;
	int lt_addr_;		// Called AM_ADDR in spec 1.0 and 1.1
	uchar sr;		// Scan Repetition field. R0/R1/R2/Reserved
	int class_of_device;
	uchar page_scan_mode;
    };

    struct LMPcmd {
	uchar content[60];	// it is 16 in specs. 
	uchar opcode;
	uchar tranid;		// 0: master initiate  1: slave initiate
    };

    enum packet_type {
	Null, Poll, FHS, DM1, DH1, HV1, HV2, HV3, DV, AUX1,
	DM3, DH3, EV4, EV5, DM5, DH5, Id, EV3, DH1_2, DH1_3,
	DH3_2, DH3_3, DH5_2, DH5_3, EV3_2, EV3_3, EV5_2, EV5_3,
	HLO, NotSpecified, Invalid
    };

    static const char *packet_type_str_short(packet_type t) {
	return (t < Invalid ? _pktTypeName_s[t] : _pktTypeName_s[Invalid]);
    }

    static const char *packet_type_str(packet_type t) {
	return (t < Invalid ? _pktTypeName[t] : _pktTypeName[Invalid]);
    }

    static packet_type str_to_packet_type(const char *const s) {
	for (int i = 0; i < Invalid; i++) {
	    if (!strcmp(s, _pktTypeName[i])) {
		return (packet_type) i;
	    }
	}
	return Invalid;
    }

    static const char *const _pktTypeName [];
    static const char *const _pktTypeName_s [];

    // enum FLOW { STOP, GO };
    typedef uchar FLOW;
    // enum ARQN { NAK, ACK };
    typedef uchar ARQN;

    bd_addr_t ac;		// access_code is generated from BD_ADDR
    packet_type type;
    int clk;
    uchar lt_addr_;		// Called AM_ADDR in spec 1.0 and 1.1
    FLOW flow;
    ARQN arqn;
    uchar seqn;

    struct payload_hdr ph;
    struct hdr_l2cap l2caphdr;
    union {
	struct FHS_payload fhs;
	struct LMPcmd lmpcmd;
	struct L2CAPcmd l2capcmd;
	struct PLSF_hello hello;
	struct SFmsg sf;
    } u;

    class ConnectionHandle *connHand_;	// connect LMP and L2CAP

    // These are only for trace purpose.
    bd_addr_t sender;
    bd_addr_t receiver;
    // bd_addr_t target_;		// Intended receiver, ID pkt sender
    // // meaningful only if receiver == -1
    // // is 1 greater than receiver's address
    // void setTarget(bd_addr_t t) { target_ = t + 1; }
    // bd_addr_t target() { return target_ - 1; }
    // 
    char srcTxSlot;
    char dstTxSlot;
    // char recved;

    char fs_;			// frequency bank index
    char bcast;

    int16_t size;
    // int transmitId;
    int16_t transmitCount;
    int seqno;			// seq number for l2cap packet.
    static int pidcntr;
    int pid;

    char nokeep;

    double X_;
    double Y_;
    double Z_;
    double ts_;			// time stamp
    char *comment_;

    // time stamp for an higher layer multihop flow
    double flow_ts_;		
    double flow_ts_lasthop_;		
    int flow_seq_;
    int flow_seq_lasthop_;
    int hops_;
    // int flowid;

    class TxBuffer *txBuffer;
    Packet *reply;
    char nextState;
    char linktype;		// acl/sco

    short extinfo;	// for debug

    static int offset_;		// baseband.cc
    inline static int &offset() { return offset_; }
    inline static hdr_bt *access(Packet * p) {
	return (hdr_bt *) p->access(offset_);
    }

    inline double txtime() { return size * 1e-6; }
    inline double ts() { return ts_; }

    char *comment() { return comment_; }
    void comment(char *c) { comment_ = c; }

    bool isCRCAclPkt() {
	return isCRCAclPkt(type);
    }

    static bool isCRCAclPkt(packet_type pt) {
	return pt == DH1 || pt == DH3 || pt == DH5 ||
	    pt == DM1 || pt == DM3 || pt == DM5 ||
	    (pt >= DH1_2 && pt <= DH5_3);
    }

    bool isCRCPkt() {
	return isCRCPkt(type);
    }

    static bool isNonCRCPkt(packet_type pt) {
	return pt <= FHS || pt == HV1 || pt == HV2 || pt == HV3 || 
	    pt == AUX1 || pt == Id;
    }

    static bool isCRCPkt(packet_type pt) {
	return !isNonCRCPkt(pt);
    }

    bool isScoPkt() {	// need to update fore EVx.
	return type == HV3 || type == HV2 || type == HV1 ||
	    type == DM1 || type == DV;
    }

    bool scoOnly() {
	return type == HV3 || type == HV2 || type == HV1;
    }

    bool isAclPkt() {
	return !isScoPkt() || type == DM1;
    }

    static int slot_num(packet_type p) {
	static const int num[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
	    3, 3, 3, 3, 5, 5, 1, 1, 1, 1,
	    3, 3, 5, 5, 1, 1, 3, 3,
	    1, 1, 1
	};
	return num[p];
    }

    static int slot_num(uint16_t p) {
	if (p >= NotSpecified) {
	    printf("hdr::slot_num(): wrong packet_type.\n");
	    exit(-1);
	}
	packet_type pt = (packet_type) p;
	return slot_num(pt);
    }

    // bytes
    static int payload_size(packet_type p) {
	static const int size[] = { 0, 0, 0, 17, 27, 10, 20, 30, 9, 29, 
	    121, 183, 120, 180, 224, 339, 0, 30, 54, 83,
	    367, 552, 679, 1021, 60, 90, 360, 540,
	    27, 0, 0
	};

	if (p >= NotSpecified) {
	    printf("hdr::slot_num(): wrong packet_type.\n");
	    exit(-1);
	}
	return size[p];
    }

    static int payload_size(uint16_t p) {
	if (p >= NotSpecified) {
	    printf("hdr::slot_num(): wrong packet_type.\n");
	    exit(-1);
	}
	packet_type pt = (packet_type) p;
	return payload_size(pt);
    }

    static int payload_size(uint16_t p, int bytes) {
	packet_type pt = (packet_type) p;
	return payload_size(pt, bytes);
    }

    // This function returns the number of symbols needed to be transmitted
    // for the given packet, including the guide time/bits.  symbols are
    // transmited at 1M symbols per second.  Prior to spec 2.0, symbol rate
    // equals to bit rate.  It's no longer true in spec 2.0 or later.
    static int packet_size(packet_type p, int bytes) {
#if 0
	static int size[] = { 126, 126, 366, /* 1-18bytes 126+45 -- */ 366,	/* */
	    /*1-28bytes 126+32 -- */ 366, 366, 366, 366,
	    /*1-10bytes 126+80+45 -- */ 350, /*AUX1 1-30bytes 126+8 -- */ 366,	/* */
	    /*2-123bytes 126+60-- */ 1626, /*2-185bytes 126+32-- */ 1622, -1, -1,	/* */
	    /*2-226bytes 126+60-- */ 2871, /*2-341bytes 126+32-- */ 2870, 68
	};
#endif
	int payload;

	if (p >= NotSpecified) {
	    printf("hdr::slot_num(): wrong packet_type.\n");
	    exit(-1);
	}

	switch (p) {

	case Id:
	    return 68;		// 4 + 64

	case Null:
	case Poll:
	    return 126;		// 72 + 54

	case FHS:
	case HV1:		// 10 bytes
	case HV2:		// 20 bytes
	case HV3:		// 30 bytes
	    return 366;		// 126 + 240

	case DM1:		// 18 bytes include 1 byte payload hdr
	    if (bytes < 0 || bytes > 17) {
		bytes = 17;
	    }
	    payload = (bytes + 3) * 8;	// 1 byte hdr, 2 bytes CRC
	    payload = ((payload + 9) / 10) * 10;	// round it to 10's
	    payload = (payload * 3) / 2;
	    return payload + 126;

	case DH1:		// 28 bytes include 1 byte payload hdr
	    if (bytes < 0 || bytes > 27) {
		bytes = 27;
	    }
	    return (bytes + 3) * 8 + 126;

	case DM3:		// 123 bytes include 2 bytes payload hdr
	    if (bytes < 0 || bytes > 121) {
		bytes = 121;
	    }
	    payload = (bytes + 4) * 8;	// 2 byte hdr, 2 bytes CRC
	    payload = ((payload + 9) / 10) * 10;
	    payload = (payload * 3) / 2;
	    return payload + 126;

	case DM5:		// 226 bytes include 2 bytes payload hdr
	    if (bytes < 0 || bytes > 224) {
		bytes = 224;
	    }
	    payload = (bytes + 4) * 8;	// 2 byte hdr, 2 bytes CRC
	    payload = ((payload + 9) / 10) * 10;
	    payload = (payload * 3) / 2;
	    return payload + 126;

	case DH3:		// 185 bytes include 2 bytes payload hdr
	    if (bytes < 0 || bytes > 183) {
		bytes = 183;
	    }
	    return (bytes + 4) * 8 + 126;

	case DH5:		// 341 bytes include 2 bytes payload hdr
	    if (bytes < 0 || bytes > 339) {
		bytes = 339;
	    }
	    return (bytes + 4) * 8 + 126;

	case AUX1:		// 30 bytes include 2 bytes payload hdr
	    if (bytes < 0 || bytes > 29) {
		bytes = 29;
	    }
	    return (bytes + 3) * 8 + 126;

	case DV:		// 10 bytes include 1 byte payload hdr 
	    // + 10 bytes voice
	    if (bytes < 0 || bytes > 9) {
		bytes = 9;
	    }
	    payload = (bytes + 3) * 8;	// 1 byte hdr, 2 bytes CRC
	    payload = ((payload + 9) / 10) * 10;
	    payload = (payload * 3) / 2;
	    return payload + 126 + 80;

	case EV3:		// 1-30 bytes, 0 byte hdr, 2 bytes CRC 
	    if (bytes < 1 || bytes > 30) {
		bytes = 30;
	    }
	    return (bytes + 2) * 8 + 126;

	case EV4:		// 1-120 bytes, 0 byte hdr, 2 byte CRC, 2/3 FEC 
	    if (bytes < 1 || bytes > 120) {
		bytes = 120;
	    }
	    payload = (bytes + 2) * 8;	// 0 byte hdr, 2 bytes CRC
	    payload = ((payload + 9) / 10) * 10;
	    payload = (payload * 3) / 2;
	    return payload + 126;

	case EV5:		// 1-180 bytes, 0 byte hdr, 2 bytes CRC 
	    if (bytes < 1 || bytes > 180) {
		bytes = 180;
	    }
	    return (bytes + 2) * 8 + 126;

	case DH1_2:		// 0-54 bytes, 2 byte hdr, 2 bytes CRC 
	    if (bytes < 0 || bytes > 54) {
		bytes = 54;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return (bytes + 4) * 4 + 126 + 18;

	case DH1_3:		// 0-83 bytes, 2 byte hdr, 2 bytes CRC 
	    if (bytes < 0 || bytes > 83) {
		bytes = 83;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return ((bytes + 4) * 8 + 2) / 3 + 126 + 18;

	case DH3_2:		// 0-367 bytes, 2 byte hdr, 2 bytes CRC 
	    if (bytes < 0 || bytes > 367) {
		bytes = 367;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return (bytes + 4) * 4 + 126 + 18;

	case DH3_3:		// 0-552 bytes, 2 byte hdr, 2 bytes CRC 
	    if (bytes < 0 || bytes > 552) {
		bytes = 552;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return ((bytes + 4) * 8 + 2) / 3 + 126 + 18;

	case DH5_2:		// 0-679 bytes, 2 byte hdr, 2 bytes CRC 
	    if (bytes < 0 || bytes > 679) {
		bytes = 679;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return (bytes + 4) * 4 + 126 + 18;

	case DH5_3:		// 0-1021 bytes, 2 byte hdr, 2 bytes CRC 
	    if (bytes < 0 || bytes > 1021) {
		bytes = 1021;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return ((bytes + 4) * 8 + 2) / 3 + 126 + 18;

	case EV3_2:		// 1-60 bytes, 0 byte hdr, 2 bytes CRC 
	    if (bytes < 1 || bytes > 60) {
		bytes = 60;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return (bytes + 2) * 4 + 126 + 18;

	case EV3_3:		// 1-90 bytes, 0 byte hdr, 2 bytes CRC 
	    if (bytes < 1 || bytes > 90) {
		bytes = 90;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return ((bytes + 2) * 8 + 2) / 3 + 126 + 18;

	case EV5_2:		// 1-360 bytes, 0 byte hdr, 2 bytes CRC 
	    if (bytes < 1 || bytes > 360) {
		bytes = 360;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return (bytes + 2) * 4 + 126 + 18;

	case EV5_3:		// 1-540 bytes, 0 byte hdr, 2 bytes CRC 
	    if (bytes < 1 || bytes > 540) {
		bytes = 540;
	    }
	    // 5 bit guard time + 11 bit sync + 2 bit trail = 18 bits
	    return ((bytes + 2) * 8 + 2) / 3 + 126 + 18;

	case HLO:		
	    if (bytes < 1 || bytes > 180) {
		bytes = 27;
	    }
	    return (bytes + 2) * 8 + 126;

	default:
	    return -1;
	}
    }

    // return the maximum size of each BT packet, in terms of symbols.
    static int packet_size(packet_type p) {
	return packet_size(p, -1);
    }

    void dump();
    void dump(FILE *f, char stat, int ad, const char *st); // in baseband.cc
    void dump_sf();
};

#endif				// __ns_hdr_bt__

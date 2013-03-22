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

#ifndef __ns_l2cap_h__
#define __ns_l2cap_h__

#include "bi-connector.h"
#include "ip.h"
#include "baseband.h"
#include "lmp.h"

class L2CAP;

/* 
Data packet:  		     length(2) cid(2) payload(0-65535)
connectionless data channel: length(2) cid(2)=0x02 PSM(>=2) payload(0-65533)
Signal:		             length(2) cid(2)=0x01 cmd#1 cmd#2 ...
	MTU_sig = 48
	Command:  Code(1) Id(1)<>0 length(2) data(>=0, length)

******** moved to hdr_bt to overlap with LMPCmd **********
*/

class L2CAP_ConnectReq_Timer:public Handler {
  public:
    L2CAP_ConnectReq_Timer(L2CAP * l):_l2cap(l) { } 
    void handle(Event * e) { }
  private:
    L2CAP * _l2cap;
};

// in the specs, L2CAPChannel is represented by a 2-byte integer
// Here is referred by the pointer.
class L2CAPChannel {
    friend class L2CAP;
    friend class LMPLink;
    friend class LMP;
    friend class ConnectionHandle;
    friend class SDP;
    friend class BNEP;
    friend class BTNode;

  public:
    L2CAPChannel(L2CAP *, int psm, ConnectionHandle * connh,
		  L2CAPChannel * r, Queue *ifq = 0);

    inline bool match(int psm, bd_addr_t addr) {
	return (_psm == psm) && (_bd_addr == addr);
    } 

    void enque(Packet * p);
    void send();
    void send(Packet * p);

    inline uint16_t psm() { return _psm; }
    inline L2CAPChannel *rcid() { return _rcid; }
    inline L2CAPChannel *next() { return _next; }
    inline void next(L2CAPChannel * n) { _next = n; }
    inline ConnectionHandle *connhand() { return _connhand; }
    void changePktType(hdr_bt::packet_type pt);
    inline hdr_bt::packet_type packetType() {
	return _connhand->link->pt;
    }
    void changeRecvPktType(hdr_bt::packet_type pt);
    inline hdr_bt::packet_type recvPacketType() {
	return _connhand->link->rpt;
    }
    inline void setCmd(const char *c) { _nscmd = c; }
    inline void setQosParam(QosParam * qos) { _qos = qos; }
    inline bd_addr_t remote() { return _bd_addr; }

    void flush();
    void disconnect(uchar reason);
    void linkDetached();

    int failed;
    ConnectionHandle *_connhand;
    int ready_;
    uchar disconnReason;

  private:
    L2CAP * l2cap_;
    L2CAPChannel *_next;	// chain at L2CAP
    L2CAPChannel *linknext;	// chain at ConnectionHandle
    L2CAPChannel *_rcid;
    // int cid;
    int _psm;
    bd_addr_t _bd_addr;
    const char *_nscmd;
    QosParam *_qos;
    QosParam *_qosReq;
    hdr_bt::packet_type _packet_type;
    hdr_bt::packet_type _recv_packet_type;
    int _uplayerPacketRecvSize;
    Queue *_queue;
};

class L2CAP:public BiConnector {
    friend class BNEP;
    friend class L2CAPChannel;

    struct ConnReq {
	int psm;
	L2CAPChannel *scid;
    };

    struct ConnResp {
	L2CAPChannel *dcid;
	L2CAPChannel *scid;
	int result;
	int status;
    };

  private:
    L2CAPChannel * _chan;
    LMP *lmp_;
    class BNEP *bnep_;
    class SDP *sdp_;
    bd_addr_t bd_addr_;
    ConnectionHandle *_connhand;

  public:
    enum opcode {
	L2CAP_RESERVED,
	L2CAP_COMMAND_REJECT,
	L2CAP_CONNECTION_REQUEST,
	L2CAP_CONNECTION_RESPONSE,
	L2CAP_CONFIGURE_REQUEST,
	L2CAP_CONFIGURE_RESPONSE,
	L2CAP_DISCONNECTION_REQUEST,
	L2CAP_DISCONNECTION_RESPONSE,
	L2CAP_ECHO_REQUEST,
	L2CAP_ECHO_RESPONSE,
	L2CAP_INFORMATION_REQUEST,
	L2CAP_INFORMATION_RESPONSE
    };

    static const char *opcode_str(int i) {
	static const char *const str[] = {
	    "L2CAP_RESERVED",
	    "L2CAP_COMMAND_REJECT",
	    "L2CAP_CONNECTION_REQUEST",
	    "L2CAP_CONNECTION_RESPONSE",
	    "L2CAP_CONFIGURE_REQUEST",
	    "L2CAP_CONFIGURE_RESPONSE",
	    "L2CAP_DISCONNECTION_REQUEST",
	    "L2CAP_DISCONNECTION_RESPONSE",
	    "L2CAP_ECHO_REQUEST",
	    "L2CAP_ECHO_RESPONSE",
	    "L2CAP_INFORMATION_REQUEST",
	    "L2CAP_INFORMATION_RESPONSE"
	};
	return str[i];
    };

    char *ifq_;			// IFQ type
    int ifq_limit_;
    L2CAPChannel *bcastCid;	// cid = 0x0002
    static int trace_all_l2cap_cmd_;
    int trace_me_l2cap_cmd_;

    L2CAP();
    inline bd_addr_t bd_addr() { return bd_addr_; }
    void setup(bd_addr_t ad, LMP * l, class BNEP *, class SDP *,
	       BTNode * node);

    void sendDown(Packet *, Handler *);
    void sendUp(Packet *, Handler *);
    Packet *_reassemble(Packet * p, ConnectionHandle *);

    void l2capCommand(uchar code, uchar * content, int len,
		      ConnectionHandle * connh);
    void registerChannel(L2CAPChannel *);
    void removeChannel(L2CAPChannel *);
    ConnectionHandle *lookupConnectionHandle(bd_addr_t bd);
    void addConnectionHandle(ConnectionHandle *);
    void removeConnectionHandle(ConnectionHandle *);
    L2CAPChannel *lookupChannel(uint16_t psm, bd_addr_t bd);
    int connection_complete_event(ConnectionHandle *, int type,
				  int status);
    void connection_ind(ConnectionHandle *);
    void _channel_setup_complete(L2CAPChannel *);

    L2CAPChannel *L2CA_ConnectReq(bd_addr_t bd_addr, uint16_t psm = 0);
    L2CAPChannel *L2CA_ConnectReq(bd_addr_t bd_addr, uint16_t psm, Queue *ifq);
    int L2CA_ConnectRsp(ConnectionHandle *connh, ConnReq *connreq);
    int L2CA_DisconnectReq(L2CAPChannel * dcid, L2CAPChannel *scid);
    int L2CA_DisconnectRsp(ConnectionHandle * connh, ConnResp * req);

    // Read/Write is implemented as the ns way: sendUp/Down
    // int L2CA_DataWrite(uint16_t cid, int length, char *outbuffer, int *size);
    // int L2CA_DataRead(uint16_t cid, int length, char *inbuffer, int *N);

    int L2CA_GroupCreate(uint16_t psm);
    int L2CA_GroupAddMember(uint16_t cid, bd_addr_t bd_addr);
    int L2CA_GroupRemoveMember(uint16_t cid, bd_addr_t bd_addr);
    int L2CA_GroupMembership(uint16_t cid, int *N,
			     bd_addr_t * bd_addr_lst);
    int L2CA_Ping(bd_addr_t bd_addr, char *echo_data, uint16_t * length);
    int L2CA_GetInfo(bd_addr_t bd_addr, uint16_t infoType, char *infodata,
		     uint16_t size);
    int L2CA_DisableCLT(uint16_t psm);
    int L2CA_EnableCLT(uint16_t psm);

  protected:
    int command(int argc, const char *const *argv);
};

#endif				// __ns_l2cap_h__

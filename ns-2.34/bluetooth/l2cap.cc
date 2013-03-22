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

/*
 *	l2cap.cc
 */

#include "l2cap.h"
#include "packet.h"
#include "bnep.h"
#include "sdp.h"

#define BUFFSIZE 1024


/*
 * Note:
 *
 * L2CAP is the only 'transport layer (BT term)' protocols.  
 * SCO connection is handled by a seperated ScoAgent.
 *
 * ConnectionHandle is used to connect L2CAP and LMP/BB.  By specs, 
 * ConnectionHandle is a 16-bit integer.  LMP and L2CAP might have 
 * different data struct to keep info associated with ConnectionHandle.
 * In this simulator, we simply it as a single data structure and
 * pass pointer as id.
 *
 * At LMP, 
 * 	ConnectionHandle is 1-1 mapped to Link.
 *
 * At L2CAP,
 * 	Several Channels may share the same ConnectionHandle.
 *
 * In a similar fashion.  CID of a L2CAP Channel is represented by the
 * pointer to the c++ object, except CID 0 and CID 1.  It is still counted
 * as 2 octets when computing packet length.  CID 0 as the Signal/Cmd 
 * channel, which don't have a c++ object because any Signal/Cmd
 * is not buffered and send to LMP right away.
 *
 */

static class L2CAPClass:public TclClass {
  public:
    L2CAPClass():TclClass("L2CAP") {} 
    TclObject *create(int, const char *const *) {
	return new L2CAP();
    }
} class_l2cap;

//////////////////////////////////////////////////////////
//                      L2CAPChannel                    //
//////////////////////////////////////////////////////////
L2CAPChannel::L2CAPChannel(L2CAP * l2c, int psm, ConnectionHandle * connh,
			   L2CAPChannel * r, Queue * ifq)
{
    Tcl & tcl = Tcl::instance();

    l2cap_ = l2c;
    _psm = psm;

    _next = NULL;
    linknext = this;
    _bd_addr = -1;
    _connhand = NULL;
    _nscmd = NULL;
    _qos = NULL;
    _qosReq = NULL;
    _packet_type = hdr_bt::DH1;
    _recv_packet_type = hdr_bt::DH1;
    ready_ = 0;
    disconnReason = BTDISCONN_LINKDOWN;
    failed = 0;
    _uplayerPacketRecvSize = 0;

    if (ifq) {
	_queue = ifq;
    } else {
	tcl.evalf("new %s", l2cap_->ifq_);
	const char *res = tcl.result();
	_queue = (Queue *) TclObject::lookup(res);
	if (l2cap_->ifq_limit_ > 0) {
	    tcl.evalf("%s set limit_ %d", res, l2cap_->ifq_limit_);
	}
    }

    _rcid = r;
    if (connh) {
	if (connh->link) {
	    _bd_addr = connh->link->remote->bd_addr_;
	}
	_connhand = connh;
	connh->add_channel(this);
    }
#if 0
    int myad = -2;
    if (l2cap_ && l2cap_->lmp_ && l2cap_->lmp_->bb_) {
	myad = l2cap_->lmp_->bb_->bd_addr_;
    }
    fprintf(stderr, "%d->%d %s qlim_ %d\n",
	    myad, _bd_addr, _queue->name(), _queue->limit());
#endif
}

void L2CAPChannel::disconnect(uchar reason)
{
    disconnReason = reason;
    l2cap_->L2CA_DisconnectReq(_rcid, this);
}

void L2CAPChannel::changePktType(hdr_bt::packet_type pt)
{
    if (hdr_bt::isCRCAclPkt(pt)) {
	if (_connhand) {
	    _connhand->packet_type = pt;
	    if (_connhand->link) {
		l2cap_->lmp_->changePktType(_connhand->link);
	    }
	} else {
	    l2cap_->lmp_->defaultPktType_ = pt;
	}
    } else {
	fprintf(stderr,
		"L2CAPChannel::changePktType(): wrong packet type");
    }
}

void L2CAPChannel::changeRecvPktType(hdr_bt::packet_type pt)
{
    if (hdr_bt::isCRCAclPkt(pt)) {
	if (_connhand) {
	    _connhand->recv_packet_type = pt;
	    if (_connhand->link) {
		l2cap_->lmp_->changePktType(_connhand->link);
	    }
	} else {
	    l2cap_->lmp_->defaultRecvPktType_ = pt;
	}
    } else {
	fprintf(stderr,
		"L2CAPChannel::changeRecvPktType(): wrong packet type");
    }
}

void L2CAPChannel::send()
{
    if (!ready_ || _connhand->link->suspended || _connhand->link->_parked) {
	return;
    }
    Packet *p = _queue->deque();
    if (p) {
	send(p);
    }
}

void L2CAPChannel::flush()
{
    Packet *p;

    while ((p = _queue->deque())) {
	Packet::free(p);
    }
}

void L2CAPChannel::enque(Packet * p)
{
    _queue->enque(p);
}

void L2CAPChannel::send(Packet * p)
{
    if (!_connhand || !_connhand->link) {
	return;
    }

    hdr_cmn *ch = HDR_CMN(p);
    hdr_bt *bh = HDR_BT(p);
    // hdr_l2cap *lh = &bh->u.l2cap;
    hdr_l2cap *lh = &bh->l2caphdr;

    lh->length = ch->size();
    ch->size() += 4;
    bh->ph.l_ch = L_CH_L2CAP_START;
    bh->connHand_ = _connhand;

    if (lh->cid == L2CAP_SIG_CID) {
	bh->comment("L2");
    } else if (lh->cid == L2CAP_BCAST_CID) {
	bh->comment("Lb");
    } else {
	lh->cid = _rcid;
	bh->comment("LD");
    }
    _connhand->link->enqueue(p);
}

void L2CAPChannel::linkDetached()
{
    if (l2cap_->bnep_) {
	l2cap_->bnep_->removeConnection(this);
    }
    _connhand->remove_channel(this);
    l2cap_->removeChannel(this);
    if (_connhand->numChan == 0) {
	l2cap_->removeConnectionHandle(_connhand);
    }
}

//////////////////////////////////////////////////////////
//                      L2CAP                           //
//////////////////////////////////////////////////////////
int L2CAP::trace_all_l2cap_cmd_ = 1;

L2CAP::L2CAP()
:  _chan(0), _connhand(0)
{
    bind("ifq_limit_", &ifq_limit_);

    ifq_ = L2CAP_IFQ;
    ifq_limit_ = L2CAP_IFQ_LIMIT;

    bcastCid = new L2CAPChannel(this, 0, 0, 0);
    // bcastCid->cid = L2CAP_BCAST_CID;

    trace_me_l2cap_cmd_ = 0;
}

void L2CAP::setup(bd_addr_t ad, LMP * l, class BNEP * b, class SDP * s,
		  BTNode * node)
{
    bd_addr_ = ad;
    lmp_ = l;
    bnep_ = b;
    sdp_ = s;
}

int L2CAP::command(int argc, const char *const *argv)
{
    // Tcl & tcl = Tcl::instance();
    if (argc == 3) {
	if (!strcmp(argv[1], "LMP")) {
	}
    }

    return BiConnector::command(argc, argv);
}

//   1. Signalling Command Packet Format
//
//   | byte0  . byte1  | byte2  . byte 3 |
//   -------------------------------------
//   |    length       |   0x0001        |
//   |-----------------------------------|
//   |            Command #1             |
//   |-----------------------------------|
//   |            Command #2             |
//   -------------------------------------
//
//   2. Command format
//
//   | byte0  . byte1  | byte2  . byte 3 |
//   -------------------------------------
//   |   Code |  ident |   Length        |
//   |-----------------------------------|
//   |               data                |
//   -------------------------------------
//
//   Current impl only handle single command per packet
void L2CAP::l2capCommand(uchar code, uchar * content, int len,
			 ConnectionHandle * connh)
{
    Packet *p = Packet::alloc();
    hdr_cmn *ch = HDR_CMN(p);
    ch->ptype() = PT_L2CAP;
    hdr_bt *bh = HDR_BT(p);
    bh->pid = hdr_bt::pidcntr++;
    // hdr_l2cap *lh = &bh->u.l2cap;
    hdr_l2cap *lh = &bh->l2caphdr;
    L2CAPcmd *cmd = &bh->u.l2capcmd;
    cmd->code = code;
    cmd->length = len;
    lh->cid = L2CAP_SIG_CID;
    if (len > 0) {
	memcpy(cmd->data, content, len);
    }
    HDR_CMN(p)->size() = len + 4;

    // sinalling packet has higher priority over data packet.  Send to
    // LMPLink directly
    connh->chan->send(p);

    if ((trace_all_l2cap_cmd_ || trace_me_l2cap_cmd_) && connh && connh->link) {
	fprintf(BtStat::log_, L2CAPPREFIX0 "%d->%d %f %s\n",
		bd_addr_, connh->link->remote->bd_addr_,
		Scheduler::instance().clock(), opcode_str(cmd->code));
    }

}

void L2CAP::registerChannel(L2CAPChannel * ch)
{
    L2CAPChannel *wk = _chan;
    while (wk) {
	if (wk == ch) {
	    fprintf(stderr, " ** Channel has been registered.\n");
	    return;
	}
	wk = wk->next();
    }

    ch->next(_chan);
    _chan = ch;
}

void L2CAP::removeChannel(L2CAPChannel * ch)
{
    if (!_chan) {
	return;
    }

    if (_chan == ch) {
	_chan = _chan->next();
	ch->flush();

    } else {
	L2CAPChannel *wk = _chan->next();
	L2CAPChannel *par = _chan;

	while (wk) {
	    if (wk == ch) {
		par->next(wk->next());
		ch->flush();
		break;
	    }
	    par = wk;
	    wk = wk->next();
	}
    }

}

L2CAPChannel *L2CAP::lookupChannel(uint16_t psm, bd_addr_t bd)
{
    L2CAPChannel *wk = _chan;
    while (wk) {
	if (wk->match(psm, bd)) {
	    return wk;
	}
	wk = wk->_next;
    }
    return NULL;
}

ConnectionHandle *L2CAP::lookupConnectionHandle(bd_addr_t bd)
{
    ConnectionHandle *wk = _connhand;
    while (wk) {
	if (wk->link && wk->link->remote->bd_addr_ == bd) {
	    return wk;
	}
	wk = wk->next;
    }
    return NULL;
}

void L2CAP::addConnectionHandle(ConnectionHandle * connh)
{
    ConnectionHandle *wk = _connhand;
    while (wk) {
	if (wk == connh) {
	    return;
	}
	wk = wk->next;
    }
    connh->next = _connhand;
    _connhand = connh;
}

void L2CAP::removeConnectionHandle(ConnectionHandle * connh)
{
    if (!_connhand) {
	return;
    } else if (connh == _connhand) {
	_connhand = _connhand->next;
	return;
    }
    ConnectionHandle *wk = _connhand->next;
    ConnectionHandle *par = _connhand;
    while (wk) {
	if (wk == connh) {
	    par->next = wk->next;
	    return;
	}
	par = wk;
	wk = wk->next;
    }
}


// reassembling per ConnectionHandle. 
// i.e. When multiple L2CAP Channel compete for ACL, the schedule
// unit is L2CAP packet instead of the fragmentations.
// specs says so.
Packet *L2CAP::_reassemble(Packet * p, ConnectionHandle * connh)
{
    hdr_cmn *ch = HDR_CMN(p);
    hdr_bt *bh = HDR_BT(p);
    hdr_l2cap *lh = &bh->l2caphdr;

    if (bh->ph.l_ch == L_CH_L2CAP_START) {
	connh->pktLen = lh->length;
	if (bh->ph.length >= lh->length + 4) {	// unfrag'd packet
	    ch->size() = lh->length;
	    connh->pktRecvedLen = 0;
	    return p;

	} else {		// first fragment
	    connh->pktRecvedLen = bh->ph.length;
	    Packet::free(p);
	}

    } else {			// cont'd fragments

	if (bh->ph.length == 0) {	//Flush packet.
	    connh->pktRecvedLen = 0;
	    Packet::free(p);
	    return p;
	}

	connh->pktRecvedLen += bh->ph.length;
	if (connh->pktRecvedLen >= connh->pktLen + 4) {	// last fragment
	    ch->size() = connh->pktLen;
	    connh->pktRecvedLen = 0;
	    return p;

	} else {
	    Packet::free(p);
	}
    }
    return NULL;
}

void L2CAP::sendDown(Packet * p, Handler * h)
{
    // used to send BCAST only ??
    fprintf(stderr, "L2CAP::sendDown(): Please send to L2CAPChannel. \n");
    abort();
}

void L2CAP::sendUp(Packet * p, Handler * h)
{
    hdr_bt *bh = HDR_BT(p);
    hdr_l2cap *lh = &bh->l2caphdr;
    // ConnectionHandle *connh = lookupConnectionHandle(bh->sender);
    ConnectionHandle *connh = bh->connHand_;

    if (!connh) {
	fprintf(stderr, "** %s: cannot find ConnectionHandle for sender %d"
			" -- maybe disconnected: \n",
		__FUNCTION__, HDR_BT(p)->sender);
	bh->dump(stderr, '!', lmp_->bb_->bd_addr_, lmp_->bb_->state_str_s());
	// abort();
	return;
    }

    if ((p = _reassemble(p, connh)) == NULL) {
	return;
    }

    if (lh->cid == L2CAP_SIG_CID) {	// l2cap signaling
	// uchar buff[BUFFSIZE];
	ConnReq *connreq;
	ConnResp *connresp;
	L2CAPChannel *lcid = NULL;
	uchar reason;
	L2CAPcmd *cmd = &bh->u.l2capcmd;

	if (trace_all_l2cap_cmd_ || trace_me_l2cap_cmd_) {
	    fprintf(BtStat::log_, L2CAPPREFIX1 "%d->%d %f %s\n",
		    HDR_BT(p)->sender, HDR_BT(p)->receiver,
		    Scheduler::instance().clock(), opcode_str(cmd->code));
	}

	switch (cmd->code) {
	case L2CAP_COMMAND_REJECT:
	    break;

	case L2CAP_CONNECTION_REQUEST:
	    connreq = (ConnReq *) cmd->data;
	    L2CA_ConnectRsp(connh, connreq);
	    break;

	case L2CAP_CONNECTION_RESPONSE:
	    connresp = (ConnResp *) cmd->data;
	    connresp->scid->_rcid = connresp->dcid;
	    // connresp->dcid->ready_ = 1;
	    _channel_setup_complete(connresp->scid);
	    break;

	case L2CAP_CONFIGURE_REQUEST:
	    break;
	case L2CAP_CONFIGURE_RESPONSE:
	    break;
	case L2CAP_DISCONNECTION_REQUEST:
	    connresp = (ConnResp *) cmd->data;
	    L2CA_DisconnectRsp(connh, connresp);
	    break;

	case L2CAP_DISCONNECTION_RESPONSE:
	    connresp = (ConnResp *) cmd->data;
	    // check if dcid, scid match. If not, discard silently.
	    lcid = connresp->scid;
	    if (lcid->_connhand != connh || lcid->_rcid != connresp->dcid) {
		break;
	    }

	    reason = lcid->disconnReason;
	    connh->remove_channel(lcid);
	    removeChannel(lcid);
	    if (connh->numChan == 0) {
		lmp_->HCI_Disconnect(connh, reason);	//
		removeConnectionHandle(connh);
	    }
	    break;

	case L2CAP_ECHO_REQUEST:
	    break;
	case L2CAP_ECHO_RESPONSE:
	    break;
	case L2CAP_INFORMATION_REQUEST:
	    break;
	case L2CAP_INFORMATION_RESPONSE:
	    break;
	default:
	    fprintf(stderr, "L2CAP_CMD: unknown command: %d\n", cmd->code);
	}
	Packet::free(p);

    } else if (lh->cid == L2CAP_BCAST_CID) {	// connectionless
	fprintf(stderr, "L2CAP Broadcast is not supported yet.\n");
	abort();

	// Data
    } else {
	lh->length = 0;		// clear L2CAP header
	if (bnep_ && lh->cid->_psm == PSM_BNEP) {
	    bnep_->recv(p, 0);
	} else if (sdp_ && lh->cid->_psm == PSM_SDP) {
	    sdp_->recv(p, lh->cid);
	} else {
	    fprintf(stderr, "L2CAP::sendUp(): Wrong PSM: %d.\n",
		    lh->cid->_psm);
	    abort();
	}
    }
}

L2CAPChannel *L2CAP::L2CA_ConnectReq(bd_addr_t bd_addr, uint16_t psm)
{
    L2CAPChannel *ch = lookupChannel(psm, bd_addr);
    if (ch) {
	if (!ch->failed) {
	    return ch;

	} else {		// reset this Channel
	    ch->failed = 0;
	    ch->ready_ = 0;
	}
    }

    Bd_info *bd;
    // Bd_info is kept at LMP to sumplify code.
    // In reality, it is kept at the host.
    if ((bd = lmp_->lookupBdinfo(bd_addr)) == NULL) {
	bd = new Bd_info(bd_addr, 0);
    }

    ConnectionHandle *connh =
	lmp_->HCI_Create_Connection(bd->bd_addr_, lmp_->defaultPktType_,
				    bd->sr_, bd->page_scan_mode_, bd->offset_,
				    lmp_->allowRS_);
    if (connh) {
	if (!ch) {
	    ch = new L2CAPChannel(this, psm, connh, 0);
	    ch->_bd_addr = bd_addr;
	    registerChannel(ch);
	} else {
	    ch->_connhand = connh;
	    connh->add_channel(ch);
	}

	connh->recv_packet_type = lmp_->defaultRecvPktType_;
	addConnectionHandle(connh);
	// ch->_connhand->add_channel(ch);
	ch->_connhand->reqCid = ch;
	if (ch->_connhand->ready_) {	// AclLink exists.
	    connection_complete_event(ch->_connhand, 0, 1);
	}
	return ch;
    } else {
	// delete ch; // ???
	abort();
	return NULL;
    }
}

L2CAPChannel *L2CAP::L2CA_ConnectReq(bd_addr_t bd_addr, uint16_t psm,
				     Queue * ifq)
{
    if (ifq) {
	L2CAPChannel *ch = new L2CAPChannel(this, psm, 0, 0, ifq);
	ch->failed = 1;		// Sorry for possible misleading.
	ch->_bd_addr = bd_addr;
	registerChannel(ch);
    }
    return L2CA_ConnectReq(bd_addr, psm);
}

int L2CAP::connection_complete_event(ConnectionHandle * connh,
				     int type, int status)
{
    L2CAPChannel *ch = connh->reqCid;
    if (!status) {
	fprintf(stderr, "L2CAP::ConnectReq is rejected.\n");
	connh->remove_channel(ch);
	removeConnectionHandle(connh);
	delete connh;
	ch->_connhand = NULL;
	ch->failed = 1;
	// report to higher layer.
	return 0;
    }

    connh->ready_ = 1;
    // connh->add_channel(ch);
    // addConnectionHandle(connh);

    ConnReq connreq;
    connreq.scid = ch;
    connreq.psm = ch->_psm;
    l2capCommand(L2CAP_CONNECTION_REQUEST,
		 (uchar *) & connreq, sizeof(ConnReq), ch->_connhand);

    return 1;
}

void L2CAP::_channel_setup_complete(L2CAPChannel * ch)
{
    ch->ready_ = 1;
    if (bnep_ && ch->psm() == PSM_BNEP) {
	bnep_->channel_setup_complete(ch);
	return;
    } else if (sdp_ && ch->psm() == PSM_SDP) {
	sdp_->channel_setup_complete(ch);
	return;
    }
    // handle other PSM handle code here
    // ...

    if (ch->_nscmd) {
	Tcl & tcl = Tcl::instance();
	tcl.eval(ch->_nscmd);
	ch->_nscmd = 0;
    }
    ch->send();
}

void L2CAP::connection_ind(ConnectionHandle * connh)
{
    // PSM is set to 0 temporarily.  Probably should set to 0x01 instead.
    // Well. Channel with PSM 0x01 does always implicitly exist.
    L2CAPChannel *ch = new L2CAPChannel(this, 0, connh, NULL);
    addConnectionHandle(connh);
    registerChannel(ch);
}

int L2CAP::L2CA_ConnectRsp(ConnectionHandle * connh,
			   L2CAP::ConnReq * connreq)
{
    ConnResp resp;
    L2CAPChannel *lcid;

    //// better scheme ????
    // When a link is up, there is always a Channel there.
    // We are here reusing the Command/Signal Channel.
    if (!connh->chan->ready_) {
	lcid = connh->chan;

	// Multiple Channel on a sigle ConnHand.
    } else {
	lcid = new L2CAPChannel(this, 0, connh, NULL);
	registerChannel(lcid);
    }

    lcid->_psm = connreq->psm;	// set PSM
    lcid->_rcid = connreq->scid;

    resp.dcid = lcid;
    resp.scid = connreq->scid;

    l2capCommand(L2CAP_CONNECTION_RESPONSE,
		 (uchar *) & resp, sizeof(ConnResp), connh);

    lcid->ready_ = 1;
    _channel_setup_complete(lcid);
    return 1;
}

int L2CAP::L2CA_DisconnectReq(L2CAPChannel * dcid, L2CAPChannel * scid)
{
    ConnResp req;		// reuse the struct.
    req.dcid = dcid;
    req.scid = scid;

    l2capCommand(L2CAP_DISCONNECTION_REQUEST,
		 (uchar *) & req, sizeof(ConnResp), scid->_connhand);
    return 1;
}

int L2CAP::L2CA_DisconnectRsp(ConnectionHandle * connh,
			      L2CAP::ConnResp * req)
{
    // check if dcid, scid match. If not, discard silently.
    L2CAPChannel *lcd = req->dcid;
    if (lcd->_connhand != connh || lcd->_rcid != req->scid) {
	return 0;
    }

    l2capCommand(L2CAP_DISCONNECTION_RESPONSE,
		 (uchar *) req, sizeof(ConnResp), connh);
    if (bnep_) {
	bnep_->removeConnection(lcd);
    }
    connh->remove_channel(lcd);
    removeChannel(lcd);
    if (connh->numChan == 0) {
	removeConnectionHandle(connh);
    }

    return 0;
}


///////////////////////////////////////////////////
// The following functions are not implemented   //
///////////////////////////////////////////////////
int L2CAP::L2CA_GroupCreate(uint16_t psm)
{
    return 0;
}

int L2CAP::L2CA_GroupAddMember(uint16_t cid, bd_addr_t bd_addr)
{
    return 0;
}

int L2CAP::L2CA_GroupRemoveMember(uint16_t cid, bd_addr_t bd_addr)
{
    return 0;
}

int L2CAP::L2CA_GroupMembership(uint16_t cid, int *N,
				bd_addr_t * bd_addr_lst)
{
    return 0;
}
int L2CAP::L2CA_Ping(bd_addr_t bd_addr, char *echo_data, uint16_t * length)
{
    return 0;
}

int L2CAP::L2CA_GetInfo(bd_addr_t bd_addr, uint16_t infoType,
			char *infodata, uint16_t size)
{
    return 0;
}

int L2CAP::L2CA_DisableCLT(uint16_t psm)
{
    return 0;
}

int L2CAP::L2CA_EnableCLT(uint16_t psm)
{
    return 0;
}

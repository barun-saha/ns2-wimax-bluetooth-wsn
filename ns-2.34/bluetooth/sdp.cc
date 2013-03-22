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

/*
 *	sdp.cc
 */

#include "sdp.h"
#include "baseband.h"


int hdr_sdp::offset_;

static class SDPHeaderClass:public PacketHeaderClass {
  public:
    SDPHeaderClass():PacketHeaderClass("PacketHeader/SDP", sizeof(hdr_sdp)) {
	bind_offset(&hdr_sdp::offset_);
    }
} class_sdphdr;

static class SDPclass:public TclClass {
  public:
    SDPclass():TclClass("SDP") {}
    TclObject *create(int, const char *const *) {
	return (new SDP());
    }
} class_sdp_agent;

void SDPTimer::handle(Event * e)
{
}

void SDPInqCallback::handle(Event *)
{
    sdp_->inq_complete();
}

SDP::SDP()
: timer_(this), inqCallback_(this), lastInqT_(-9999),
inqTShred_(60), inInquiry_(0), conn_(0), numConn_(0), connNumShred_(1),
nbr_(0), numNbr_(0), transid_(0), q_()
{}

void SDP::inq_complete()
{
    if (nbr_) {
	lmp_->destroyNeighborList(nbr_);
    }
    nbr_ = lmp_->getNeighborList(&numNbr_);
    inInquiry_ = 0;
    // waitForInq_ = 0;
    // make_connections();

    if (numNbr_ <= 0) {
	printf("*** Ooops, no neighbors are found. Inquiry failed.\n");
	return;
    }
    Bd_info *wk = nbr_;
    for (int i = 0; i < numNbr_; i++) {
	connect(wk->bd_addr_);
	wk = wk->next_;
    }
    sendToAll();
}

SDP::Connection * SDP::lookupConnection(bd_addr_t addr)
{
    Connection *wk = conn_;
    while (wk) {
	if (wk->daddr_ == addr) {
	    return wk;
	}
	wk = wk->next_;
    }
    return NULL;
}

SDP::Connection * SDP::addConnection(L2CAPChannel * ch)
{
    Connection *c = new Connection(this, ch);
    c->next_ = conn_;
    conn_ = c;
    numConn_++;
    return c;
}

void SDP::removeConnection(SDP::Connection * c)
{
    if (c == conn_) {
	conn_ = conn_->next_;
	delete c;
	return;
    }

    Connection *par = conn_;
    Connection *wk = conn_->next_;
    while (wk) {
	if (wk == c) {
	    par->next_ = wk->next_;
	    delete c;
	    return;
	}
	par = wk;
	wk = wk->next_;
    }
}

void SDP::channel_setup_complete(L2CAPChannel * ch)
{
    Connection *c = lookupConnection(ch->_bd_addr);
    if (!c) {
	c = addConnection(ch);
    }
    c->ready_ = 1;
    c->send();
}

SDP::Connection * SDP::connect(bd_addr_t addr)
{
    Connection *c;
    if ((c = lookupConnection(addr))) {
	return c;
    }

    L2CAPChannel *ch = l2cap_->L2CA_ConnectReq(addr, PSM_SDP);

    c = addConnection(ch);
    if (ch->ready_) {
	c->ready_ = 1;
    }

    return c;
}

void SDP::setup(bd_addr_t ad, LMP * l, L2CAP * l2, BTNode * node)
{
    bd_addr_ = ad;
    l2cap_ = l2;
    lmp_ = l;
    btnode_ = node;
}

int SDP::command(int argc, const char *const *argv)
{
    if (argc == 2) {
	if (!strcmp(argv[1], "test")) {
	    uchar req[128];
	    int len = 1;
	    SDP_serviceSearchReq(req, len);
	    return (TCL_OK);
	}
    }
    return Connector::command(argc, argv);
}

void SDP::inquiry()
{
    printf("%d SDP start inquiry().\n", bd_addr_);
    lmp_->HCI_Inquiry(lmp_->giac_, 4, 7);	// 4x1.28s
    inInquiry_ = 1;
    lmp_->addInqCallback(&inqCallback_);
}

void SDP::inqAndSend(Packet * p)
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();

    q_.enque(p);
    if (lastInqT_ + inqTShred_ < now && !inInquiry_) {
	inquiry();		// start inquiry process
	return;
    }

    int i;
    if (numConn_ < connNumShred_) {
	Bd_info *wk = nbr_;
	for (i = 0; i < numNbr_ - 1; i++) {
	    connect(wk->bd_addr_);
	}
    }
    sendToAll();
}

void SDP::sendToAll()
{
    assert(conn_);

    Connection *c;
    Packet *p;

    while ((p = q_.deque())) {
	c = conn_;
	for (int i = 0; i < numConn_ - 1; i++) {	// bcast
	    c->cid_->enque(p->copy());
	    c = c->next_;
	}
	c->cid_->enque(p);
    }
}

Packet *SDP::gen_sdp_pkt(uchar * req, int len)
{
    Packet *p = Packet::alloc(len);
    hdr_sdp *sh = HDR_SDP(p);
    hdr_cmn *ch = HDR_CMN(p);
    ch->ptype() = PT_SDP;
    ch->size() = len + 5;
    // sh->transid = _transid++;
    sh->paramLen_ = len;
    memcpy(p->accessdata(), req, len);

    return p;
}

void SDP::recv(Packet * p, L2CAPChannel * ch)
{
    hdr_sdp *sh = HDR_SDP(p);

    char buf[1024];
    int len = 1024;
    printf("%d SDP::recv() from %d - %s\n", bd_addr_, ch->remote(),
	   sh->dump(buf, len));

    switch (sh->pduid_) {
    case SDP_ErrorResp:
	break;

    case SDP_ServSrchReq:
	printf("Got SDP_ServSrchReq.\n");
	SDP_serviceSearchRsp(p, ch);
	break;

    case SDP_ServSrchRes:
	printf("Got SDP_ServSrchResp.\n");
	break;

    case SDP_ServAttrReq:
	printf("Got SDP_ServAttrReq.\n");
	SDP_serviceAttributeRsp(p, ch);
	break;

    case SDP_ServAttrRes:
	printf("Got SDP_ServAttrResp.\n");
	break;

    case SDP_ServSrchAttrReq:
	printf("Got SDP_ServSrchAttrReq.\n");
	SDP_serviceSearchAttributeRsp(p, ch);
	break;

    case SDP_ServSrchAttrRes:
	printf("Got SDP_ServSrchAttrResp.\n");
	break;

    default:
	fprintf(stderr, "%d SDP recv invalid PDUID %d\n",
		lmp_->bb_->bd_addr_, sh->pduid_);
	abort();
    }

    Packet::free(p);
}

void SDP::SDP_serviceSearchReq(uchar * req, int len)
{
    Packet *p = gen_sdp_pkt(req, len);
    hdr_sdp *sh = HDR_SDP(p);
    sh->pduid_ = SDP_ServSrchReq;
    sh->transid_ = transid_++;
    inqAndSend(p);
}

void SDP::SDP_serviceSearchRsp(Packet * p, L2CAPChannel * ch)
{
    hdr_sdp *sh = HDR_SDP(p);

    uchar rep[128];
    int len = 1;

    Packet *resp = gen_sdp_pkt(rep, len);
    hdr_sdp *sh_resp = HDR_SDP(resp);
    sh_resp->pduid_ = SDP_ServSrchRes;
    sh_resp->transid_ = sh->transid_;

    ch->enque(resp);
}

void SDP::SDP_serviceAttributeReq(uchar * req, int len)
{
    Packet *p = gen_sdp_pkt(req, len);
    hdr_sdp *sh = HDR_SDP(p);
    sh->pduid_ = SDP_ServAttrReq;
    sh->transid_ = transid_++;
    inqAndSend(p);
}

void SDP::SDP_serviceAttributeRsp(Packet * p, L2CAPChannel * ch)
{
    hdr_sdp *sh = HDR_SDP(p);

    uchar rep[128];
    int len = 1;

    Packet *resp = gen_sdp_pkt(rep, len);
    hdr_sdp *sh_resp = HDR_SDP(resp);
    sh_resp->pduid_ = SDP_ServAttrRes;
    sh_resp->transid_ = sh->transid_;

    ch->enque(resp);
}

void SDP::SDP_serviceSearchAttributeReq(uchar * req, int len)
{
    Packet *p = gen_sdp_pkt(req, len);
    hdr_sdp *sh = HDR_SDP(p);
    sh->pduid_ = SDP_ServSrchAttrReq;
    sh->transid_ = transid_++;
    inqAndSend(p);
}

void SDP::SDP_serviceSearchAttributeRsp(Packet * p, L2CAPChannel * ch)
{
    hdr_sdp *sh = HDR_SDP(p);

    uchar rep[128];
    int len = 1;

    Packet *resp = gen_sdp_pkt(rep, len);
    hdr_sdp *sh_resp = HDR_SDP(resp);
    sh_resp->pduid_ = SDP_ServSrchAttrRes;
    sh_resp->transid_ = sh->transid_;

    ch->enque(resp);
}

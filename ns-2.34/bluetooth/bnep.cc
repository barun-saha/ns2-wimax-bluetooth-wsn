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
 *	bnep.cc
 */

/*
 * Note --
 *  The bridge function of GN/NAP is supposed to implement here.
 *  We rather use the L3 approach to do it. That is, GN/NAP and BR are routers
 *  instead of bridges.  This design simplifies the simulator implementation
 *  significantly, and we don't need to handle ARP for BT devices.
 *  Bridge function presented here is quite minimum.
 */

/*
 * PAN stuff is still messy.
 *
 */

#include "l2cap.h"
#include "bnep.h"
#include "packet.h"
#include "mac.h"
#include "gridkeeper.h"
#include "lmp-piconet.h"
#include "scat-form.h"

#define BUFFSIZE 1024

int hdr_bnep::offset_;

static class BNEPHeaderClass:public PacketHeaderClass {
  public:
    BNEPHeaderClass():PacketHeaderClass("PacketHeader/BNEP",
					sizeof(hdr_bnep)) {
	bind_offset(&hdr_bnep::offset_);
    }
} class_bnephdr;

static class BNEPClass:public TclClass {
  public:
    BNEPClass():TclClass("Mac/BNEP") {}
    TclObject *create(int, const char *const *) {
	return new BNEP();
    }
} class_bnep;


//////////////////////////////////////////////////////////
//                   BridgeTable                        //
//////////////////////////////////////////////////////////
int BridgeTable::lookup(int ad)
{
    BrTableEntry *wk = _table;
    while (wk) {
	if (wk->addr == ad) {
	    return wk->port;
	}
	wk = wk->next;
    }

    return -1;
}

void BridgeTable::dump()
{
    int cntr = 0;
    printf("\nRridge Table:\n");

    BrTableEntry *wk = _table;
    while (wk) {
	printf("%d:%d %f\n", wk->addr, wk->port, wk->ts);
	cntr++;
	wk = wk->next;
    }

    printf("Total %d entries.\n\n", cntr);
}

void BridgeTable::add(int ad, int p)
{
    BrTableEntry *wk = _table;
    while (wk) {
	if (wk->addr == ad) {
	    wk->port = p;
	    wk->ts = Scheduler::instance().clock();
	    return;
	}
	wk = wk->next;
    }

    wk = new BrTableEntry(ad, p, (Scheduler::instance().clock()));
    wk->next = _table;
    _table = wk;
}

void BridgeTable::remove(int addr)
{
    if (!_table) {
	return;
    }

    BrTableEntry *wk = _table;
    if (_table->addr == addr) {
	_table = _table->next;
	delete wk;
	return;
    }

    BrTableEntry *par = _table;
    wk = _table->next;
    while (wk) {
	if (wk->addr == addr) {
	    par->next = wk->next;
	    delete wk;
	    return;
	}
	par = wk;
	wk = wk->next;
    }
}

// purge any entry old than t
void BridgeTable::remove(double t)
{
    if (!_table) {
	return;
    }

    BrTableEntry *wk = _table;
    if (_table->ts <= t) {
	_table = _table->next;
	delete wk;
	return remove(t);	// not likely to happen since header is newer.
    }

    BrTableEntry *par = _table;
    wk = _table->next;
    while (wk) {
	if (wk->ts <= t) {
	    par->next = wk->next;
	    delete wk;
	    wk = par->next;
	} else {
	    par = wk;
	    wk = wk->next;
	}
    }
}

//////////////////////////////////////////////////////////
//                      BNEP                            //
//////////////////////////////////////////////////////////
int BNEP::trace_all_bnep_ = 1;

BNEP::BNEP()
:  _timer(this), sendTimer(this), inqCallback(this)
{
    bind("onDemand_", &onDemand_);

    rolemask_ = ROLEMASK;
    role_ = 0;
    numRole_ = 0;
    _chan = 0;

    nb_ = 0;
    nb_num = 0;
    waitForInq_ = 0;
    numConnReq_ = 0;
    schedsend = 0;

    onDemand_ = 0;
    _in_make_pico = 0;

    num_conn = 0;
    num_conn_max = 8;
    _conn = new Connection *[num_conn_max];
    int i;
    for (i = 0; i < num_conn_max; i++) {
	_conn[i] = 0;
    }

    _current = 0;
    trace_me_bnep_ = 0;
}

void BNEP::setup(bd_addr_t ad, LMP * l, L2CAP * l2, SDP * s, BTNode * node)
{
    bd_addr_ = ad;
    lmp_ = l;
    l2cap_ = l2;
    sdp_ = s;
    // l2cap_->bnep_ = this;
    node_ = node;
}

void BNEPSendTimer::handle(Event *)
{
    _bnep->handle_send();
}

void BNEPTimer::handle(Event *)
{
    _bnep->piconet_sched();
}

void BNEPInqCallback::handle(Event *)
{
    _bnep->inq_complete();
}

void BNEP::addSchedEntry(Piconet * pico, double len)
{
    if (!_current) {
	_current = new BNEPSchedEntry(pico, len);
	_numSchedEntry = 1;
	return;
    }

    BNEPSchedEntry *wk = _current;
    do {
	if (wk->pico == pico) {	// do an update for existing entry.
	    wk->length = len;
	    return;
	}
    } while ((wk = wk->next) != _current);

    wk = new BNEPSchedEntry(pico, len);

    // add to the end
    wk->next = _current;
    wk->prev = _current->prev;
    _current->prev->next = wk;
    _current->prev = wk;
    _numSchedEntry++;
}

void BNEP::removeSchedEntry(Piconet * pico)
{
    if (!_current) {
	return;
    }
    BNEPSchedEntry *wk = _current;
    if (_current->pico == pico) {
	if (_current == _current->next) {	// singleton
	    _current = NULL;
	} else {
	    wk->next->prev = wk->prev;
	    wk->prev->next = wk->next;
	    _current = _current->next;
	}
	delete wk;
	_numSchedEntry--;
	return;
    }
    do {
	if (wk->pico == pico) {
	    wk->next->prev = wk->prev;
	    wk->prev->next = wk->next;
	    delete wk;
	    _numSchedEntry--;
	    return;
	}
    } while ((wk = wk->next) != _current);
}

void BNEP::disableScan()
{
    if (!_current) {
	return;
    }
    BNEPSchedEntry *wk = _current;
    do {
	if (!wk->pico) {
	    if (wk->length > 0) {
		wk->length = -wk->length;
	    }
	    return;
	}
    } while ((wk = wk->next) != _current);
}

void BNEP::enableScan(double len)
{
    if (!_current) {
	addSchedEntry(NULL, len);
	return;
    }

    BNEPSchedEntry *wk = _current;
    do {
	if (!wk->pico) {
	    wk->length = len;
	    return;
	}
    } while ((wk = wk->next) != _current);

    addSchedEntry(NULL, len);
}

void BNEP::piconet_sched()
{
    int cntr = _numSchedEntry;
    Scheduler & s = Scheduler::instance();
    if (!_current) {
	return;
    }
    if (waitForInq_) {
	printf("waitForInq_\n");
	s.schedule(&_timer, &_ev, 30E-3);
	return;
    }
    _current = _current->next;

    printf("%d %f bnepSched p:%x %f\n", bd_addr_, s.clock(),
	   (unsigned int) _current->pico, _current->length);

    lmp_->wakeup(_current->pico);

    while (_current->length <= 0 && cntr-- > 0) {	// mask out disabled
	_current = _current->next;
    }

    s.schedule(&_timer, &_ev, _current->length);
}

void BNEP::inq_complete()
{
    if (nb_) {
	lmp_->destroyNeighborList(nb_);
    }
    nb_ = lmp_->getNeighborList(&nb_num);
    waitForInq_ = 0;
    make_connections();
}

void BNEP::make_connections()
{
    if (nb_num < 1) {
	inq(1, 7);
	return;
    }

    int n = 1;
    if (canBeMaster()) {
	n = MIN(nb_num, 7);
    }

    Bd_info *wk = nb_;
    for (int i = 0; i < n; i++) {
	connect(wk->bd_addr_);
	wk = wk->next_;
    }
    numConnReq_ = n;
    schedsend = 0;
}

void BNEP::inq(int to, int num)
{
    lmp_->HCI_Inquiry(lmp_->giac_, to, num);
    lmp_->addInqCallback(&inqCallback);
    waitForInq_ = 1;
}

// Note:
// Well, BCAST packets probably is the first higher layer packet arrived, since
// Routing Agent and LL module will send them first before a data packet can
// be sent. (LL does not send ARP pkt any more.)
//
// Senarios for onDemand Scatternet formation:
// 1. no port/conn. 
//      a. check L2CAP and LMP to see if any link exists. If so, add 
//         BNEP conn/port quickly. send the packet.
//      b. no Links. retrieve neighbor list. Check capability. If canBeMaster,
//         if has neighbor, page them, otherwise, inquiry and paging.
//         If canBePANU, page one of neighbor and do a role switch upon
//         connection setup.

void BNEP::bcast(Packet * p)
{
    int i;

    // if num of bnep <= N do inqiry and paging

    if (onDemand_) {		// try to format the scatternet on demand
	if (num_conn < 1) {
	    _q.enque(p);
	    // _curPkt = p;
	    make_piconet();
	    return;
	} else if (canBeMaster() && num_conn < 2) {
	    _q.enque(p);
	    // _curPkt = p;
	    make_piconet();
	    return;
	}
    }

    printf("BNEP::bcast():num_conn:%d\n", num_conn);

    for (i = 0; i < num_conn_max; i++) {
	if (!_conn[i]) {
	    continue;
	}
	_conn[i]->cid->enque(p->copy());
    }
    Packet::free(p);
}


#if 0
    // three cases: 1. bcast.

if (role_ == PANU) {
    if (_master_bd_addr) {
    } else if (_ondemand) {
	// inquiry and page
    } else {
	// drop the packet.
    }
} else if (role_ == NAP || role_ == GN) {
    // if no bridge, mac desn't match slaves. if _ondemand
    // inquiry and page
    //
    // 
    if ((slot = findPort(mh->macDA())) >= 0) {
	_conn[slot]->cid->enque(p);
    } else {			// MAC_BROADCAST
	bcast(p);
    }
} else {			// BR
    if (_master) {
    }

}
#endif

void BNEP::make_piconet()
{
    if (_in_make_pico) {
	return;
    }

    _in_make_pico = 1;

    // Check if Links exists.  If so, make BNEP conn out of them.
    //ConnectionHandle *wk = l2cap_->_connhand;
    // while (wk) {
    // }

    // grab neighbor list from LMP
    if (nb_) {
	lmp_->destroyNeighborList(nb_);
    }
    nb_ = lmp_->getNeighborList(&nb_num);

    if (isMaster()) {
    } else if (isPANU()) {
	if (isBridge()) {
	}
    }

    if (canBeMaster()) {
	if (nb_num < 2) {
	    if (lmp_->suspendCurPiconetReq()) {
		inq(1, 4);
	    } else {
		inq(1, 4);
	    }
	} else {
	    make_connections();
	}
	return;
    } else if (canBePANU()) {
	if (canBeBridge()) {
	}
	if (nb_num < 1) {
	    if (lmp_->suspendCurPiconetReq()) {
		inq(1, 3);
	    } else {
		inq(1, 3);
	    }
	} else {
	    make_connections();
	}
	return;
    }
}

BNEP::Connection * BNEP::addConnection(L2CAPChannel * ch)
{
    Connection *c = new Connection(ch);
    if (num_conn == num_conn_max) {
	num_conn_max += num_conn_max;
	Connection **nc = new Connection *[num_conn_max];
	memset(nc, 0, sizeof(Connection *) * num_conn_max);
	memcpy(nc, _conn, sizeof(Connection *) * num_conn);
	delete[]_conn;
	_conn = nc;
    }
    num_conn++;
    for (int i = 0; i < num_conn_max; i++) {
	if (_conn[i] == 0) {
	    _conn[i] = c;
	    c->port = i;
	    return c;
	}
    }
    return NULL;
}

void BNEP::removeConnection(L2CAPChannel * ch)
{
    Connection *c = lookupConnection(ch);
    if (c) {
	removeConnection(c);
    }
}

void BNEP::removeConnection(BNEP::Connection * c)
{
    _br_table.remove(c->daddr);

    num_conn--;
    _conn[c->port] = 0;
    delete c;
}

void BNEP::portLearning(int fromPort, Packet * p)
{
    hdr_ip *ip = HDR_IP(p);
    // hdr_cmn *ch = HDR_CMN(p);
    //hdr_mac *mh = HDR_MAC(p);

    //FIXME:put source ip addr as an alternative ??
    _br_table.add(ip->saddr(), fromPort);
    // basically, mac_addr == ip_addr in ns.
    // _br_table.add(mh->macSA(), fromPort);
}

int BNEP::findPortByIp(int ip)
{
    return _br_table.lookup(ip);
}

int BNEP::findPort(int macDA)
{
    // only if macDA is the other end of the link
    // otherwise use _br_table.lookup(macDA);
    Connection *conn = lookupConnection(macDA);
    if (conn) {
	return conn->port;
    } else {
	return -1;
    }
}

BNEP::Connection * BNEP::lookupConnection(bd_addr_t addr)
{
    for (int i = 0; i < num_conn_max; i++) {
	if (_conn[i] && _conn[i]->daddr == addr) {
	    return _conn[i];
	}
    }
    return NULL;
}

BNEP::Connection * BNEP::lookupConnection(L2CAPChannel * ch)
{
    for (int i = 0; i < num_conn_max; i++) {
	if (_conn[i] && _conn[i]->cid == ch) {
	    return _conn[i];
	}
    }
    return NULL;
}

L2CAPChannel *BNEP::lookupChannel(bd_addr_t addr)
{
    for (int i = 0; i < num_conn_max; i++) {
	if (_conn[i] && _conn[i]->cid->remote() == addr) {
	    return _conn[i]->cid;
	}
    }
    return NULL;
}

void BNEP::handle_send()
{
    if (!_in_make_pico) {
	return;
    }
    _in_make_pico = 0;
    _send();
}

void BNEP::_send()
{
    Packet *p;
    while ((p = _q.deque())) {
	hdr_ip *ip = HDR_IP(p);

	// hdr_cmn *ch = HDR_CMN(p);
	hdr_mac *mh = HDR_MAC(p);
	int slot, i;

	int da = ip->daddr();

	if (mh->macDA() == (int) MAC_BROADCAST) {
	    printf("BNEP::bcast():num_conn:%d\n", num_conn);

	    for (i = 0; i < num_conn; i++) {
		_conn[i]->cid->enque(p->copy());
	    }
	    Packet::free(p);
	} else if ((slot = findPort(da)) >= 0) {
	    _conn[slot]->cid->enque(p);
	}
    }
}

void BNEP::schedule_send(int slots)
{
    Scheduler::instance().schedule(&sendTimer, &send_ev,
				   BTSlotTime * slots);
}

// Master has a scheduler.  The slave is controlled by the master.
// t0: upper layer pkt arrives.
// t1: M: idle->Inq, Page,              S:idle->Scan
// t2: M: page_complete, upper layer    S: conn_ind, upper layer.
// t3: M: master piconet. decide the    S: Know when to return
//          intv, send to S:
// t4: M: send Data                     S: receive bcast DATA
//                                         put link on hold, Inq, page

void BNEP::channel_setup_complete(L2CAPChannel * ch)
{
    Connection *c = lookupConnection(ch);
    if (!c) {
	c = addConnection(ch);
    }
    _br_table.add(c->daddr, c->port);
    c->ready_ = 1;

    fprintf(stdout, "%d %s ", bd_addr_, __FUNCTION__);
    c->dump(stdout);
    fprintf(stdout, "\n");

    if (node_->scatFormator_) {
	node_->scatFormator_->connected(ch->remote());
	// return;
    }
    // Add arp stuff to LL arp table, if it exists.
    // TODO 

    // Add routing table entry.
    // add SchedEntry.
    // TODO 

    if (c->cid->connhand()->link->piconet->isMaster()) {
	becomeGN();
	_masterPort++;
	if (_masterPort == 1) {
	    enableScan(30 * 1E-3);
	    addSchedEntry(c->cid->connhand()->link->piconet, 1);
	    if (_ev.uid_ <= 0) {
		// piconet_sched();
	    }
	}

    } else {
	becomePANU();
#if 0
	enableScan(30 * 1E-3);
	addSchedEntry(c->cid->connhand()->link->piconet, 1);
	if (_ev.uid_ <= 0) {
	    piconet_sched();
	}
#endif
    }

    if (!schedsend) {
	schedule_send(12);
	schedsend = 1;
    }
    if (c->_nscmd) {
	Tcl & tcl = Tcl::instance();
	tcl.eval(c->_nscmd);
	c->_nscmd = 0;
    }

    if (numConnReq_ > 1) {
	numConnReq_--;
    } else {
	_send();
    }
}

void BNEP::disconnect(bd_addr_t addr, uchar reason)
{
    Connection *c = lookupConnection(addr);
    // assert(c && c->cid);
    if (c && c->cid) {
	c->cid->disconnect(reason);
    } else {
	ConnectionHandle *connh = l2cap_->lookupConnectionHandle(addr);
	if (connh && connh->chan) {
	    connh->chan->disconnect(reason);
	} else if (connh) {
	    lmp_->HCI_Disconnect(connh, reason);
	}
    }

    if (c) {
	removeConnection(c);
    }
}

BNEP::Connection * BNEP::connect(bd_addr_t addr, hdr_bt::packet_type pt,
				 hdr_bt::packet_type rpt, Queue * ifq)
{
    Connection *c;
    if ((c = lookupConnection(addr))) {
	return c;
    }
#if 0
    if (pt < hdr_bt::NotSpecified) {
	lmp_->defaultPktType_ = pt;
    }
    if (rpt < hdr_bt::NotSpecified) {
	lmp_->defaultRecvPktType_ = rpt;
    }
#endif

    // In reality, this call will block until the L2CAP Channel
    // is established. The simulator returns as long as Page request
    // is queued.  So, we have a flag in Connection to indicate if the
    // underlying L2CAP Channel setup is completed.
    L2CAPChannel *ch = l2cap_->L2CA_ConnectReq(addr, PSM_BNEP, ifq);

    if (pt < hdr_bt::NotSpecified) {	// try to change pktType
	ch->changePktType(pt);
    }
    if (rpt < hdr_bt::NotSpecified) {
	ch->changeRecvPktType(rpt);
    }

    c = addConnection(ch);
    if (ch->ready_) {
	c->ready_ = 1;
    }

    return c;
}

// In current implentment, bridges are routers. I.e., UCBT adopts a L3 
// approach.  When packet is passed down to BNEP.  MacDA() should be the 
// othe end of BNEP link, unless it is a broadcasting pkt.  Unlike 
// specified in PAN profile, where an external interface may exist,
// a packet to an external interface should be directed to a different MAC
// by the routing agent (hier routing).
//
// 1. If the packet is bcast, send it to each port.
// 2. Lookup outgoing port for the pkt by its MacDA(), and send to that port,
// 3. otherwise, drop it if no port is found.
void BNEP::sendDown(Packet * p, Handler * h)
{
    int slot;
    hdr_ip *ip = HDR_IP(p);
    hdr_cmn *ch = HDR_CMN(p);
    hdr_bt *bh = HDR_BT(p);
    hdr_mac *mh = HDR_MAC(p);
    hdr_bnep *bneph = HDR_BNEP(p);
    // hdr_tcp *tcp = HDR_TCP(p);
    double now = Scheduler::instance().clock();

    bh->ts_ = now;		// record time stamp, used by BTFCFS

    node_->recordSend(ch->size(), ip->daddr(), ip->dport(), &bh->hops_,
		      &bh->flow_ts_, &bh->flow_seq_, &bh->flow_ts_lasthop_,
		      &bh->flow_seq_lasthop_, ip->saddr() == bd_addr_);

    bneph->u.ether.prot_type = mh->hdr_type();
    bneph->ext_bit = 0;

    if (trace_all_bnep_ || trace_me_bnep_) {
	fprintf(BtStat::log_, BNEPPREFIX0
		"%d %d:%d->%d:%d %f %d %d %d %d %d\n",
		bd_addr_, ip->saddr(), ip->sport(), ip->daddr(),
		ip->dport(), now,
		ch->next_hop(), bh->hops_, bh->flow_seq_, ch->size(),
		bh->flow_seq_lasthop_);
    }
#if 0
    // UCBT doesn't model ARP since bluetooth link are P to P and the
    // both ends of a link always know the MAC of each other.
    if (mh->hdr_type() == ETHERTYPE_ARP) {	// Arp packet, handle to proxy.
	handle_arp(p);
	return;
    }
#endif

    // int da = ip->daddr();
    // Add BNEP header.
    if (mh->macDA() == (int) MAC_BROADCAST) {
	bneph->type = BNEP_COMPRESSED_ETHERNET_DEST_ONLY;
	bneph->u.ether.daddr = MAC_BROADCAST;
	ch->size() += bneph->hdr_len();
	bcast(p);
	// } else if ((slot = findPort(mh->macDA())) >= 0) {
	// } else if ((slot = findPort(da)) >= 0) {
	// } else if ((slot = findPortByIp(ip->daddr())) >= 0) {
    } else if ((slot = findPortByIp(ch->next_hop())) >= 0) {
	bneph->type = BNEP_COMPRESSED_ETHERNET;
	ch->size() += bneph->hdr_len();
	_conn[slot]->cid->enque(p);
    } else {
	// A possible way to handle it is to bcast the pkt.  However,
	// choose to drop it at this moment.
	// bcast(p);
	if (node_->getRagent()) {
	    node_->getRagent()->linkFailed(p);
	}
	// drop(p, "NoPort");
    }
}

// receive packet from L2CAP CID.  
void BNEP::sendUp(Packet * p, Handler * h)
{
    hdr_ip *ip = HDR_IP(p);
    hdr_cmn *ch = HDR_CMN(p);
    hdr_bt *bh = HDR_BT(p);
    hdr_bnep *bneph = HDR_BNEP(p);
    // hdr_tcp *tcp = HDR_TCP(p);

    if (bneph->u.ether.prot_type == ETHER_PROT_SCAT_FORM) {
	node_->scatFormator_->recv(p, HDR_BT(p)->sender);
	return;
    }

    double now = Scheduler::instance().clock();
    node_->recordRecv(ch->size(), ip->daddr(), ip->dport(),
		      bh->hops_, bh->flow_ts_, bh->flow_seq_,
		      bh->flow_ts_lasthop_, bh->flow_seq_lasthop_);

    ch->size() -= bneph->hdr_len();

#if 0
    // set mac frame paramter, we don't need to do so in the simulator.
    mh->hdr_type() = bneph->u.ether.prot_type;
    if (bneph->type == BNEP_COMPRESSED_ETHERNET) {
    } else if (bneph->type == BNEP_COMPRESSED_ETHERNET_DEST_ONLY) {
    } else if (bneph->type == BNEP_COMPRESSED_ETHERNET_SOURCE_ONLY) {
    } else if (bneph->type == BNEP_GENERAL_ETHERNET) {
    }
#endif

    if (trace_all_bnep_ || trace_me_bnep_) {
	fprintf(BtStat::log_, BNEPPREFIX1
		"%d %d:%d->%d:%d %f %f %d %d %d %f %d\n",
		bd_addr_, ip->saddr(),
		ip->sport(), ip->daddr(), ip->dport(),
		now, (now - bh->flow_ts_), bh->hops_, bh->flow_seq_,
		ch->size(), (now - bh->flow_ts_lasthop_),
		bh->flow_seq_lasthop_);
/*
	fprintf(BtStat::log_,
		"ts:%f ts_e:%f seq:%d reason:%d rtt:%d delay:%f\n",
		tcp->ts(), tcp->ts_echo(), tcp->seqno(), tcp->reason(),
		tcp->last_rtt(), now - tcp->ts());
*/
    }

    int fromPort = findPort(HDR_BT(p)->sender);
    portLearning(fromPort, p);

    uptarget_->recv(p);
    return;

#if 0
    // Bridge function
    int slot;
    if (mh->macDA() == bd_addr_) {
	uptarget_->recv(p);
	return;
    } else if ((slot = findPort(mh->macDA())) >= 0) {
	_conn[slot]->cid->enque(p);
    } else {			// MAC_BROADCAST
	uptarget_->recv(p);
	bcast(p);
    }
#endif
}

void BNEP::dump_energy(FILE * out)
{
}

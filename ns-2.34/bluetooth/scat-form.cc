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

#include "scat-form.h"
#include "lmp-piconet.h"

void ScatFormInqCallback::handle(Event *)
{
    formator_->inq_complete();
}

void ScatFormPageCallback::handle(Event *)
{
    formator_->page_complete();
}

void ScatFormTimer::handle(Event * e)
{
    formator_->fire(e);
}

int ScatFormator::seqno_ = 10;
int ScatFormator::sendMsg(uchar id, uchar * content, int len, int dst,
			  int nexthop, int datasize)
{
    int slot;
    Packet *p = Packet::alloc();
    hdr_cmn *ch = HDR_CMN(p);
    ch->ptype() = PT_BNEP;
    hdr_bt *bh = HDR_BT(p);
    hdr_bnep *bneph = HDR_BNEP(p);
    SFmsg *msg = &bh->u.sf;
    // double now = Scheduler::instance().clock();

    bh->extinfo = seqno_++;
    bneph->u.ether.prot_type = ETHER_PROT_SCAT_FORM;
    bneph->ext_bit = 0;

    // XXX: Remove this limitation
    if (len > 92) {
	fprintf(stderr, "%s: message (%d) is too large.\n", __FUNCTION__,
		len);
	abort();
    }

    memcpy(msg->data, content, len);
    msg->code = id;
    msg->type = type();
    msg->target = dst;
    if (datasize > 0) {
	ch->size() = datasize + 8;
    } else {
	ch->size() = len + 8;
    }

    bh->dump(stdout, '=', id_, "SF");
    if (bh->extinfo >= 10) {
	bh->dump_sf();
    }
    if ((slot = bnep_->findPortByIp(nexthop)) >= 0) {
	bneph->type = BNEP_COMPRESSED_ETHERNET;
	ch->size() += bneph->hdr_len();
	bnep_->_conn[slot]->cid->enque(p);
    } else {
	fprintf(stdout,
		"%s: message from %d to %d via %d can not be sent.",
		__FUNCTION__, id_, dst, nexthop);
	fprintf(stderr,
		"%s: message from %d to %d via %d can not be sent.",
		__FUNCTION__, id_, dst, nexthop);
	// abort();
	Packet::free(p);
	return 0;
    }
    return 1;
}


// Factor: number of neibouring piconet
//    Slave: 1
//    Master: number of M/S bridg + number of 2 hop master connected
//            by S/S bridge
//    S/S bridge: number of Roles
//    M/S bridge: number of Roles + Factor for the master role - 1

void ScatFormator::dumpTopo()
{
    BTNode *wk = node_;
    do {
	wk->lmp_->dump();
	wk->lmp_->dump(stderr);
    } while ((wk = wk->getNext()) != node_);

    checkConnectivity(GeometryConn);
    checkConnectivity(ActualConn);
}

void ScatFormator::clearConnMark()
{
    BTNode *wk = node_;
    do {
	wk->bb_->clearConnMark();
    } while ((wk = wk->getNext()) != node_);
}

// Type: GeometryConn/RandomConn/ActualConn
//   GeometryConn:  -- basically a distance graph, ie. a link presents if
//                 2 nodes are within Baseband::radioRange_.
//   RandomConn:    -- a node randomly becomes a master or bridge.  There is
//                 no link setup cost.
//                 Parameters: p: ratio to become a master
//                             s: ratio to become a S/S bridge
//                             r: max number of roles 
//                             b: max number of bridges per piconet
//   ActualConn:    -- The resulting scatternet by some scatternet formation 
//                     algo.    
void ScatFormator::checkConnectivity(ConnType type)
{
    clearConnMark();
    BTNode *wk = node_;
    Queue q;
    int componId = -1;
    Component *compon = NULL;

    do {
	if (wk->bb_->getComponentId() < 0) {
	    componId++;
	    compon = new Component(compon, componId);
	    q.enque(wk);
	    wk->bb_->setComponentId(componId);
	    BTNode *nd;
	    while ((nd = q.deque())) {
		compon->addNode(nd);
		switch (type) {
		case ActualConn:
		    expandNode(nd, &q, componId);
		    break;
		case GeometryConn:
		    expandGeometryNode(nd, &q, componId);
		    break;
		case RandomConn:
		    break;
		}
	    }
	}
    } while ((wk = wk->getNext()) != node_);

    char *tname;
    switch (type) {
    case ActualConn:
	clearComponent(&component_);
	component_ = compon;
	tname = "Actual Scatternet";
	break;
    case GeometryConn:
	clearComponent(&geoCompon_);
	geoCompon_ = compon;
	tname = "Geometry";
	break;
    case RandomConn:
	clearComponent(&randCompon_);
	randCompon_ = compon;
	tname = "Random";
	break;
    }
    Component *maxC = compon;
    Component *comp = compon;
    int ttlNode = 0;

    fprintf(stderr, "\n%s Components: ", tname);
    while (comp) {
	if (comp->numNode > maxC->numNode) {
	    maxC = comp;
	}
	ttlNode += comp->numNode;
	fprintf(stderr, " %d:", comp->numNode);
	for (int i = 0; i < comp->numNode; i++) {
	    fprintf(stderr, "%d-", comp->ad[i]->bb_->bd_addr_);
	}
	comp = comp->next;
    }

    fprintf(stderr, " ==> connectivity: %d/%d = %f\n", maxC->numNode,
	    ttlNode, double (maxC->numNode) / ttlNode);

    if (type == ActualConn) {
	dumpDegree(stderr);
	dumpDegree(stdout);
    }
}

void ScatFormator::dumpDegree(FILE * out)
{
    int role;
    int deg;
    int degSlave = 0;
    int numSlave = 0;
    int degMaster = 0;
    int numMaster = 0;
    int degSSbr = 0;
    int numSSbr = 0;
    int degMSbr = 0;
    int numMSbr = 0;
    int numNotConn = 0;
    char *r = "NN";
    int maNumBr = 0;
    int maNumSlave = 0;
    int ttlmaNumBr = 0;
    int ttlmaNumSlave = 0;
    int ttlmsNumBr = 0;
    int ttlmsNumSlave = 0;

    int geometryDeg;
    int ttlgeometryDeg = 0;
    int numNode = 0;

    if (!out) {
	out = stderr;
    }

    BTNode *wk = node_;
    do {
	deg = wk->lmp_->computeDegree(&role, &maNumBr, &maNumSlave);
	switch (role) {
	case 1:
	    degSlave += deg;
	    numSlave++;
	    r = "SL";
	    break;
	case 2:
	    degMaster += deg;
	    numMaster++;
	    ttlmaNumBr += maNumBr;
	    ttlmaNumSlave += maNumSlave;
	    r = "MA";
	    break;
	case 3:
	    degSSbr += deg;
	    numSSbr++;
	    r = "SS";
	    break;
	case 4:
	    degMSbr += deg;
	    numMSbr++;
	    ttlmsNumBr += maNumBr;
	    ttlmsNumSlave += maNumSlave;
	    r = "MS";
	    break;

	default:
	    numNotConn++;
	    r = "NN";
	    break;
	}

	wk->lmp_->dump(out);
	geometryDeg = wk->lmp_->computeGeometryDegree();
	ttlgeometryDeg += geometryDeg;
	numNode++;
	if (role == 2 || role == 4) {
	/** Commented by Barun [07 March 2013]
	    fprintf(out, "  %d %d (%d %d) %d %s\n", wk->bb_->bd_addr_, deg,
		    maNumBr, maNumSlave, geometryDeg, r);
	*/
	} else {
	/** Commented by Barun [07 March 2013]
	    fprintf(out, "  %d %d %d %s\n", wk->bb_->bd_addr_, deg, geometryDeg,
		    r);
	*/
	}
	/** Commented by Barun [07 March 2013]
	fprintf(out, "   -- ");
	*/
	wk->scatFormator_->dump(out);
    } while ((wk = wk->getNext()) != node_);

    double aveDegSlave = (numSlave > 0 ? double (degSlave) / numSlave : 0);
    double aveDegMaster =
	(numMaster > 0 ? double (degMaster) / numMaster : 0);
    double aveMaNumBr =
	(numMaster > 0 ? double (ttlmaNumBr) / numMaster : 0);
    double aveMaNumSlave =
	(numMaster > 0 ? double (ttlmaNumSlave) / numMaster : 0);
    double aveDegSSbr = (numSSbr > 0 ? double (degSSbr) / numSSbr : 0);
    double aveDegMSbr = (numMSbr > 0 ? double (degMSbr) / numMSbr : 0);
    double aveMSNumBr =
	(numMaster > 0 ? double (ttlmsNumBr) / numMaster : 0);
    double aveMSNumSlave =
	(numMaster > 0 ? double (ttlmsNumSlave) / numMaster : 0);
    double aveDegBr =
	(numMSbr + numSSbr >
	 0 ? double (degMSbr + degSSbr) / (numMSbr + numSSbr) : 0);
    fprintf(out, "\n  -slave %d %f\n", numSlave, aveDegSlave);
    fprintf(out, "  -master %d %f %f %f\n", numMaster, aveDegMaster,
	    aveMaNumBr, aveMaNumSlave);
    fprintf(out, "  -S/S br %d %f\n", numSSbr, aveDegSSbr);
    fprintf(out, "  -M/S br %d %f %f %f\n", numMSbr, aveDegMSbr,
	    aveMSNumBr, aveMSNumSlave);
    fprintf(out, "  -br %d %f\n", numMSbr + numSSbr, aveDegBr);
    fprintf(out, "  -not connected %d\n", numNotConn);
    fprintf(out, "  Geometry %d %f\n", numNode,
	    double (ttlgeometryDeg) / numNode);
}

void ScatFormator::expandGeometryNode(BTNode * nd, Queue * q, int componId)
{
    BTNode *wk = node_;
    do {
	if (wk->bb_->getComponentId() < 0
	    && wk->distance(nd) < nd->radioRange_) {
	    wk->bb_->setComponentId(componId);
	    q->enque(wk);
	}
    } while ((wk = wk->getNext()) != node_);
}

void ScatFormator::expandNode(BTNode * nd, Queue * q, int componId)
{
    if (nd->lmp_->numPico() < 0) {
	return;
    }
    int i = 0;
    Piconet *pico = nd->lmp_->curPico;
    if (pico) {
	expandPiconet(pico, q, componId);
	i++;
    }
    pico = nd->lmp_->suspendPico;
    for (; i < nd->lmp_->numPico(); i++) {
	if (!pico) {
	    fprintf(stdout, "Null pico: ");
	    nd->lmp_->dump(stdout, 1);
	    break;
	}
	expandPiconet(pico, q, componId);
	pico = pico->next;
    }
}

void ScatFormator::expandPiconet(Piconet * pico, Queue * q, int componId)
{
    int i;
    LMPLink *link = pico->activeLink;
    for (i = 0; i < pico->numActiveLink; i++) {
	enqueueLink(link, q, componId);
	link = link->next;
    }

    link = pico->suspendLink;
    for (i = 0; i < pico->numSuspendLink; i++) {
	enqueueLink(link, q, componId);
	link = link->next;
    }
}

void ScatFormator::enqueueLink(LMPLink * link, Queue * q, int componId)
{
    BTNode *rmt = node_->lookupNode(link->remote->bd_addr_);
    if (rmt->bb_->getComponentId() < 0) {
	rmt->bb_->setComponentId(componId);
	q->enque(rmt);
    }
}

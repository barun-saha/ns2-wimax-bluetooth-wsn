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
 *	aodv-bt.cc
 */

#include "manual-bt.h"
#include "random.h"
#include <lmp.h>
#include <bt-node.h>


static class Manual_BTclass:public TclClass {
  public:
    Manual_BTclass():TclClass("Agent/ManualBT") { } 
    TclObject *create(int argc, const char *const *argv) {
	return new Manual_BT();
    }
} class_rtProtoManualBT;

int Manual_BT::command(int argc, const char *const *argv)
{
    if (argc == 3) {
	if (strcmp(argv[1], "node") == 0) {
	    RoutingIF::node_ = (BTNode *) TclObject::lookup(argv[2]);
	    if (RoutingIF::node_ == 0) {
		return TCL_ERROR;
	    }
	    RoutingIF::node_->setRagent(this);
	    return TCL_OK;
	} else if (strcmp(argv[1], "port-dmux") == 0) {
	    dmux_ = (PortClassifier *) TclObject::lookup(argv[2]);
	    if (dmux_ == 0) {
		fprintf(stderr, "%s: %s lookup of %s failed\n", __FILE__,
			argv[1], argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;
	} else if (strcmp(argv[1], "tracetarget") == 0) {
	    tracetarget_ = (Trace *) TclObject::lookup(argv[2]);
	    if (tracetarget_ == 0) {
		fprintf(stderr, "%s: %s lookup of %s failed\n", __FILE__,
			argv[1], argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;
	}
    }
    return Agent::command(argc, argv);
}

void Manual_BT::recv(Packet * p, Handler * h = 0)
{
    hdr_cmn *ch = HDR_CMN(p);
    hdr_ip *ih = HDR_IP(p);

    CrtEntry *rtent;

    if (ih->daddr() == (nsaddr_t) IP_BROADCAST) {
	ch->addr_type() = NS_AF_NONE;
    } else {
	if ((rtent = rtable.nextHopEnt(ih->daddr())) == NULL) {
	    fprintf(stderr, "Manual_BT::recv(): No route.\n");
	    abort();
	}
	ch->next_hop_ = rtent->nexthop;
	ch->addr_type() = NS_AF_INET;

	if (rtent->flag != CRTENT_UP
	    && ((BTNode *) node_)->lmp_->rpScheduler
	    && ((BTNode *) node_)->lmp_->rpScheduler->type() 
		!= RPSched::LPDRP 
	    && ((BTNode *) node_)->lmp_->rpScheduler->type() 
		!= RPSched::MDRP) {
	    if (rtent->flag == CRTENT_UNINI) {
		rtent->flag = CRTENT_ININI;
		prapare_for_traffic(ih->daddr());
	    }
	    ch->direction() = hdr_cmn::DOWN;
	    enque(p);
	    return;
	}
    }
    ch->direction() = hdr_cmn::DOWN;

    target_->recv(p, (Handler *) 0);
}

void Manual_BT::prapare_for_traffic(nsaddr_t dst)
{
    if (((BTNode *) node_)->lmp_->rpScheduler) {
	((BTNode *) node_)->lmp_->rpScheduler->rpAdjustStart(dst);
	return;
    }
}

nsaddr_t Manual_BT::nextHop(nsaddr_t dst)
{
    return rtable.nextHop(dst);
}

Packet *Manual_BT::deque(nsaddr_t dst)
{
    Packet *p;

    pq.resetIterator();
    while ((p = pq.getNext())) {
	if (HDR_IP(p)->daddr() == dst) {
	    pq.remove(p);
	    return p;
	}
    }
    return NULL;
}

void Manual_BT::sendInBuffer(nsaddr_t dst)
{
    CrtEntry *rtent = rtable.nextHopEnt(dst);
    rtent->flag = CRTENT_UP;

    Packet *p;
    while ((p = deque(dst))) {
	target_->recv(p, (Handler *) 0);
    }
}

void Manual_BT::addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag)
{
    rtable.add(dst, nexthop);
}

void Manual_BT::delRtEntry(nsaddr_t nexthop)
{
    rtable.removeNextHop(nexthop);
}

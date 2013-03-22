/*
 * Copyright (c) 1997 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Ported from CMU/Monarch's code, appropriate copyright applies.
 * nov'98 -Padma.
 *
 * $Header: /home/cvs/CVS/bt/trace-bt.cc,v 1.13 2006/01/14 23:44:31 qw Exp $
 */

/*
 * Adapted from ../trace/cmu-trace.cc.
 * -qw
 */

#include "trace-bt.h"

static class BTTraceClass:public TclClass {
  public:
    BTTraceClass():TclClass("CMUTrace/BT") {
    } TclObject *create(int, const char *const *argv) {
	return (new BTTrace(argv[4], *argv[5]));
    }
}

bttrace_class;

int BTTrace::command(int argc, const char *const *argv)
{
    if (argc == 3) {
	if (strcmp(argv[1], "node") == 0) {
	    node_ = (BTNode *) TclObject::lookup(argv[2]);
	    if (node_ == 0) {
		return TCL_ERROR;
	    }
	    // CMUTrace::node_ = (MobileNode *) 1;      // mark it initialized.
	    return TCL_OK;
	}
    }
    return CMUTrace::command(argc, argv);
}

void BTTrace::recv(Packet * p, Handler * h)
{
    if (!node_energy()) {
	Packet::free(p);
	return;
    }
    assert(initialized());
    format(p, "---");
    pt_->dump();
    //namdump();
    if (target_ == 0) {
	Packet::free(p);
    } else {
	send(p, h);
    }
}

void BTTrace::recv(Packet * p, const char *why)
{
    assert(initialized() && type_ == DROP);
    if (!node_energy()) {
	Packet::free(p);
	return;
    }
    format(p, why);
    pt_->dump();
    //namdump();
    Packet::free(p);
}


void BTTrace::nam_format(Packet * p, int offset)
{
    Node *srcnode = 0;
    Node *dstnode = 0;
    Node *nextnode = 0;
    struct hdr_cmn *ch = HDR_CMN(p);
    struct hdr_ip *ih = HDR_IP(p);
    char op = (char) type_;
    char colors[32];
    int next_hop = -1;

// change wrt Mike's code
    assert(type_ != EOT);



    int dst = Address::instance().get_nodeaddr(ih->daddr());

    nextnode = Node::get_node_by_address(ch->next_hop_);
    if (nextnode)
	next_hop = nextnode->nodeid();

    srcnode = Node::get_node_by_address(src_);
    dstnode = Node::get_node_by_address(ch->next_hop_);

    double distance = 0;

    if ((srcnode) && (dstnode)) {
	/*
	BTNode *tmnode = (BTNode *) srcnode;
	BTNode *rmnode = (BTNode *) dstnode;
	*/

	// distance = tmnode->propdelay(rmnode) * 300000000 ;
	distance = 1;
    }

    double energy = -1;
    double initenergy = -1;

    //default value for changing node color with respect to energy depletion
    double l1 = 0.5;
    double l2 = 0.2;

#if 1
    if (srcnode) {
	if (srcnode->energy_model()) {
	    energy = srcnode->energy_model()->energy();
	    initenergy = srcnode->energy_model()->initialenergy();
	    l1 = srcnode->energy_model()->level1();
	    l2 = srcnode->energy_model()->level2();
	}
    }
#endif

    int energyLevel = 0;
    double energyLeft = (double) (energy / initenergy);

    if ((energyLeft <= 1) && (energyLeft >= l1))
	energyLevel = 3;
    if ((energyLeft >= l2) && (energyLeft < l1))
	energyLevel = 2;
    if ((energyLeft > 0) && (energyLeft < l2))
	energyLevel = 1;

    if (energyLevel == 0)
	strcpy(colors, "-c black -o red");
    else if (energyLevel == 1)
	strcpy(colors, "-c red -o yellow");
    else if (energyLevel == 2)
	strcpy(colors, "-c yellow -o green");
    else if (energyLevel == 3)
	strcpy(colors, "-c green -o black");

    // A simple hack for scadds demo (fernandez's visit) -- Chalermek
    int pkt_color = 0;
    if (ch->ptype() == PT_DIFF) {
	hdr_cdiff *dfh = HDR_CDIFF(p);
	if (dfh->mess_type != DATA) {
	    pkt_color = 1;
	}
    }
    // convert to nam format 
    if (op == 's')
	op = 'h';
    if (op == 'D')
	op = 'd';
    if (op == 'h') {
	sprintf(pt_->nbuffer(), "+ -t %.9f -s %d -d %d -p %s -e %d -c 2 -a %d -i %d -k %3s ", Scheduler::instance().clock(), src_,	// this node
		next_hop,
		packet_info.name(ch->ptype()),
		ch->size(), pkt_color, ch->uid(), tracename);

	offset = strlen(pt_->nbuffer());
	pt_->namdump();
	sprintf(pt_->nbuffer(), "- -t %.9f -s %d -d %d -p %s -e %d -c 2 -a %d -i %d -k %3s", Scheduler::instance().clock(), src_,	// this node
		next_hop,
		packet_info.name(ch->ptype()),
		ch->size(), pkt_color, ch->uid(), tracename);

	offset = strlen(pt_->nbuffer());
	pt_->namdump();
    }
    // if nodes are too far from each other
    // nam won't dump SEND event 'cuz it's
    // gonna be dropped later anyway
    // this value 250 is pre-calculated by using 
    // two-ray ground refelction model with fixed
    // transmission power 3.652e-10
//      if ((type_ == SEND)  && (distance > 250 )) return ;

    if (tracetype == TR_ROUTER && type_ == RECV && dst != -1)
	return;
    if (type_ == RECV && dst == -1)
	dst = src_;		//broadcasting event

    if (energy != -1) {		//energy model being turned on
	if (src_ >= MAX_NODE) {
	    fprintf(stderr, "%s: node id must be < %d\n",
		    __PRETTY_FUNCTION__, MAX_NODE);
	    exit(0);
	}
	if (nodeColor[src_] != energyLevel) {	//only dump it when node  
	    sprintf(pt_->nbuffer(),	//color change
		    "n -t %.9f -s %d -S COLOR %s", Scheduler::instance().clock(), src_,	// this node
		    colors);
	    offset = strlen(pt_->nbuffer());
	    pt_->namdump();
	    nodeColor[src_] = energyLevel;
	}
    }

    sprintf(pt_->nbuffer(), "%c -t %.9f -s %d -d %d -p %s -e %d -c 2 -a %d -i %d -k %3s", op, Scheduler::instance().clock(), src_,	// this node
	    next_hop,
	    packet_info.name(ch->ptype()),
	    ch->size(), pkt_color, ch->uid(), tracename);

    // hdr_bt *bh = HDR_BT(p);
    if (op == 'h') {
	double r = 0.5;
	// double dur = bh->size * 1E-6;
	sprintf(pt_->nbuffer() + strlen(pt_->nbuffer()),
                        " -R %.1f", r);
                        // " -R %.1f -D %.6f", r, dur);
    }

    offset = strlen(pt_->nbuffer());
    pt_->namdump();
}

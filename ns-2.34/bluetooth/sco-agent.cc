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
 *	sco-agent.cc
 */

#include "sco-agent.h"
#include "baseband.h"

static class ScoAgentclass:public TclClass {
  public:
    ScoAgentclass():TclClass("Agent/SCO") {} 

    TclObject *create(int, const char *const *) {
	return (new ScoAgent());
    }
} class_sco_agent;

void ScoAgentTimer::handle(Event * e)
{
    _agent->start_app();
}

int ScoAgent::trace_all_scoagent_ = 1;

ScoAgent::ScoAgent()
:  Agent(PT_BT), _timer(this)
{
    bind("packetType_", &packetType_);
    bind("initDelay_", &initDelay_);

    packetType_ = hdr_bt::HV3;
    initDelay_ = 0;		// delay app_start() after link complete.

    trace_me_scoagent_ = 0;
}

void ScoAgent::recv(Packet * p)
{
    if (trace_all_scoagent_ || trace_me_scoagent_) {
	fprintf(BtStat::log_, SCOAGENTPREFIX1
		"%d->%d %f size:%d\n",
		lmp_->bb_->bd_addr_, daddr,
		(Scheduler::instance().clock()), HDR_CMN(p)->size());
    }
}

void ScoAgent::sendmsg(int nbytes, const char *flag)
{
    sendmsg(nbytes);
}

void ScoAgent::sendmsg(int nbytes)
{
    Packet *p = Packet::alloc();
    hdr_cmn *ch = HDR_CMN(p);
    ch->ptype() = PT_SCO;
    hdr_bt *bh = HDR_BT(p);
    // hdr_ip *ip = HDR_IP(p);

    if (trace_all_scoagent_ || trace_me_scoagent_) {
	fprintf(BtStat::log_, SCOAGENTPREFIX0
		"%d->%d %f size:%d\n",
		lmp_->bb_->bd_addr_, daddr,
		(Scheduler::instance().clock()), nbytes);
    }

    if (nbytes > 30) {
	fprintf(stderr, "ScoAgent::sendmsg(): payload(%d > 30) is too big.\n",
	       nbytes);
	abort();
    }
    if (!connh || !connh->link) {
	drop(p, "SCO_no_link");
	return;
    }
    bh->pid = hdr_bt::pidcntr++;
    bh->comment("S");
    bh->type = connh->link->pt;
    bh->ph.l_ch = L_CH_UD;
    bh->size = hdr_bt::packet_size(bh->type, nbytes);
    HDR_CMN(p)->size() = nbytes;
    if (connh->link->DataQlen() > 0) {
	drop(p, "SCO_no_queue");
    } else if (!connh->link->tobedetached) {
	bh->receiver = connh->link->remote->bd_addr_;
	connh->link->enqueue(p);
    } else {
	drop(p, "SCO_link_to_be_detached");
    }
}

int ScoAgent::start_app()
{
    if (app_) {
	Tcl & tcl = Tcl::instance();
        if (trace_all_scoagent_ || trace_me_scoagent_) {
	    fprintf(BtStat::log_, SCOAGENTPREFIX2
		"%d->%d %f: %s start \n",
		lmp_->bb_->bd_addr_, daddr,
		(Scheduler::instance().clock()), app_->name());

        }
	tcl.evalf("%s start ", app_->name());
	return TCL_OK;
    }
    return 1;
}

int ScoAgent::connection_complete_event(ConnectionHandle * coh)
{
    connh = coh;
    if (app_) {
	if (initDelay_ > 0) {
	    Scheduler::instance().schedule(&_timer, &_ev,
				       initDelay_ * BTSlotTime);
	} else {
	    start_app();
	}
    }
    return 1;
}

void ScoAgent::linkDetached()
{
    connh = NULL;
}


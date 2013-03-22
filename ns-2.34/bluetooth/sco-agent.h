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

#ifndef __ns_sco_agent_h__
#define __ns_sco_agent_h__

#include "bi-connector.h"
#include "agent.h"
#include "baseband.h"
#include "lmp.h"
#include "l2cap.h"

class ScoAgent;

class ScoAgentTimer:public Handler {
  public:
    ScoAgentTimer(ScoAgent * a):_agent(a) {} 
    void handle(Event *);
  private:
    ScoAgent * _agent;
};

class ScoAgent:public Agent {
  public:
    ScoAgent();
    void recv(Packet * p);
    void sendmsg(int nbytes);
    void sendmsg(int nbytes, const char *);
    // int command(int argc, const char *const *argv);
    int connection_complete_event(ConnectionHandle *);
    int start_app();
    void linkDetached();

    LMP *lmp_;
    // L2CAP *l2cap_;
    // BTNode * btnode_;

    int packetType_;
    int initDelay_;		//  Slots

    ConnectionHandle *connh;
    ScoAgent *next;
    ScoAgent *lnext;
    bd_addr_t daddr;

    ScoAgentTimer _timer;
    Event _ev;

    static int trace_all_scoagent_;
    int trace_me_scoagent_;
};

#endif				// __ns_sco_agent_h__

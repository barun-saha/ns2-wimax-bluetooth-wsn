
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

#ifndef __aodv_bt_h__
#define __aodv_bt_h__

#include "aodv/aodv.h"
#include "wnode.h"

#define MY_ROUTE_TIMEOUT_bt	100
#define ACTIVE_ROUTE_TIMEOUT_bt	100
#define REV_ROUTE_LIFE_bt	60
#define BCAST_ID_SAVE_bt	60
#define MAX_RREQ_TIMEOUT_bt	100

class AODV_BT:public AODV, public RoutingIF {
  public:
    AODV_BT(nsaddr_t id);

    // overiding AODV functions
    int command(int argc, const char *const *argv);
    virtual void recvReply(Packet *p);
    virtual void recvError(Packet *p); 
    virtual void rt_ll_failed(Packet *p);
    virtual void log_link_broke(Packet *p);

    // RoutingIF implementations
    virtual nsaddr_t nextHop(nsaddr_t dst);
    virtual void sendInBuffer(nsaddr_t dst);
    virtual void addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag);
    virtual void delRtEntry(nsaddr_t nexthop);
    virtual void linkFailed(Packet *p) { rt_ll_failed(p); }
};

#endif				// __aodv_bt_h__

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

#ifndef __ns_bt_node_h__
#define __ns_bt_node_h__

#include "mobilenode.h"

#include "baseband.h"
#include "bt-channel.h"
#include "lmp.h"
#include "l2cap.h"
#include "sco-agent.h"
#include "bnep.h"
#include "sdp.h"
#include "wnode.h"
#include "bt-stat.h"

class BTNodeTimer:public Handler {
  public:
    BTNodeTimer(class BTNode *nd): node_(nd) {}
    void handle(Event *);

  private:
    class BTNode *node_;
};

class BTNode:public WNode {
    friend class LMP;

  public:
    BTNode();
    virtual BTNode *getNext() { return next_; }
    virtual void setNext(BTNode *n) { next_ = n; }

    virtual BtStat *getStat() { return (BtStat *) stat_; }

    int command(int, const char *const *);
    void on();
    BTNode *lookupNode(bd_addr_t n) { return (BTNode *) WNode::lookupNode(n); }

    void setup(uint32_t addr, BTChannel *ch, Baseband * bb, LMP * lmp,
		       L2CAP * l2cap, BNEP *, SDP *);
    void sco_connect(BTNode *, ScoAgent *, ScoAgent *);
    void addScoAgent(ScoAgent * a);
    int masterLink(bd_addr_t rmt);
    void linkDetached(bd_addr_t addr, uchar reason);
    //void join(BTNode *slave, hdr_bt::packet_type pt, hdr_bt::packet_type rpt);
    void bnep_join(BTNode * slave, hdr_bt::packet_type pt,
		  hdr_bt::packet_type rpt);
    void setall_scanwhenon(int v);
    void force_on();
    void forceSuspendCurPico();
    L2CAPChannel *setupL2CAPChannel(ConnectionHandle *connh, bd_addr_t rmt, 
			L2CAPChannel *rcid);

    virtual void flushPkt(int addr);
    virtual void printStat();
    virtual void printAllStatExtra();
    virtual void energyReset();
    virtual void setdest(double x, double y, double z, double s);
    virtual void setIFQ(WNode *rmt, Queue *q);
    virtual void getIFQ(WNode *rmt);

    virtual int setTrace(const char *cmdname, const char *arg, int);

    void printClassified(FILE *out);

  public:
    BTChannel *phy_;
    Baseband *bb_;
    LMP * lmp_;
    L2CAP *l2cap_;
    BNEP *bnep_;
    ScoAgent *scoAgent_;
    SDP *sdp_;
    class ScatFormator * scatFormator_;

    int randomizeSlotOffset_;
    int enable_clkdrfit_in_rp_;

  private:
    static BTNode *chain_;
    BTNode *next_;
};

#endif				// __ns_bt_node_h__

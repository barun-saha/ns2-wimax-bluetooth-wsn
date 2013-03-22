/*
 * Copyright (c) 2004-2006, University of Cincinnati, Ohio.
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

#ifndef __bt_channel_h__
#define __bt_channel_h__

#include "bi-connector.h"

 
class Packet;
class Baseband;
class LMP;
class BTNode;
class BTLossMod;
class BTInterferQue;

class BTChannel;
class BTChanTxHandler:public Handler {
  public:
    BTChanTxHandler(BTChannel * ch):ch_(ch) {}
    inline void handle(Event * e);

  private:
    BTChannel * ch_;
};

class BTChanTxCompleteHandler:public Handler {
  public:
    BTChanTxCompleteHandler(BTChannel * ch):ch_(ch) {}
    inline void handle(Event * e);

  private:
    BTChannel * ch_;
};

class BTChanTxUnlockHandler:public Handler {
  public:
    BTChanTxUnlockHandler(BTChannel * ch):ch_(ch) {}
    inline void handle(Event * e);

  private:
    BTChannel * ch_;
};

class BTChanRxHandler:public Handler {
  public:
    BTChanRxHandler(BTChannel * ch):ch_(ch) {}
    inline void handle(Event * e);

  private:
    BTChannel * ch_;
};

// BTPicoChannel is defined by access code, fhs, and slot timing
// This class is used for speed up simulation for large network
class BTPicoChannel {
  public:
    BTPicoChannel(int n, BTChannel **ch, int ma): ref_(0), numCh_(n), 
	ch_(ch), master_(ma) {}

    int numCh() { return numCh_; }
    BTChannel *getCh(int i) { return ch_[i]; }
    int master() { return master_; }
    void incRef() { ref_++; }
    void decRef() { if (--ref_ == 0) { delete this; } }

    void dump(bd_addr_t ad, FILE *out = 0);

  private:
    ~BTPicoChannel() { delete ch_; }

  private:
    int ref_;
    int numCh_;
    BTChannel ** ch_;
    int master_;
};

class BTChannel : public BiConnector {
  public:
    BTChannel();
    virtual ~BTChannel();

    void setup(bd_addr_t addr, Baseband *bb, LMP *lmp, BTNode *nd) {
	addr_ = addr; bb_ = bb; lmp_ = lmp; node_ = nd;
    }

    void sendDown(Packet *p, Handler*) {
	Scheduler::instance().schedule(&txHandler_, p, BTDELAY);
    }
    void sendUp(Packet *p, Handler* h);

    void recvFirstBit(Packet*);
    void transmit(Packet *p);
    void transmitComplete(Packet *p);
    void unlockTx();

    static void setLossMode(BTLossMod *l) { delete lossMod_; lossMod_ = l; }
    bool lost(WNode * bb, hdr_bt * bh) {
	return lossMod_->lost(bb, bh);
    }
    bool collide(BTNode * bb, hdr_bt * bh) {
	return lossMod_->collide(bb, bh);
    }
    static BTInterferQue & interfQ(int i) { return interfQ_[i]; }

    int lookupPicoChannel(int ma);
    void setPicoChannel(BTPicoChannel *pico);
    void clearPicoChannel(int ma);

    bd_addr_t addr() { return addr_; }

  private:
    static BTChannel *chain_;
    BTChannel *next_;
    BTChanTxHandler txHandler_;
    BTChanTxCompleteHandler txFinHandler_;
    BTChanTxUnlockHandler unlockTxHandler_;
    BTChanRxHandler rxHandler_;
    Event unlockTxEv_;

    bd_addr_t addr_;
    Baseband *bb_;
    LMP *lmp_;
    BTNode *node_;
    BTPicoChannel **pico_;
    int numPico_;
    static BTLossMod *lossMod_;
    static BTInterferQue * interfQ_;
};

/////////////////////////

void BTChanTxHandler::handle(Event * e)
{
    ch_->transmit((Packet *) e);
}

void BTChanTxCompleteHandler::handle(Event * e)
{
    ch_->transmitComplete((Packet *) e);
}

void BTChanTxUnlockHandler::handle(Event * e)
{
    ch_->unlockTx();
}

void BTChanRxHandler::handle(Event * e)
{
    ch_->sendUp((Packet *) e, 0);
}

#endif 	// __bt_channel_h__

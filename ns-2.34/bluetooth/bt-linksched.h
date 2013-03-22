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


#ifndef __BT_LINKSCHED_H__
#define __BT_LINKSCHED_H__

#include "bt.h"
#include "hdr-bt.h"
#include "bt-stat.h"


class Baseband;
class BTSchedWord;
class LMPLink;

class BTLinkScheduler {
  public:
    enum Type { None,
	RR,
	DRR,
	ERR,
	PRR,
	FCFS,
	AWMMF
    };

    BTLinkScheduler(Baseband * bb);
    virtual ~BTLinkScheduler() {}

    virtual char type() = 0;
    virtual void init();
    virtual void update() {}
    virtual void reset() {}
    virtual int sched(int clk, BTSchedWord * sched, int curSlot) {
	return curSlot;
    }
    void setLastSchedSlot(int lss) {lastSchedSlot_ = lss; }
    int pass2(LMPLink *next);

  protected:
    Baseband * bb_;

    int lastSchedSlot_;
    int lastSchedSlot_highclass_;
    int inited_;
};

class BTRR:public BTLinkScheduler {
  public:
    BTRR(Baseband * bb):BTLinkScheduler(bb) {} 
    virtual char type() { return RR; }
    virtual int sched(int clk, BTSchedWord * sched, int curSlot);
};

class BTDRR:public BTLinkScheduler {
  public:
    BTDRR(Baseband * bb):BTLinkScheduler(bb) {} 
    virtual char type() { return DRR; }
    virtual int sched(int clk, BTSchedWord * sched, int curSlot);
};

class BTERR:public BTLinkScheduler {
  public:
    BTERR(Baseband * bb):BTLinkScheduler(bb) {}
    virtual char type() { return ERR; }
    virtual int sched(int clk, BTSchedWord * sched, int curSlot);
};

class BTPRR:public BTLinkScheduler {
  public:
    BTPRR(Baseband * bb):BTLinkScheduler(bb) {}
    virtual char type() { return PRR; }
    virtual int sched(int clk, BTSchedWord * sched, int curSlot);
};

class BTFCFS:public BTLinkScheduler {
  public:
    BTFCFS(Baseband * bb);
    virtual char type() { return FCFS; }
    virtual int sched(int clk, BTSchedWord * sched, int curSlot);
};

#endif				// __BT_LINKSCHED_H__

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

#ifndef __dsr_bt_h__
#define __dsr_bt_h__

#include "agent.h"
#include "ll.h"
#include "classifier-port.h"
#include "wnode.h"
#include "trace.h"

#define CRTENT_UNINI	0
#define CRTENT_ININI	1
#define CRTENT_UP	2

class CrtEntry {
  public:
    CrtEntry(nsaddr_t d, nsaddr_t n, CrtEntry *ne) 
	: next(ne), dst(d), nexthop(n), flag(0) {}
    CrtEntry *next;
    nsaddr_t dst;
    nsaddr_t nexthop;
    int flag;
};

class CrtTable {
  public:
    CrtTable():_table(0) {}

    void add(nsaddr_t dst, nsaddr_t nexthop) {
	CrtEntry * wk = _table;
	while (wk) {
	    if (wk->dst == dst) {
		wk->nexthop = nexthop;
		return;
	    }
	    wk = wk->next;
	}
	_table = new CrtEntry(dst, nexthop, _table);
    }

    CrtEntry * nextHopEnt(nsaddr_t dst) {
	CrtEntry * wk = _table;
	while (wk) {
	    if (wk->dst == dst) {
		return wk;
	    }
	    wk = wk->next;
	}
	return NULL;
    }

    nsaddr_t nextHop(nsaddr_t dst) {
	CrtEntry * wk = _table;
	while (wk) {
	    if (wk->dst == dst) {
		return wk->nexthop;
	    }
	    wk = wk->next;
	}
	return -1;
    }

    void remove(nsaddr_t dst) {
	CrtEntry * wk = _table;
	CrtEntry *par = _table;
	CrtEntry *t;
	while (wk) {
	    if (wk->dst == dst) {
		if (wk == _table) {
		    _table = _table->next;
		}
		t = wk;
		par->next = wk->next;
		delete t;
		return;
	    } else {
		par = wk;
		wk = wk->next;
	    }
	}
    }

    void removeNextHop(nsaddr_t nexthop) {
	CrtEntry * wk = _table;
	CrtEntry *par = _table;
	while (wk) {
	    if (wk->nexthop == nexthop) {
		if (wk == _table) {
		    _table = _table->next;
		}
		CrtEntry *t = wk;
		par->next = wk->next;
		wk = wk->next;
		delete t;
	    } else {
		par = wk;
		wk = wk->next;
	    }
	}
    }

  private:
    CrtEntry * _table;
};


class Manual_BT:public Agent, public RoutingIF {
  public:
    Manual_BT():Agent(PT_PING), RoutingIF() {} 

    virtual int command(int argc, const char *const *argv);
    virtual void recv(Packet*, Handler*);
    void prapare_for_traffic(nsaddr_t dst);
    inline void enque(Packet *p) {pq.enque(p);}
    Packet* deque(nsaddr_t dst);

    virtual nsaddr_t nextHop(nsaddr_t dst);
    virtual void sendInBuffer(nsaddr_t dst);
    // virtual void start();
    virtual void addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag);
    virtual void delRtEntry(nsaddr_t nexthop);

    CrtTable rtable;
    PacketQueue pq;

    PortClassifier *dmux_;
    Trace *tracetarget_;
};

#endif			

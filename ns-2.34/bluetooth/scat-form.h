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

#ifndef __ns_scat_form_h__
#define __ns_scat_form_h__

#include "bt.h"
#include "baseband.h"
#include "bt-node.h"

class BTNode;
class ScatFormator;

class ScatFormInqCallback : public Handler {
  public:
    ScatFormInqCallback(ScatFormator*f) : formator_(f) {}
    void handle(Event * e);
  private:
    ScatFormator *formator_;
};

class ScatFormPageCallback : public Handler {
  public:
    ScatFormPageCallback(ScatFormator*f) : formator_(f) {}
    void handle(Event * e);
  private:
    ScatFormator *formator_;
};

class ScatFormTimer:public Handler {
  public:
    ScatFormTimer(ScatFormator* f):formator_(f) {}
    void handle(Event * e);
  private:
    ScatFormator* formator_;
};

class ScatFormator {
  public:
    struct NodeInfo {
	NodeInfo *next;
	int id;
    };
    typedef enum { SFNone,
		SFLaw,
		SFPL,
		numSF
    } Type;
    typedef enum { GeometryConn, RandomConn, ActualConn } ConnType;
    struct Component {
	Component *next;
	BTNode * node;
	BTNode *nextNode;
	int id;
	int numNode;
	BTNode *ad[1024];
	Component(Component *n, int i)
	 :next(n), node(0), nextNode(0), id(i), numNode(0) {}
	Component(Component *n, BTNode *nd, int i)
	 :next(n), node(nd), nextNode(nd), id(i), numNode(0) {}
	void addNode(BTNode* nd) {
	    ad[numNode++] = nd;
	}
    };

    struct QNode {
	QNode *prev;
	QNode *next;
	BTNode *node;
	QNode(QNode *n, BTNode *bt): prev(0), next(n), node(bt) {
	    if (next) {next->prev = this;}
	}
    };
    class Queue {
      public:
	Queue():head_(0), tail_(0) {}
	~Queue(){ while (deque()); }

	void enque(BTNode *nd) {
	    head_ = new QNode(head_, nd); 
	    if (!tail_) {tail_ = head_;}
	}
	BTNode *deque() {
	    if (!tail_) {return 0;}
	    BTNode *ret = tail_->node;
	    QNode *tmp = tail_;	
	    tail_ = tail_->prev;
	    if (!tail_) { head_ = 0; }
	    delete tmp;
	    return ret;
	}
	
      private:
	QNode *head_;
	QNode *tail_;
    };

    ScatFormator(BTNode *n) :node_(n), bnep_(n->bnep_),
			      id_(n->bb_->bd_addr_), inqCallback_(this),
			      pageCallback_(this),
			      timer_(this), component_(0),
			      geoCompon_(0), randCompon_(0) {}
    virtual ~ScatFormator() {}
    virtual Type type() = 0;
    virtual void start() = 0;
    virtual void terminate() {} 
    inline void setNode(BTNode* n) {node_ = n;}
    virtual void inq_complete() {}
    virtual void page_complete() {}
    virtual void fire(Event *e) {}
    virtual void connected (bd_addr_t rmt) {}
    virtual void linkDetached(bd_addr_t rmt, uchar reason) {}
    virtual void recv_hello(Packet *p) { Packet::free(p); }
    virtual void dumpTopo();
    virtual void dump(FILE *out = 0) { 
	if (!out) out = stdout;
	fprintf(out, "\n");
    }
    virtual void checkConnectivity(ConnType type);

    int sendMsg(uchar id, uchar * content, int len, int dst, int nexthop,
		 int datasize = 0);
    virtual void recv(Packet *p, int rmt) { Packet::free(p); }

    void clearComponent(class Component ** comp) {
	while(*comp) {
	    Component *tmp = *comp;
	    *comp = (*comp)->next;
	    delete tmp;
	}
	*comp = 0;
    }
    void expandGeometryNode(BTNode * nd, Queue * q, int componId);
    void enqueueLink(LMPLink *link, Queue *q, int componId);
    void expandPiconet(Piconet *pico, Queue *q, int componId);
    void expandNode(BTNode *nd, Queue *q, int componId);
    void clearConnMark();
    void dumpDegree(FILE *out = 0);
    
  protected:
    BTNode * node_;	// A pointer to the Node I attached.
    ScatFormator *next_;	// used to link componet
    BNEP * bnep_;
    bd_addr_t id_;
    ScatFormInqCallback inqCallback_;
    ScatFormPageCallback pageCallback_;
    ScatFormTimer timer_;
    Event ev_;
    static int seqno_;


    Component *component_;
    Component *geoCompon_;
    Component *randCompon_;

};

#endif	// __ns_scat_form_h__

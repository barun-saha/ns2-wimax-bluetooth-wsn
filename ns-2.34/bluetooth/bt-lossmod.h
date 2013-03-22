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


#ifndef __BT_LOSSMOD_H__
#define __BT_LOSSMOD_H__
#include "distfer.h"
#include "bt.h"

class WNode;
class BTNode;
struct hdr_bt;

class BTLossMod {
  public:
    enum Type { None,
	BlueHoc,
	CoChBlueHoc
    };

    BTLossMod() {} 
    virtual ~BTLossMod() {} 
    virtual char type() { return None; }
    virtual bool lost(WNode * bb, hdr_bt * bh);
    virtual bool collide(BTNode * bb, hdr_bt * bh);
};

class LMBlueHoc:public BTLossMod {
  public:
    LMBlueHoc() {} 
    virtual char type() { return BlueHoc; }
    virtual bool lost(WNode * bb, hdr_bt * bh);
};

// This module is based on Guanhua Yan's code.
class LMCoChBlueHoc:public LMBlueHoc {
  public:
    LMCoChBlueHoc() {} 
    virtual char type() { return CoChBlueHoc; }
    virtual bool collide(BTNode * bb, hdr_bt * bh);
};

struct BTInterfer {
    int sender_;	// bd_addr_t ??
    int collided_;
    double X_, Y_, Z_;	// position of the source
    double st, et;	// start time / end time

    BTInterfer():sender_(-1), collided_(0), X_(-1), Y_(-1), Z_(-1), 
	st(-1), et(-1) {}
};

class BTInterferQue {
  public:
    BTInterferQue():_head(0), _tail(0) {}
    // return number of collided packets: 0/1/2
    int add(int s, double x, double y, double z, double st, double et);
    int collide(class BTNode *nd, hdr_bt *bh);
    
    int cochannel_collide(class BTNode *nd, hdr_bt *bh);
    double path_loss(double distance);

  private:
    int _head, _tail;
    BTInterfer _q[BTInterferQueMaxLen];
};
#endif				// __BT_LOSSMOD_H__

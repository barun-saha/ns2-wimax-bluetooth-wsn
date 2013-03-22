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

#ifndef __ns_scat_form_law_h__
#define __ns_scat_form_law_h__

#include "scat-form.h"
#include "baseband.h"
#include "lmp.h"

/*****************************************************************
* This algorithm is proposed by C. Law et. al. at MIT, and       *
* presented in several places.  Such as,                         *
*    C. Law and K.-Y. Siu, A Bluetooth scatternet formation      * 
*       algorithm, in: Proceedings of the IEEE Symposium on Ad   *
*       Hoc Wireless Networks 2001, San Antonio, TX (November    *
*       2001)                                                    *
*    C. Law, AK Mehta and K.-Y. Siu, "A new Bluetooth scatternet *
*       formation protocol", Mobile Netw. Apps., vol. 8, no. 5,  *
*       Oct. 2003                                                *
*****************************************************************/

struct SFLawSlaveInfo {
    uchar isLeader;
    uchar MaNumSlave;
    int master;
};

struct SFLawMoveInfo {
    int ad[MAX_SLAVE_PER_PICONET];
    int num;
    char isShared[MAX_SLAVE_PER_PICONET];
};

#define SCAT_LAW_DELTA 5
#define SCAT_LAW_P 0.5		// (1/3, 2/3)
#define SCAT_LAW_FAIL_SCHRED 7
#define SCAT_LAW_CASE3_DELAY 0.5	// second
#define SCAT_LAW_MERGE_DELAY 0.4	// second
#define SCAT_LAW_DISCONN_DELAY 0.3	// second
#define SCAT_LAW_MOVE_PAGE_DELAY 0.3
// #define SCAT_LAW_PAGETO 2048	// 0.64 s
#define SCAT_LAW_PAGETO 4098	// 1.28 s

class ScatFormLaw:public ScatFormator {
    enum { CmdScan, CmdInfo, CmdRetire, CmdLead, CmdMove, CmdMerge,
	CmdDisconn
    };
  public:
     ScatFormLaw(BTNode * n);

    inline Type type() { return SFLaw; }
    virtual void start() { _main(); }
    virtual void fire(Event * e);
    virtual void connected(bd_addr_t rmt);
    virtual void linkDetached(bd_addr_t rmt, uchar reason);
    virtual void recv(Packet * p, int rmt);

  protected:
    void _main();
    void _seek();
    void _scan();
    void _cancel_scan();	// used for a slave
    void _connected(int v, int visLeader, int w, int wNumSlave);
    void _merge(int v, int w);
    void _handleMerge(int w, int v);
    void _handleMerge_p2();
    void _migrate(int v, int w, int wNumSlave);
    void _move(int *y, int num, char *sh, int w, int v);
    void _askSlaveScan(int s, int nhop);
    void _askToRetire(int s, int nhop);
    void _askToBecomeLeader(int, int);
    void _askToDisconnect(int target, int otherend, int nhop);
    void _case3_p2();
    void _handleMove(SFLawMoveInfo *info);
    void _moveBeginPage();
    void _handleDisconn(int);
    void _handleDisconn_p2();
    void _becomeLeader();
    void _disconnect(int v, uchar reason);
    inline void _retire();
    void reset();
    void suspendMasterPicoForInqPage();
    void _beginPageScan();

    int numSlave();
    int _getUnsharedSlave();

  public:
    double _P;
    double _delta;
    int _K;

    // If a leader tails to get a new slave for _term_schred times, the 
    // algorithm terminates.
    int _term_schred;

    enum Status { SCAN, SEEK, CONN_MASTER, CONN_SLAVE, MERGE_wait,
	CASE3_wait, CONN_MASTER_waitToPage, DISCONN_wait
    } _status;

    bool _isLeader;

    int _maNumSlave;
    int _leaderMaster;

    int _scanner;
    int _v_merge;
    int _numOfSlaveToBeConnected;
    double _CONN_MASTER_st_t;
    double _CONN_SLAVE_st_t;
    int _v_disconn;
    double _t_main;
    int _fail_count;
    int _succ;

    int _case3_y;
    char _case3_sh;
    int _case3_v;

    int _moveAd[MAX_SLAVE_PER_PICONET];
    int _numMoveAd;

    Event pageDelayEv_;
    Event pageScanDelayEv_;
};

#endif				// __ns_scat_form_law_h__

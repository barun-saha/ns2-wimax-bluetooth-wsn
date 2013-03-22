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

#ifndef __ns_lmp_piconet_h__
#define __ns_lmp_piconet_h__

//** Added by Barun; for MIN/MAX
#include <sys/param.h>

#include "queue.h"
#include "baseband.h"
#include "lmp-link.h"
#include "bt-channel.h"

/*
 * 	piconet switching/scheduling	-- OUTDATED
 
1. After paging, lmp has some Links:
   1a. Master.  i) enter inq/page to get more link
   		  -- curPico
   		ii) enter scan to be connected.
		  -- new pico.
		  -- role switch
		iii) higher layer park/hold/sniff each ACL.
		  -- park each ACL, until all done, then i) or ii).

   1b. Slave.   i) enter inq/page to form a master piconet.
   		  -- new pico.
		  -- role switch, -- still new pico.
   		ii) enter scan to be connected.
		  -- new pico.
		iii) higher layer park/hold/sniff each ACL.
		  -- park ACL, then i) or ii).

2. Master: -- suspend ACLs
   2a. Inq/ConnectionReq
       -- add ACL to pico.  waiting suspend ACLs back.
   2b. Scan
       -- LMP check it's a new Pico, computing its life span for suspension

3. Slave: -- suspend ACLs
   3a. Inq/ConnectionReq/Sscan
       -- LMP check it's a new Pico. compute schedule.

 */

class L2CAPChannel;
class ConnectionHandle;
class LMP;

class Piconet {
  public:
    Piconet(Bd_info * mster, Bd_info * slave, LMPLink * l);
    Piconet(LMP *lmp);
    ~Piconet();
    void _init();
    void setPicoChannel();
    LMPLink *add_slave(Bd_info * remote, LMP * lmp,
		       ConnectionHandle * connh);
    LMPLink *add_slave(LMPLink * nl);
    void remove_slave(Bd_info * remote);
    // void add_bd(Bd_info * bd);
    void addScoLink(LMPLink *);
    void removeScoLink(LMPLink *);
    LMPLink *lookupLink(bd_addr_t bd);
    LMPLink *lookupScoLinkByAddr(bd_addr_t bd);
    void checkLink();
    int isMaster();
    void setSchedWord();
    int compute_sched();
    BTSchedWord *compute_bb_sco_sched(int m);
    BTSchedWord *compute_bb_sco_sched();
    void detach_link(LMPLink * link, uchar reason);
    void clear_skip();

    void dump(FILE *out = 0);

    // Boardcasting in piconet is handled this way:  The host pick up
    // a ConnectionHandle and set boardcasting flag in the Data Packet passed
    // to Host controller.  The host controller manages to transmit it.
    // No previous setup needed.  Essentially, two sepecial ConnectionHandles
    // identify two kinds of boardcasting.  It is simplified as function
    // call in this simulator.
    void picoBCast(Packet * p);	// send as the beacon signal
    void activeBCast(Packet * p);	// don't care of held, sniffing or packed ones.
    LMPLink *addBCastLink(int active);

    uchar allocPMaddr(bd_addr_t bd);
    uchar allocARaddr(bd_addr_t bd);

    int ltAddrIsValid(uchar lt);

    void add_slave_info(LMPLink::Slave_info *);

    uchar _getDsco();

    int _getScoHand() {
	++_scohand;
	return ++_scohand;
    } 
    LMPLink *lookupScoLink(int connh);

    double lookupWakeupTime();
    void suspend_link(LMPLink * nl);
    void unsuspend_link(LMPLink * nl);

    int allocLTaddr();
    TxBuffer *lookupTxBuffer(hdr_bt * bh);
    int scoUnoccupiedSlot();
    void add_sco_table(int D_sco, int T_poll, LMPLink * slink);

    void schedule_SCO();
    void cancel_schedule_SCO();
    int updateSchedWord(BTSchedWord *sw, bool isMa, bool aligned);

    int lookupRP(BrReq * brreq, LMPLink * exceptLink = 0);
    void setNeedAdjustSniffAttempt(LMPLink *);
    void masterAdjustLinkDSniff(uint16_t affectedDS, uint16_t att);
    void fwdCommandtoAll(unsigned char, uchar *, int, int, LMPLink *,
			  int);

    static BTSchedWord _mword;
    static BTSchedWord _sword;

    Piconet *prev, *next;
    bd_addr_t master_bd_addr_;
    clk_t master_clk_;
    // BTChannel *rfChannel_;

    int numActiveLink;
    LMPLink *activeLink;
    int numSuspendLink;
    LMPLink *suspendLink;
    int numScoLink;
    LMPLink *scoLink;
    int suspended;
    int suspendReqed;
    // int numSlave;
    // Bd_info *bd_info;
    // Bd_info *master;
    Bd_info *old_slave;
    int numOldSlave;
    LMP *lmp_;
    int clk_offset;
    int slot_offset;		// usec
    BTSchedWord *_sched_word;
    LMPLink *lt_table[MAX_SLAVE_PER_PICONET + 1];
    int lt_table_len;
    LMPLink *sco_table[6];
    int _scohand;
    double _res;

    uchar _DscoMap;
    clk_t _DscoMap_ts;

    LMPEvent _sco_ev;
    LMPEvent _sco_suspend_ev;

#ifdef PARK_STATE
    bd_addr_t pmaddr[PMADDRLEN];
    bd_addr_t araddr[ARADDRLEN];
#endif

    LMPLink *picoBCastLink;
    LMPLink *activeBCastLink;

    int num_sniff1;
    int num_sniff2;
    int RP;
    uint16_t T_sniff;
    int16_t prevFixedRP;
    int16_t prp;
    int rpFixed;

    inline int _gcd(int a, int b) {
	while (b) { int rem = a % b; a = b; b = rem; }
	return a;
    }
    inline int _lcm(int a, int b) { return a / _gcd(a, b) * b; }
};

#endif				// __ns_lmp_piconet_h__

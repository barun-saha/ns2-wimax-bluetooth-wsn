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

/*
 *  baseband.cc
 */

/*   --LIMITS: 
 * 1. clock wrap around is not handled.  -- not necessary since clk is small.
 * 2. Park/unpark is not completed.  -- no plan to complete it.
 * 3. RS take over is not completed. -- need to be complete.
 * 4. GO/STOP bit is not used. -- No queue at receiver side to make use of it.
 * 5. eSCO is not implemented. -- need to be complete.
 */

/*	Baseband duty cycle specified in Spec 1.2
 *
 *	Inquiry Scan --
 *	    1. performance oriented: T_inquiry_scan_ = 320 tick()s (100us)
 *		interlaced scan
 *	    2. power consumption is important:
 *		T_inquiry_scan_ = 8192 (2.56s)
 *		interlaced scan
 *	    3. power consumption critical:
 *		T_inquiry_scan_ = 8192 (2.56s)
 *		no interlaced scan
 *	    4. SCO/ESCO present: interlaced scan.

	Page Scan --

                  Page Scan Page Scan
                  Interval  Window
        Scenario                              Scan Type
        
        R0 (1.28s)        TGAP(107) TGAP(107) Normal scan
        Fast R1 (100ms)   TGAP(106) TGAP(101) Interlaced scan
        Medium R1 (1.28s) TGAP(107) TGAP(101) Interlaced scan
        Slow R1 (1.28s)   TGAP(107) TGAP(101) Normal scan
        Fast R2 (2.56s)   TGAP(108) TGAP(101) Interlaced scan
        Slow R2 (2.56s)   TGAP(108) TGAP(101) Normal scan

	Time value defined in General Access profile:

	TGAP(100) 10.24 s 
	TGAP(101) 10.625 ms 
	TGAP(102) 2.56 s  
	TGAP(103) 30.72 s 
	TGAP(104) 1 min.  
	TGAP(105) 100ms 
	TGAP(106) 100ms 
	TGAP(107) 1.28s  
	TGAP(108) 2.56s  
 */

#include "baseband.h"
#include "lmp.h"
#include "lmp-piconet.h"
#include "bt-stat.h"
#include "bt-node.h"
#include "bt-channel.h"
#include "stdio.h"
#include "random.h"
#include "wireless-phy.h"
#include "scat-form.h"

#define DUMPSYNBYGOD
#define PRINTRESYN
// #define SEQ_8BIT
// #define PRINT_RECV_TIMING_MISMATCH
// #define PRINT_TPOLL_BKOFF
// #define PRINT_SUSPEND_TXBUF
#define FILTER_OUT_UNDETECTED_UNINTENED_RCVR

// #define DUMPTRX
#ifdef DUMPTRX
#define DUMP_TRX_ST fprintf(stdout, "%d %f %s st:%d\n", bd_addr_, now, \
				__FUNCTION__, trx_st_)
#define DUMP_TRX_ST1 fprintf(stdout, "%d %s st:%d\n", bd_addr_, \
				__FUNCTION__, trx_st_);
#else
#define DUMP_TRX_ST
#define DUMP_TRX_ST1
#endif

// Added by Barun: 13 Mar 2012
#undef BTDEBUG
// End

static class BTHeaderClass:public PacketHeaderClass {
  public:
    BTHeaderClass():PacketHeaderClass("PacketHeader/BT", sizeof(hdr_bt)) {
	bind_offset(&hdr_bt::offset_);
    }
} class_bthdr;

static class BasebandClass:public TclClass {
  public:
    BasebandClass():TclClass("Baseband") { }
    TclObject *create(int, const char *const *argv) {
	return new Baseband();
    }
} class_baseband;


//////////////////////////////////////////////////////////
//                                                      //
//                  hdr_bt                              //
//                                                      //
//////////////////////////////////////////////////////////
int hdr_bt::offset_;
int hdr_bt::pidcntr = 0;
const char *const hdr_bt::_pktTypeName[] = {
    "NULL", "POLL", "FHS", "DM1", "DH1", "HV1", "HV2", "HV3", "DV",
    "AUX1", "DM3", "DH3", "EV4", "EV5", "DM5", "DH5", "ID", "EV3",
    "2-DH1", "3-DH1",
    "2-DH3", "3-DH3", "2-DH5", "3-DH5",
    "2-EV3", "3-EV3", "2-EV5", "3-EV5",
    "HLO", "NotSpecified", "Invalid"
};
const char *const hdr_bt::_pktTypeName_s[] = {
    "NU", "PO", "FH", "M1", "H1", "V1", "V2", "V3", "DV", "AU",
    "M3", "H3", "E4", "E5", "M5", "H5", "ID", "E3", "D1", "T1",
    "D3", "T3", "D5", "T5", "F3", "G3", "F5", "G5",
    "HL", "NotSpecified", "Invalid"
};

void hdr_bt::dump()
{
    if (receiver == BD_ADDR_BCAST) {
	printf("B %d:%d-*:%d ", sender, srcTxSlot, dstTxSlot);
    } else {
	printf("B %d:%d-%d:%d ", sender, srcTxSlot, receiver, dstTxSlot);
    }

    // pktType lt ac fs seqn transmitCount size timestamp transId pktId comm
    printf("%s %d %d %.02d %d %d cn:%d %.03d %f %d:%d %s\n",
	   packet_type_str_short(type), lt_addr_, ac, fs_, arqn, seqn,
	   transmitCount, size, ts(), pid, seqno, comment());
}

void hdr_bt::dump(FILE * f, char stat, int ad, const char *st)
{
    if (receiver == BD_ADDR_BCAST) {
	/** Commented by Barun [07 March 2013]
	fprintf(f, BTPREFIX "%c %d %s %d:%d-*:%d ", stat, ad, st, sender,
		srcTxSlot, dstTxSlot);
	*/
    } else {
	/** Commented by Barun [07 March 2013]
	fprintf(f, BTPREFIX "%c %d %s %d:%d-%d:%d ", stat, ad, st, sender,
		srcTxSlot, receiver, dstTxSlot);
	*/
    }

    // pktType lt ac fs seqn transmitCount size timestamp transId pktId comm
	/** Commented by Barun [07 March 2013]
    fprintf(f, "%s %d %d %.02d %d %d c:%d %.03d %f %d:%d %s %d %d\n",
	    packet_type_str_short(type), lt_addr_, ac, fs_, arqn, seqn,
	    transmitCount, size, ts(), pid, seqno, comment(), clk,
	    extinfo);
	*/
}

void hdr_bt::dump_sf()
{
    // printf(" 0x%x t:%d c:%d l:%d d:%d data: %d %d %d %d\n",
    // (unsigned int) this,
    printf("  --- t:%d c:%d l:%d d:%d data: %d %d %d %d\n",
	   u.sf.type, u.sf.code, u.sf.length, u.sf.target,
	   u.sf.data[0], u.sf.data[1], u.sf.data[2], u.sf.data[3]);
}

//////////////////////////////////////////////////////////
//                                                      //
//                  BTChannelUseSet                     //
//                                                      //
//////////////////////////////////////////////////////////
BTChannelUseSet::BTChannelUseSet(uint16_t map_h, uint32_t map_m,
				 uint32_t map_l)
{
    _u.s.map_h = ntohs(map_h);
    _u.s.map_m = ntohl(map_m);
    _u.s.map_l = ntohl(map_l);
    _expand();
}

void BTChannelUseSet::reset()
{
    for (int i = 0; i < 9; i++) {
	_u.map[i] = 0xFF;
    }
    _u.map[9] = 0x7F;
    _expand();
}

void BTChannelUseSet::importMapByHexString(const char *ch)
{
    int len = strlen((const char *) ch);
    if (len == 22 && ch[0] == '0' && (ch[1] == 'x' || ch[1] == 'X')) {
	ch += 2;
    } else if (len != 20) {
	abort();
    }

    for (int i = 0; i < 10; i++) {
	if (ch[2 * i] >= '0' && ch[2 * i] <= '9') {
	    _u.map[9 - i] = ch[2 * i] - '0';
	} else if (ch[2 * i] >= 'A' && ch[2 * i] <= 'F') {
	    _u.map[9 - i] = ch[2 * i] - 'A' + 10;
	} else if (ch[2 * i] >= 'a' && ch[2 * i] <= 'f') {
	    _u.map[9 - i] = ch[2 * i] - 'a' + 10;
	} else {
	    abort();
	}
	_u.map[9 - i] <<= 4;

	if (ch[2 * i + 1] >= '0' && ch[2 * i + 1] <= '9') {
	    _u.map[9 - i] += (ch[2 * i + 1] - '0');
	} else if (ch[2 * i + 1] >= 'A' && ch[2 * i + 1] <= 'F') {
	    _u.map[9 - i] += (ch[2 * i + 1] - 'A' + 10);
	} else if (ch[2 * i + 1] >= 'a' && ch[2 * i + 1] <= 'f') {
	    _u.map[9 - i] += (ch[2 * i + 1] - 'a' + 10);
	} else {
	    abort();
	}
    }
    _expand();
}

void BTChannelUseSet::importMap(uchar * map)
{
    memcpy(_u.map, map, 10);
    _expand();
}

uchar *BTChannelUseSet::exportMap(uchar * map)
{
    if (map) {
	memcpy(map, _u.map, 10);
	return map;
    } else {
	return _u.map;
    }
}

char *BTChannelUseSet::toHexString(char *s)
{
    char *ret = s;
    if (!s) {
	ret = new char[21];
    }
    sprintf(ret, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
	    _u.map[9], _u.map[8], _u.map[7], _u.map[6], _u.map[5],
	    _u.map[4], _u.map[3], _u.map[2], _u.map[1], _u.map[0]);
    ret[20] = '\0';
    return ret;
}

void BTChannelUseSet::_expand()
{

    int numOddCh = 0;
    char oddCh[40];
    int i, j;

    _numUsedCh = 0;
    _u.map[9] &= 0x7F;

    for (i = 0; i < 10; i++) {
	for (j = 0; j < 8; j++) {
	    if ((_u.map[i] >> j) & 0x01) {
		if ((j & 0x0001)) {	// odd
		    oddCh[numOddCh++] = i * 8 + j;
		} else {
		    _usedCh[_numUsedCh++] = i * 8 + j;
		}
		_notUsedCh[i * 8 + j] = 0;
	    } else {
		_notUsedCh[i * 8 + j] = 1;
	    }
	}
    }

    memcpy(&_usedCh[_numUsedCh], oddCh, sizeof(char) * numOddCh);
    _numUsedCh += numOddCh;
}


//////////////////////////////////////////////////////////
//                                                      //
//                      TxBuffer                        //
//                                                      //
//////////////////////////////////////////////////////////
TxBuffer::TxBuffer(class Baseband * bb, class LMPLink * l, int s)
:current_(NULL), next_(NULL), bb_(bb), link_(l), slot_(s),
dstTxSlot_(0),
send_ack_(0), seqn_rx_(0), seqn_tx_(0), arqn_(0),
bd_addr_(bb->bd_addr_),
afhEnabled_(0), scoStarted_(0),
t_index_(-1), t_cntr_(0),
lastPktRecvT_(0), lastDataPktRecvT_(0), lastSynT_(0),
ttlFails_(0), fails_(0), ttlSent_(0), nullCntr_(0),
suspended_(0),
hasData_(1),
prioClass_(Low),
prio_(PRIO_PANU),
deficit_(0),
ttlSlots_(0),
T_poll_max_(Baseband::T_poll_max_),
T_poll_default_(Baseband::T_poll_default_),
T_poll_(Baseband::T_poll_default_),
lastPollClk_(0), lastPollClk_highclass_(0),
remote_bd_addr_((l ? l->remote->bd_addr_ : -1)),
txType_(hdr_bt::Invalid), rxType_(hdr_bt::Invalid)
{
}


// return TxBuffer type: 0 - ACL   1 - SCO   2 - baseband (BBSIG)
// current implementation doesn't put baseband signaling pkt into TxBuffer
// So, it never returns 2.
int TxBuffer::type()
{
    return (link_ ? link_->type() : BBSIG);
}

// #define DEBUGGHOSTPKT

#ifdef DEBUGGHOSTPKT
#define SHOWBUFFER printf("%d:%d %s cur: %x next: %x\n", bd_addr_, \
		slot_, __FUNCTION__, (uint32_t)current_, (uint32_t)next_)
#else
#define SHOWBUFFER
#endif

void TxBuffer::reset_seqn()
{
    // Inversed before a new CRC pkt is transmitted.
    // Therefore the first CRC pkt carrying seqn 1 as specs says.
    seqn_tx_ = 0;

    // The first incoming CRC pkt carrying seqn 1, which is 
    // different from this value, is identified as a new pkt.
    seqn_rx_ = 0;

    // ACK
    arqn_ = 0;

    if (current_) {
	Packet::free(current_);
	current_ = NULL;
    }
    if (next_) {
	Packet::free(next_);
	next_ = NULL;
    }
}

///////////////////////////////////////////////////////////////////////
//   Note about ACK/NAK                                              //
//-------------------------------------------------------------------//
// Facts in current implentment
// 1. A CRC pkt is always retranmitted until an ACK is received, or a flush
//    is performed. No other pkts are in between.
// 2. If a non-CRC pkt is received, all previous CRC pkts are ACK'd.
// 3. Therefore, upon receiving a non-CRC pkt, NAK can be set, although
//    spec1.1 says ACK is not changed.
// 4. A slave always gets a pkt prior to sending something.  It cannot 
//    detect pkt loss.  If loss happens, it thinks it is not polled.
//    This may not true if a better collison detection module is used.
//      -- we may work on this such that the slave may see a correct hdr
//         instead of the whole packet, so it know it is polled. TODO
// 5. A master can detect pkt loss, if it receives nothing in the 
//    following slot.

// Current ACK/NAK scheme in ucbt
// 1. Upon receiving a CRC pkt sucessfully, ACK is set, otherwise NAK is set.
// 2. A master is check the possible loss in receiving slot to set NAK.
// 3. Retransmission will update ACK.
// 
// **** This model can be refined if the ErrMode differentiates header error
//      and payload error.

// transmit a packet in TxBuffer
// if suspended_ == 1, only POLL pkts are transmitted.
// In case that two ends of the link are not syn'd, eg. one is awake 
// while the other is in Hold/Sniff/Pack state, the txBuffer is marked 
// as suspended, so that Data pkt is not flushed.
///////////////////////////////////////////////////////////////////////

// This function is called when BB decides to transmit a pkt from
// this TxBuffer, i.e. the timing is aligned to the beginning of a slot.
Packet *TxBuffer::transmit()
{
    hdr_bt *bh;

    // The peer is not present.  Sends POLL pkt instead.
    // Note this behavior is not defined in the spec.  Spec2.0 does suggest
    // in case if possbile out of sync, MA may transmit one-slot packet so
    // slaves can re-sync more easily.
    if (suspended_) {
#ifdef PRINT_SUSPEND_TXBUF
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_, "%d:%d TxBuffer is suspended.\n",
		bb_->bd_addr_, slot_);
	*/
#endif
	Packet *p = bb_->stamp(link_->genPollPkt(), this);
	bb_->polled_lt_addr_ = HDR_BT(p)->lt_addr_;
	HDR_BT(p)->nokeep = 1;

	return p;		// XXX When to free p ??
    }

    SHOWBUFFER;
    // check if a flush is needed, or a new pkt should be transmitted.
    if (current_) {
	bh = HDR_BT(current_);

#ifdef BTDEBUG
	if (bh->comment_ == NULL) {	// debug purpose
	    // Barun
	    //fprintf(stderr,
		    //"*%d %f ghost packages in TxBuffer::transmit()\n",
		    //bb_->bd_addr_, Scheduler::instance().clock());
	    //bh->dump(stderr, BTTXPREFIX, bb_->bd_addr_, bb_->state_str_s());	// "t "
	    current_ = 0;
	}
#endif

	if (bh->bcast && bh->transmitCount >=
	    link_->lmp_->_num_Broadcast_Retran) {
	    switch_reg();
	} else if (bh->transmitCount > 0 && !bh->isCRCPkt()) {
	    switch_reg();
	} else if (bh->transmitCount >= link_->lmp_->_max_num_retran) {
	    flush();
	}
    }
    // try to get a data pkt to send if current_ is NULL.
    if (!current_) {
	current_ = next_;
	link_->callback();	// get a packet from lmp if it has one.
    }
    // send a ACK if it can not be piggy-backed.
    if (!current_ && send_ack_) {
	if (bb_->isMaster()) {
	    // Master sends POLL pkt including ACK.
	    // We let the master always send POLL, so a reply is expected.
	    link_->sendPoll(1);
	} else {
	    link_->sendNull();
	}
    }
    send_ack_ = 0;

    // master sends a POLL packet on ACL link if it has nothing to send
    if (!current_ && bb_->polling_ && type() == ACL &&
	slot_ >= Baseband::MinTxBufferSlot) {
	link_->sendPoll();	// NAK
    }
    // I really have nothing to send at this moment.
    if (!current_) {
	return NULL;
    }
    // Finally, a packet is ready to transmit.
    bh = HDR_BT(current_);
#ifdef BTDEBUG
    if (bh->comment_ == NULL) {	// It should not happen.
	// Barun
	//fprintf(stderr,
		//"**%d:%d %x %f ghost packages in TxBuffer::transmit()\n",
		//bb_->bd_addr_, slot_, (uint32_t) current_,
		//Scheduler::instance().clock());
	//bh->dump(stderr, BTTXPREFIX, bb_->bd_addr_, bb_->state_str_s());	// "t "
	current_ = NULL;
	return NULL;
    }
#endif

    bb_->stamp(current_, this);
    if (bh->transmitCount == 1) {	// new packet
	if (bh->ph.l_ch == L_CH_L2CAP_CONT && bh->ph.length == 0) {
	    // a new flush pkt. seqn_tx_ is not changed.
	    bh->seqn = seqn_tx_;
	} else if (bh->isCRCPkt()) {
#ifdef SEQ_8BIT
	    seqn_tx_++;
#else
	    seqn_tx_ = !seqn_tx_;
#endif
	    bh->seqn = seqn_tx_;
	}
    }
    // Check if the master receives successfully at the slave-to-master slot.
    // (arqn_ is set to 0(NAK) when receiving a non-CRC pkt.)
    if (bb_->isMaster()) {
	if (fails_ > 0) {
	    arqn_ = 0;
	}
    }
    bh->arqn = arqn_;

    bb_->polled_lt_addr_ = HDR_BT(current_)->lt_addr_;

    // fails_ is basically the transmission counter. It is reseted
    // to 0 upon a successful receiption.
    if (!bh->bcast) {
	fails_++;
    }
    ttlSent_++;
    if (type() != SCO) {
	if (fails_ > FAILS_SCHRED_FOR_TPOLL_DBL) {
	    T_poll_ += T_poll_;
	    if (T_poll_ > T_poll_max_) {
		T_poll_ = T_poll_max_;
	    }
	}
	if (fails_ >= link_->lmp_->failTriggerSchred_) {
	    link_->failTrigger();
	}
    }

    return current_;
}

// return: 1 -- p will go to upper layer,
//         0 -- p will be dropped.
int TxBuffer::handle_recv(Packet * p)
{
    // Scheduler & s = Scheduler::instance();
    SHOWBUFFER;
    lastPktRecvT_ = bb_->clk_;
    hdr_bt *bh = HDR_BT(p);
    if (bh->txBuffer->dstTxSlot() == 0) {
	bh->txBuffer->dstTxSlot_ = bh->srcTxSlot;
    }
    rxType_ = bh->type;

    if (type() == SCO) {
	if (!scoStarted_) {
	    if (bh->scoOnly()) {
		scoStarted_ = 1;
	    } else {
		return 0;
	    }
	}
	if (bh->isScoPkt()) {
	    if (bh->type == hdr_bt::DV) {
		fprintf
		    (stderr,
		     "\n*** DV packet can not handled by the simulator.\n");
		exit(1);
	    }
	    ttlFails_ += (fails_ > 1 ? fails_ - 1 : 0);
	    fails_ = 0;
	    return 1;
	} else {		// non-SCO pkt
	    return 0;
	}
    } else if (type() != ACL) {
	return 0;
    }
    // when in SNIFF mode, notify a incoming pkt.
    if (link()->_in_sniff_attempt) {
	link()->recvd_in_sniff_attempt(p);
    }

    resume();			// clear the suspended_ flag if set.
    if (!bb_->isMaster()) {
	bb_->polled(bh->txBuffer->slot());	// turn on tx in schedWord
	update_T_poll();	// Estimating T_poll using by link mode negotiation.

    } else {

	if (bh->type == hdr_bt::Null && txType_ == hdr_bt::Poll) {
	    hasData_ = 0;
	} else {
	    hasData_ = 1;
	}

	deficit_ -= hdr_bt::slot_num(bh->type);
	ttlSlots_ += hdr_bt::slot_num(bh->type);
    }

    ttlFails_ += (fails_ > 1 ? fails_ - 1 : 0);
    fails_ = 0;

    if (bh->type != hdr_bt::Poll && bh->type != hdr_bt::Null) {
	lastDataPktRecvT_ = bb_->clk_;
	nullCntr_ = 0;
	T_poll_ = T_poll_default_;
    } else {
	nullCntr_++;
	if (!hasData_ && bb_->isMaster()) {
	    T_poll_ += T_poll_;
	    if (T_poll_ > T_poll_max_) {
		T_poll_ = T_poll_max_;
	    }
#ifdef PRINT_TPOLL_BKOFF
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    "Receive POLL/NULL %d times, back off %d\n", nullCntr_,
		    T_poll_);
	*/
#endif
	}
	if (nullCntr_ >= link_->lmp_->nullTriggerSchred_) {
	    link_->nullTrigger();
	}
    }

    if (bh->isCRCAclPkt()) {
	arqn_ = 1;		// send ACK
	uchar seqn_expect;

#ifdef SEQ_8BIT
	if (bh->transmitCount == 1) {	// Debug purpose.
	    if (bh->seqn != (uchar) (seqn_rx_ + 1)) {
		fprintf(stderr,
			"seqn wrong: %d expect %d fixit by cheating.\n",
			bh->seqn, (uchar) seqn_rx_ + 1);
		seqn_rx_ = bh->seqn - 1;
	    }
	}
	if (bh->seqn != (uchar) (seqn_rx_ + 1)) {	// retransmission
	    seqn_expect = seqn_rx_ + 1;
#else
	if (bh->seqn == seqn_rx_) {	// retransmission
	    seqn_expect = (seqn_rx_ ? 0 : 1);
#endif
	    // Barun
	    //fprintf(BtStat::log_, "Duplicated pkt: seqn %d, expect %d ",
		    //bh->seqn, seqn_expect);
	    //bh->dump(BtStat::log_, BTRXPREFIX, bb_->bd_addr_,
		     //bb_->state_str_s());

	    link()->sendACK();
	    ack(bh->arqn);
	    return 0;
	} else {
	    seqn_rx_ = bh->seqn;	// ie. SEQN_old
	}
    } else {
	arqn_ = 0;
    }

    ack(bh->arqn);

    if (bh->isCRCAclPkt() || bh->type == hdr_bt::Poll) {
	link()->sendACK();
    }

    return ((bh->isCRCAclPkt() || bh->type == hdr_bt::AUX1) ? 1 : 0);
}

// slave measure T_poll in last TPOLLUPDATE_PKTNUM packets.
void TxBuffer::update_T_poll()
{
    int i, j;
    int32_t sum = 0;

    if (++t_index_ == TPOLLUPDATE_PKTNUM) {
	// t_index_ initiated to -1 or TPOLLUPDATE_PKTNUM - 1
	t_index_ = 0;
    }

    T_[t_index_] = bb_->clk_;
    if (t_cntr_ < TPOLLUPDATE_PKTNUM - 1) {
	if (t_cntr_ > 0) {
	    for (i = 0; i < t_cntr_; i++) {
		sum += T_[i + 1] - T_[i];
	    }
	    link_->T_poll_ = sum / t_cntr_ / 2;
	}
	t_cntr_++;
    } else {			// t_cntr_ = TPOLLUPDATE_PKTNUM - 1
	// T_[] can store TPOLLUPDATE_PKTNUM - 1 newest T_poll's.
	j = t_index_;
	for (i = 0; i < t_cntr_; i++) {
	    if (j == 0) {
		sum += T_[0] - T_[TPOLLUPDATE_PKTNUM - 1];
		j = 7;
	    } else {
		sum += T_[j] - T_[j - 1];
		j--;
	    }
	}
	link_->T_poll_ = sum / t_cntr_ / 2;
    }
}

// ack() implements ARQ
void TxBuffer::ack(int a)
{
    if (a) {
	// check if a ACK for detach request arrived.
	if (current_
	    && HDR_BT(current_)->u.lmpcmd.opcode == LMP::LMP_DETACH) {
	    link()->recv_detach_ack();
	}
	switch_reg();
    }
}

// LMP puts a packet into TxBuffer
// return 0 on failure -- Buffer is full.
//        1 on success
int TxBuffer::push(Packet * p)
{
    SHOWBUFFER;
    if (!available()) {
	return 0;
    }

    if (current_ == NULL && next_ == NULL) {
	current_ = p;
    } else if (next_ == NULL) {
	next_ = p;
    } else {
	current_ = next_;
	next_ = p;
    }
    return 1;
}

// 1. discard the current CRC pkt
// 2. send L2CAP cont pkt with lengh 0
// 3. send new L2CAP pkt upon receiving a ACK
void TxBuffer::flush()
{
    SHOWBUFFER;
	/** Commented by Barun [07 March 2013]
    fprintf(BtStat::log_, "# %d:%d flush ", bd_addr_, slot_);
	*/
    if (current_) {
	HDR_BT(current_)->dump(BtStat::log_, BTRXPREFIX, bb_->bd_addr_,
			       bb_->state_str_s());
	Packet::free(current_);
    } else {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_, "current_ is NULL.\n");
	*/
    }

    // discard L2CAP cont pkts.
    // if next_ is a L2CAP cont pkt, discard it.
    // discard L2CAP cont pkt in link l2capQ.
    //
    // if next_ is a L2CAP start pkt, it will be kept.
    //
    // Note that spec1.1 says if next_ is a new L2CAP start pkt,
    // it can be transmited immediately. ie. set current_ to it.
    // Here a L2CAP Cont Pkt with length 0 is always transmitted.
    if (!next_) {
	link_->flushL2CAPPkt();
    } else if (HDR_BT(next_)->ph.l_ch == L_CH_L2CAP_CONT) {
	Packet::free(next_);
	link_->flushL2CAPPkt();
	next_ = NULL;
    }
    // generate a L2CAP Cont Pkt with length 0.
    current_ = Packet::alloc();
    hdr_cmn *ch = HDR_CMN(current_);
    hdr_bt *bh = HDR_BT(current_);

    ch->ptype() = PT_L2CAP;
    bh->pid = hdr_bt::pidcntr++;
    bh->type = hdr_bt::DM1;
    bh->ph.l_ch = L_CH_L2CAP_CONT;
    bh->ph.length = 0;
    bh->size = hdr_bt::packet_size(bh->type, bh->ph.length);
    bh->receiver = remote_bd_addr_;
    bh->comment("FL");
}

void TxBuffer::switch_reg()
{
    SHOWBUFFER;
    if (current_) {
	if (current_->uid_ > 0) {	// Shouldn't happen
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, "%d current_ scheduled to be freed. ",
		    bd_addr_);
	*/
	    HDR_BT(current_)->dump(BtStat::log_, 'F', bb_->bd_addr_,
				   "XXX");
	    Scheduler::instance().cancel(current_);
	}
	Packet::free(current_);
    }
    current_ = next_;
    next_ = NULL;
    link_->callback();
}

int TxBuffer::hasBcast()
{
    if (current_ && HDR_BT(current_)->transmitCount >=
	bb_->lmp_->_num_Broadcast_Retran) {
	switch_reg();
    }
    return (current_ ? 1 : 0);
}

// Used by the master to implement link scheduling.
// The master decides if T_poll should be increased.
int TxBuffer::hasDataPkt()
{
    return hasData_ && !suspended_;
#if 0
    return !suspended_ && (nullCntr_ < 2 ||
			   (current_
			    && HDR_BT(current_)->type != hdr_bt::Poll));
#endif
}


//////////////////////////////////////////////////////////
//                      BTSchedWord                     //
//////////////////////////////////////////////////////////

// NOTE: SchedWord control is almost out of date.

// SchedWord as a control word for bb set by lmp.  Think of it as one of
// the methods to implement timing control over bb.

// For baseband signaling (page, inq, ..., etc), 
//      Master: send at even slots, receive at odd slots, like srsrsrsrsr
//      Slave:  allow to send at arbitrary time, since it has to align 
//              with Master's clock.  like ssssssssss.

BTSchedWord::BTSchedWord(bool b):in_use(0), len(2)
{
    word = new uint8_t[len];
    if (b) {			// can send bb signals in any slot
	word[0] = word[1] = Baseband::BasebandSlot;
    } else {			// cannot send
	word[0] = word[1] = Baseband::RecvSlot;
    }
}

// copy constructor
BTSchedWord::BTSchedWord(BTSchedWord & b):in_use(0), len(b.len)
{
    word = new uint8_t[len];
    memcpy(word, b.word, sizeof(uint8_t) * len);
}

BTSchedWord::BTSchedWord(BTSchedWord & b, int l):in_use(0), len(l)
{
    word = new uint8_t[len];
    int s = (len < b.len ? len : b.len);
    memcpy(word, b.word, sizeof(uint8_t) * s);
    for (int i = b.len; i < len; i++) {
	word[i++] = Baseband::BasebandSlot;
	word[i] = Baseband::RecvSlot;
    }
}

// generate schedule word for the master confirming to Bluetooth BB specs. 
// i.e. send at even slots, receive at odd slots.
BTSchedWord::BTSchedWord(int l):in_use(0), len(l)
{
    word = new uint8_t[len];
    for (int i = 0; i < len; i++) {
	if (i % 2 < 1) {
	    word[i] = Baseband::BasebandSlot;
	} else {
	    word[i] = Baseband::RecvSlot;
	}
    }
}

// generate a default schedule word for normal pico operation
// drdrdr/rrrrrr
BTSchedWord::BTSchedWord(int l, int master, int dummy):in_use(1), len(l)
{
    word = new uint8_t[len];

    for (int i = 0; i < len; i++) {
	if (master && (i % 2 == 0)) {
	    word[i] = Baseband::DynamicSlot;;
	} else {
	    word[i] = Baseband::RecvSlot;
	}
    }
}

BTSchedWord::BTSchedWord(int l, bool b):in_use(0), len(l)
{
    word = new uint8_t[len];
    for (int i = 0; i < len; i++) {
	if (b) {
	    word[i] = Baseband::BasebandSlot;
	} else {
	    word[i] = Baseband::RecvSlot;
	}
    }
}

void BTSchedWord::dump(FILE * out)
{
    if (!out) {
	out = BtStat::log_;
    }
	/** Commented by Barun [07 March 2013]
    fprintf(out, "BTSchedWord(%d):", len);
	*/
    if (len == 1) {
	/** Commented by Barun [07 March 2013]
	fprintf(out, "ooops, len = 1\n");
	*/
    }
    for (int i = 0; i < len; i++) {
	/** Commented by Barun [07 March 2013]
	fprintf(out, " %d", word[i]);
	*/
    }
    fprintf(out, "\n");
}


//////////////////////////////////////////////////////////
//                      Baseband                        //
//////////////////////////////////////////////////////////

int Baseband::trace_all_tx_ = 1;
int Baseband::trace_all_rx_ = 1;
int Baseband::trace_all_poll_ = 1;
int Baseband::trace_all_null_ = 1;
int Baseband::trace_all_in_air_ = 0;
int Baseband::trace_all_stat_ = 1;

int32_t Baseband::T_poll_max_ = MAX_T_POLL;
int32_t Baseband::T_poll_default_ = T_POLL_DEFAULT;

int Baseband::useSynByGod_ = 0;

Baseband::Baseband()
:
clkn_handler_(this), clk_handler_(this), resync_handler_(this),
trxtoTimer_(this), unlockRxHandler_(this),
slaveSendnonConnHandler_(this), slaveChangeStHandler_(this)
{
    bind("T_w_inquiry_scan_", &T_w_inquiry_scan_);
    bind("T_inquiry_scan_", &T_inquiry_scan_);
    bind("Inquiry_Scan_Type_", &Inquiry_Scan_Type_);

    bind("T_w_page_scan_", &T_w_page_scan_);
    bind("T_page_scan_", &T_page_scan_);
    bind("Page_Scan_Type_", &Page_Scan_Type_);

    bind("ver_", &ver_);

    bind("pageTO_", &pageTO_);
    bind("backoffParam_", &backoffParam_);
    bind("backoffParam_s_", &backoffParam_small_);
    bind("SR_mode_", &SR_mode_);
    bind("N_page_", &N_page_);
    bind("N_inquiry_", &N_inquiry_);

    bind("driftType_", &driftType_);
    bind("clkdrift_", &clkdrift_);

    bind("useDynamicTpoll_", &useDynamicTpoll_);
    bind("pollReserveClass_", &pollReserveClass_);

/*
    bind("energyMin_", &energyRec_.energyMin_);
    bind("energy_", &energyRec_.energy_);
    bind("activeEnrgConRate_", &energyRec_.activeEnrgConRate_);
    bind("activeTime_", &energyRec_.activeTime_);
    bind("startTime_", &energyRec_.startTime_);
    bind("numTurnOn_", &energyRec_.numTurnOn_);
    bind("warmUpTime_", &energyRec_.warmUpTime_);
*/

    dXperTick_ = 0;		// speed = 0
    dYperTick_ = 0;

    ver_ = BTSPEC;

    useDynamicTpoll_ = 1;
    pollReserveClass_ = 0;

    _init();

    linkSched_ = new BTDRR(this);
}

void Baseband::_init()
{
    isMaster_ = 0;
    clk_ = 0;
    receiver_ = 0;
    clke_ = 0;
    clkf_ = 0;
    old_master_bd_addr_ = -1;

    // reset schedWord after sending, used by slaves
    reset_as_ = 1;

    inSleep_ = 0;
    trace_me_tx_ = 0;
    trace_me_rx_ = 0;
    trace_me_poll_ = 0;
    trace_me_null_ = 0;
    trace_me_in_air_ = 0;
    trace_me_stat_ = 0;

    started_ = 0;

    inRS_ = 0;

    activeBuffer_ = NULL;	// txBuffer in tranceiving.
    rxPkt_ = 0;

    afhEnabled_ = 0;
    Page_Scan_Type_ = Standard;
    Inquiry_Scan_Type_ = Standard;

    sched_word_ = 0;
    maxNumTxBuffer_ = 16;
    numTxBuffer_ = MinTxBufferSlot;

    drift_clk_ = 0;

    suspendClkn_ = 0;

    N_page_ = NPAGE;
    pageTO_ = PAGETO;
    SR_mode_ = 1;

    T_w_page_scan_ = TWPAGESCAN;
    T_page_scan_ = TPAGESCAN;
    connectable_ = 0;
    pscan_fs_clk_ = 0;
    iscan_fs_clk_ = 0;
    iscan_addr_ = GIAC;

    slave_rsps_count_incr_slot_ = 5;	// set false
    slave_rsps_count_ = 0;
    master_rsps_count_ = 0;

    scoState_ = SCO_IDLE;
    clearStateLock();

    pageRespTO_ = PAGERESPTO;
    newConnectionTO_ = NEWCONNECTIONTO;
    pageRespTimer_ = 0;

    T_w_inquiry_scan_ = TWINQUIRYSCAN;
    T_inquiry_scan_ = TINQUIRYSCAN;
    discoverable_ = 0;
    inBackoff_ = 0;
    pagescan_after_inqscan_ = 0;
    page_after_inq_ = 0;

    N_inquiry_ = NINQUIRY;
    inqRespTO_ = INQRESPTO;
    backoffParam_ = BACKOFF;
    backoffParam_small_ = BACKOFF_s;

    driftType_ = BT_DRIFT_OFF;
    clkdrift_ = 0;

    SlotTime = BTSlotTime;
    Tick = slotTime() / 2;

    state_ = stableState_ = STANDBY;

    lastRecvT_ = 0;

    polled_lt_addr_ = 0;
    poll_time_ = -1;

    _prioLevelReq = 0;

    sched_word_ = new BTSchedWord(2);	// set a default sched_word
    txBuffer_ = new TxBuffer *[maxNumTxBuffer_];
    memset(txBuffer_, 0, sizeof(TxBuffer *) * maxNumTxBuffer_);
    txBuffer_[BcastSlot] = new TxBuffer(this, 0, BcastSlot);
    txBuffer_[BeaconSlot] = new TxBuffer(this, 0, BeaconSlot);

    resyncWind_ = 625E-6;
    resyncWindSlotNum_ = 2;
    resyncCntr_ = 0;
    maxResync_ = 9;
    resyncWindStartT_ = -1;

    trx_st_ = TRX_OFF;

    t_clkn_00_ = 0;
    t_clk_00_ = 0;

    for (int i = 0; i < 6; i++) {
	lt_sco_table_[i] = 0;
    }
}

void Baseband::reset()
{
    for (int i = BcastSlot; i < maxNumTxBuffer_; i++) {
	if (txBuffer_[i]) {
	    delete txBuffer_[i];
	}
    }
    delete[]txBuffer_;
    txBuffer_ = NULL;
    _init();
}

void Baseband::on()
{
    double slotoffset = 0;
    if (!started_) {
	Scheduler::instance().schedule(&clkn_handler_, &clkn_ev_,
				       slotoffset);
	started_ = 1;
	clkn_ = (clkn_ & 0xfffffffc) + 3;
	t_clkn_00_ = clkn_ev_.time_ - slotTime() * 2;
    }
    // MAX_SLOT_DRIFT = 20E-6
    // BTSlotTime = 625E-6
    double maxdriftperslot = MAX_SLOT_DRIFT * BTSlotTime;
    if (driftType_ == BT_DRIFT) {
	SlotTime = Random::uniform(BTSlotTime - maxdriftperslot,
				   BTSlotTime + maxdriftperslot);
    } else if (driftType_ == BT_DRIFT_NORMAL) {
	SlotTime = Random::normal(BTSlotTime, BT_DRIFT_NORMAL_STD);
	while (slotTime() > BTSlotTime + maxdriftperslot ||
	       slotTime() < BTSlotTime - maxdriftperslot) {
	    SlotTime = Random::normal(BTSlotTime, BT_DRIFT_NORMAL_STD);
	}
    } else if (driftType_ == BT_DRIFT_USER) {
	SlotTime = BTSlotTime + clkdrift_ * 1E-6 * BTSlotTime;
	fprintf(stderr, "%d clkdrift_ : %d\n", bd_addr_, clkdrift_);
    } else {
	SlotTime = BTSlotTime;
    }
    Tick = slotTime() / 2;
}

void Baseband::turn_on_tx()
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();

    if (trxoff_ev_.uid_ > 0) {
	s.cancel(&trxoff_ev_);
    }

    if (trx_st_ == TX_ON) {
	return;
    }

    DUMP_TRX_ST;

    // if (trx_st_ != RX_ON) {
    if (trx_st_ == TRX_OFF) {
	energyRec_.trxTurnOnTime_ = now;
	energyRec_.numTurnOn_++;
    }
    trx_st_ = TX_ON;
}

void Baseband::turn_on_rx()
{
    Scheduler & s = Scheduler::instance();
    double now = s.clock();

    if (trxoff_ev_.uid_ > 0) {
	s.cancel(&trxoff_ev_);
    }

    if (trx_st_ == RX_ON) {
	return;
    }

    if (trx_st_ == RX_RECV) {
	fprintf(stderr, "**** %d %f : turn on RX when receving ...\n",
		bd_addr_, s.clock());
    }

    DUMP_TRX_ST;

    if (trx_st_ == TRX_OFF) {
	energyRec_.trxTurnOnTime_ = now;
	energyRec_.numTurnOn_++;
    }
    trx_st_ = RX_ON;
    // fprintf(BtStat::log_, "%d (%s) turn_on_rx: RX_ON\n", bd_addr_, state_str());
}

void Baseband::turn_off_rx_to()
{
    double now = Scheduler::instance().clock();
    if (trx_st_ == TRX_OFF) {
	return;
    }

    DUMP_TRX_ST;

    if (energyRec_.trxTurnOnTime_ < 0) {
	fprintf(stderr, "%d %f %s turn off before started.\n",
		bd_addr_, now, __FUNCTION__);
    } else if (trx_st_ == TX_ON || trx_st_ == RX_ON || trx_st_ == RX_RECV) {
	energyRec_.energy_ -=
	    (now - energyRec_.trxTurnOnTime_ +
	     energyRec_.warmUpTime_) * energyRec_.activeEnrgConRate_;
	energyRec_.activeTime_ += (now - energyRec_.trxTurnOnTime_);
	if (energyRec_.energy_ < energyRec_.energyMin_) {
	    off();
	}
    }
    energyRec_.trxTurnOnTime_ = -1;
    trx_st_ = TRX_OFF;
}

void Baseband::turn_off_trx()
{
/*
    if (trxoff_ev_.st != 0) {
	fprintf(stderr, "TRX_off: event:%d is not cleared.\n", trxoff_ev_.st);
    }
*/
    double now = Scheduler::instance().clock();

    if (trx_st_ == TRX_OFF) {
	return;
    }

    DUMP_TRX_ST;

    if (energyRec_.trxTurnOnTime_ < 0) {
	fprintf(stderr, "%d %f %s turn off before started.\n",
		bd_addr_, now, __FUNCTION__);
    } else if (trx_st_ == TX_ON || trx_st_ == RX_ON || trx_st_ == RX_RECV) {
	energyRec_.energy_ -=
	    (now - energyRec_.trxTurnOnTime_) * energyRec_.activeEnrgConRate_;
	energyRec_.activeTime_ += (now - energyRec_.trxTurnOnTime_);
	if (energyRec_.energy_ < energyRec_.energyMin_) {
	    off();
	}
    }
    energyRec_.trxTurnOnTime_ = -1;
    trx_st_ = TRX_OFF;
}

void Baseband::turn_off_trx(BTTRXoffEvent * e)
{
#if 0
    if (e->st != trx_st_) {
	fprintf(stderr, "TRX_st doesn't match: cur:%d turnoff:%d\n",
		trx_st_, e->st);
    }
#endif

    DUMP_TRX_ST1;

    turn_off_trx();
    e->clearSt();
}

void Baseband::energyReset()
{
    energyRec_.startTime_ = Scheduler::instance().clock();
    energyRec_.energy_ = 1;
    energyRec_.activeTime_ = 0;
    energyRec_.numTurnOn_ = 0;

    for (int i = MinTxBufferSlot; i < maxNumTxBuffer_; i++) {
	if (txBuffer_[i]) {
	    txBuffer_[i]->ttlSlots_ = 0;
	}
    }
}

void Baseband::dumpTtlSlots(FILE * f)
{
    if (!f) {
	f = stdout;
    }
    int ad = bd_addr_;

	/** Commented by Barun [07 March 2013]
    fprintf(f, "ttlSlots %d: ", ad);
	*/
    for (int i = MinTxBufferSlot; i < maxNumTxBuffer_; i++) {
	if (txBuffer_[i]) {
	/** Commented by Barun [07 March 2013]
	    fprintf(f, " %d", txBuffer_[i]->ttlSlots_);
	*/
	}
    }
    fprintf(f, "\n");
}

void Baseband::dumpEnergy(FILE * f)
{
    Scheduler & s = Scheduler::instance();
    if (!f) {
	f = stdout;
    }

    double tt = s.clock() - energyRec_.startTime_;
    int ad = bd_addr_;

	/** Commented by Barun [07 March 2013]
    fprintf(f, "E %d tt: %f at: %f %f e: %f n: %d \n",
	    ad, tt, energyRec_.activeTime_, (energyRec_.activeTime_ / tt),
	    energyRec_.energy_, energyRec_.numTurnOn_);
	*/
}

void Baseband::off()
{
    Scheduler &s = Scheduler::instance();

    turn_off_trx();
    if (clk_ev_.uid_ > 0) {
	s.cancel(&clk_ev_);
    }
    s.cancel(&clkn_ev_);
}

int Baseband::command(int argc, const char *const *argv)
{
    if (argc == 3) {
	if (!strcmp(argv[1], "test-fh")) {
	    bd_addr_t addr = strtol(argv[2], NULL, 0);
	    test_fh(addr);
	    return TCL_OK;
	}
    } else if (argc == 5) {
	if (!strcmp(argv[1], "analysis")) {
	    bd_addr_t addr = strtol(argv[2], NULL, 0);
	    int clk = atoi(argv[3]);
	    int len = atoi(argv[4]);
	    signed char *buf =
		seq_analysis(addr, FHChannel, clk, 0, len, 2);
	    delete[]buf;
	    return TCL_OK;
	}
    }
    return BiConnector::command(argc, argv);
}

// Used by PRR. So you can specify prio for a specific link even if it
// has not been formed yet.
void Baseband::set_prio(bd_addr_t remote, int prio)
{
    int i;
    for (i = MinTxBufferSlot; i < maxNumTxBuffer_; i++) {
	if (txBuffer_[i] && txBuffer_[i]->remote_bd_addr_ == remote &&
	    (txBuffer_[i]->type() == ACL)) {
	    txBuffer_[i]->prio_ = prio;
	    return;
	}
    }

    // txBuffer for remote doesn't exist yet, put the request in queue.
    _prioLevelReq = new PrioLevelReq(remote, prio, _prioLevelReq);
}

// When a new txBuffer is created, apply prio setting if a prioLevelReq exists.
void Baseband::_try_to_set_prio(TxBuffer * txBuffer)
{
    if (!_prioLevelReq) {
	return;
    }
    PrioLevelReq *par = _prioLevelReq;	// parent (of wk) pointer
    if (_prioLevelReq->addr == txBuffer->remote_bd_addr_) {
	txBuffer->prio_ = _prioLevelReq->prio;
	_prioLevelReq = _prioLevelReq->next;
	delete par;
	return;
    }
    PrioLevelReq *wk = _prioLevelReq->next;
    while (wk) {
	if (wk->addr == txBuffer->remote_bd_addr_) {
	    txBuffer->prio_ = wk->prio;
	    par->next = wk->next;
	    delete wk;
	    return;
	}
	par = wk;
	wk = wk->next;
    }
}

TxBuffer *Baseband::allocateTxBuffer(LMPLink * link)
{
    int slot;

    // Locate a free slot.  If no, double the buffer size.
    if (++numTxBuffer_ == maxNumTxBuffer_) {
	slot = maxNumTxBuffer_;
	maxNumTxBuffer_ += maxNumTxBuffer_;
	TxBuffer **ntx = new TxBuffer *[maxNumTxBuffer_];
	memcpy(ntx, txBuffer_, sizeof(TxBuffer *) * slot);
	memset(&ntx[slot], 0, sizeof(TxBuffer *) * slot);
	delete[]txBuffer_;
	txBuffer_ = ntx;
    } else {
	for (int i = MinTxBufferSlot; i < maxNumTxBuffer_; i++) {
	    if (txBuffer_[i] == NULL) {
		slot = i;
		break;
	    }
	}
    }

    txBuffer_[slot] = new TxBuffer(this, link, slot);
    if (_prioLevelReq && txBuffer_[slot]->type() == ACL) {
	_try_to_set_prio(txBuffer_[slot]);
    }

    // suggested by Guanhua Yan <ghyan@lanl.gov> for supervision purpose.
    txBuffer_[slot]->lastPktRecvT_ = clk_;

    return txBuffer_[slot];
}

void Baseband::freeTxBuffer(TxBuffer * tx)
{
    int slot = tx->slot();
    txBuffer_[slot] = NULL;
    numTxBuffer_--;
    delete tx;
}


/*-----------------------------------------------------------*/
//  Sending and Receiving routines
/*-----------------------------------------------------------*/

void Baseband::sendBBSig(Packet * p)
{
    // BBsig packets are never kepted in txBuffer.
    HDR_BT(p)->nokeep = 1;
    sendDown(p);
}

// This function send packet to phy layer
void Baseband::sendDown(Packet * p, Handler * h)
{
    Scheduler & s = Scheduler::instance();
    hdr_cmn *ch = HDR_CMN(p);
    hdr_bt *bh = HDR_BT(p);
    ch->direction() = hdr_cmn::DOWN;
    ch->size() = bh->size / 8;
    bh->ts_ = s.clock();
    bh->sender = bd_addr_;

    activeBuffer_ = lookupTxBuffer(bh);

    turn_on_tx();
/*
    trxoff_ev_.st = trx_st_;
    s.schedule(&trxtoTimer_, &trxoff_ev_,
	       bh->txtime() + energyRec_.trxTurnarndTime_);
*/
    if (p->uid_ > 0) {
	s.cancel(p);
    }
    // s.schedule(&txHandler, p, BTDELAY);

    ch_->interfQ(bh->fs_).add(bd_addr_, node_->X_, node_->Y_, node_->Z_,
			      bh->ts_, bh->ts_ + bh->txtime());

    if ((trace_null() && bh->type == hdr_bt::Null) ||
	(trace_poll() && bh->type == hdr_bt::Poll) ||
	(trace_tx() && bh->type != hdr_bt::Null
	 && bh->type != hdr_bt::Poll)) {

	// Barun
	//bh->dump(BtStat::log_, BTTXPREFIX, bd_addr_, state_str_s());	// "t "
	if (bh->extinfo >= 10) {
	    bh->dump_sf();
	}
    }
    // Note, p is freed only right before next new pkt is transmitted, 
    // since p == current_ at this moment.  However, the packets not kept
    // in TxBuffer should be freed in tx_handle().

    if (bh->nokeep) {
	downtarget_->recv(p, h);
    } else {
	downtarget_->recv(p->copy(), h);
    }
}

int Baseband::tranceiveSlots(hdr_bt * bh)
{
    // TODO: need more exact calculation??
/*
    if (bh->type == hdr_bt::Id) {
	return 1;
    }
*/
#if 0
    if (bh->size > 1626) {	// DM3/DH3 1626/1622
	return 10;		// 5 slots
    } else if (bh->size > 366) {
	return 6;		// 3 slots
    } else if (bh->size >= 126) {
	return 2;		// 1 slot
    } else {
	return 1;		// 1/2 slot
    }
#endif

    return (bh->type == hdr_bt::Id ? 1 : hdr_bt::slot_num(bh->type) * 2);
}

// put time stamp on the packet
Packet *Baseband::stamp(Packet * p, TxBuffer * tx)
{
    hdr_bt *bh = HDR_BT(p);
    bh->lt_addr_ = tx->link()->lt_addr_;
    bh->clk = clk_;
    bh->ac = master_bd_addr_;
    bh->srcTxSlot = tx->slot();
    bh->dstTxSlot = tx->dstTxSlot();
    bh->sender = bd_addr_;
    bh->transmitCount++;
    bh->X_ = node_->X();
    bh->Y_ = node_->Y();
    bh->Z_ = node_->Z();
    bh->nokeep = 0;

    if (isMaster()) {
	FH_sequence_type fhs = (tx->afhEnabled_ ? FHAFH : FHChannel);
	bh->fs_ = FH_kernel(clk_, clkf_, fhs, master_bd_addr_);

	if (tx->afhEnabled_) {
	    afhEnabled_ = 1;
	    transmit_fs_ = bh->fs_;	// Used for same channel mechanism.
	} else {
	    afhEnabled_ = 0;
	}
    } else if (tx->afhEnabled_) {
	bh->fs_ = transmit_fs_;
    } else {
	bh->fs_ = FH_kernel(clk_, clkf_, FHChannel, master_bd_addr_);
    }

    return p;
}

// Guanhua Yan made very good suggestions on add this.
void Baseband::standBy()
{
    clearConnectState();
    if (state() != STANDBY) {
	change_state(STANDBY);
    }

    if (lmp_->scanWhenOn_) {
	lmp_->HCI_Write_Scan_Enable(0x03);
    }
}

// When upper layer removed all LMPLink, the baseband should
// enter STANDBY state.  However, ongoing page/inq/scan should not
// be interrupted.
void Baseband::clearConnectState()
{
    if (clk_ev_.uid_ > 0) {
	Scheduler::instance().cancel(&clk_ev_);
    }
    if (state() == Baseband::CONNECTION) {
	change_state(STANDBY);
    }
    stableState_ = STANDBY; 
}

int Baseband::isBusy()
{
#if 0
    return state() == PAGE ||
	state() == PAGE_SCAN ||
	state() == NEW_CONNECTION_MASTER ||
	state() == NEW_CONNECTION_SLAVE ||
	state() == ROLE_SWITCH_MASTER ||
	state() == RS_NEW_CONNECTION_MASTER ||
	state() == MASTER_RESP ||
	state() == SLAVE_RESP ||
	state() == INQUIRY_RESP || state() == INQUIRY;
#endif
    // return state() != STANDBY && state() != CONNECTION;

    // It is too strong in case that scan window is small.
    // That scan and connect interlaced.
    return discoverable_ || connectable_ || inRS_ ||
	(state() != STANDBY && state() != CONNECTION);
}

void Baseband::setPiconetParam(Piconet * pico)
{
    isMaster_ = pico->isMaster();
    if (state() != CONNECTION) {
	change_state(CONNECTION);
    }
    master_bd_addr_ = pico->master_bd_addr_;
    pico->compute_sched();
    set_sched_word(pico->_sched_word);
    if (trace_state()) {
	pico->_sched_word->dump();
    }

    if (isMaster_) {
	polling_ = 1;
	reset_as_ = 0;
	linkSched_->init();
	// fprintf(stdout, "* %d is master\n", bd_addr_);
    } else {
	polling_ = 0;
	reset_as_ = 1;
	lt_addr_ = pico->activeLink->lt_addr_;
	// fprintf(stdout, "* %d is slave\n", bd_addr_);
    }
    lastRecvT_ = 0;
    // rxPkt_ = 0;
    txSlot_ = 0;
    scoState_ = SCO_IDLE;
    setScoLTtable(pico->sco_table);
}

// doesn't consider clk wrap round
int Baseband::sched(int clk, BTSchedWord * sched)
{
    int ret = sched->word[(clk >> 1) % sched->len];

    // a txbuffer maybe just freed.
    if (ret >= MinTxBufferSlot && !txBuffer_[ret]) {
	ret = DynamicSlot;
    }

    if (ret < ReserveSlot || ret == RecvSlot ||
	(ret >= MinTxBufferSlot && txBuffer_[ret]->type() == SCO)) {
	return ret;
    }
    // Slave's SchedWord defauts to receive only.  Upone polled, the 
    // following slot is set to send. After sending, that slot is set
    // to receive again.
    if (ret >= MinTxBufferSlot &&
	txBuffer_[ret]->type() == ACL &&
	(reset_as_ || txBuffer_[ret]->link()->reset_as_)) {
	sched->word[(clk >> 1) % sched->len] = RecvSlot;
	return ret;
    }
    // Check if any Bcast pkt pending.  Send it first.  We don't consider
    // the case where bcast overload the network.  The principle is,
    // bcast is sent only necessary, so, deliver them first.
    if (txBuffer_[BcastSlot] && txBuffer_[BcastSlot]->hasBcast()) {
	return BcastSlot;
    }
    // BeaconSlot should be handled the same way if PARK is supported.

    if (!isMaster()) {
	return ret;
    }
    // Master schedule the sending by different algorithms.
    if (!lmp_->curPico) {
	if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1 "%d oops, curPico = null at bb sched().\n",
		    bd_addr_);
	*/
	}
	return RecvSlot;
    } else if (lmp_->curPico->numActiveLink == 0) {
	return RecvSlot;
    }
    return linkSched_->sched(clk, sched, ret);
}

// Letting slave send.
void Baseband::polled(int slot)
{
    sched_word_->word[((polling_clk_ >> 1)) % sched_word_->len] = slot;
}

void Baseband::change_state(BBState st)
{
    const char *ps = state_str();
    int turnonrx = 0;

    // reset pageRespTimer_;
    pageRespTimer_ = 0;

    switch (st) {
    case INQUIRY_SCAN:
	T_w_inquiry_scan_timer_ = T_w_inquiry_scan_;
	iscan_fs_clk_ = -1;

	// interlaced scan
	if (Inquiry_Scan_Type_ != Standard) {
	    Inquiry_Scan_Type_ = Interlaced;
	    if (T_w_inquiry_scan_ + T_w_inquiry_scan_ <= T_inquiry_scan_) {
		T_w_inquiry_scan_timer_ += T_w_inquiry_scan_;
	    }
	}
	break;

    case PAGE_SCAN:
	T_w_page_scan_timer_ = T_w_page_scan_;
	pscan_fs_clk_ = -1;

	// interlaced scan
	if (Page_Scan_Type_ != Standard) {
	    Page_Scan_Type_ = Interlaced;
	    if (T_w_page_scan_ + T_w_page_scan_ <= T_page_scan_) {
		T_w_page_scan_timer_ += T_w_page_scan_;
	    }
	}
	break;

    case INQUIRY_RESP:
	inqRespTimer_ = inqRespTO_;
	break;
    case MASTER_RESP:
	pageRespTimer_ = pageRespTO_;
	break;
    case SLAVE_RESP:
	turnonrx = 1;
	pageRespTimer_ = pageRespTO_;
	break;
    case ROLE_SWITCH_MASTER:
    case ROLE_SWITCH_SLAVE:
	inRS_ = 1;
    case RS_NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_MASTER:
    case NEW_CONNECTION_SLAVE:
    case NEW_CONNECTION_MASTER:
	newConnectionTimer_ = newConnectionTO_;
	break;
    default:
	break;
    }

    if (turnonrx) {
	turn_on_rx();
    }

    state_ = st;
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		BTPREFIX1 "%d at %f CHANGE ST %s -> %s clkn:%d clk:%d\n",
		bd_addr_,
		Scheduler::instance().clock(), ps, state_str(),
		clkn_, clk_);
	*/
    }
}

// If a slave didn't hear a packet during 250ms, it should perform
// channel re-sychronization. This is done whenever a slave is waken up
// from a suspened mode such as SNIFF, HOLD and PARK.  The search window 
// should be increased to a proper value according to the time of losing 
// sync.  
void Baseband::enter_re_sync(double t)
{
    Scheduler & s = Scheduler::instance();

    // clk_ was set to the value of the beginning of next frame. 
    // It is so arranged in order to be the right value in the beginning of
    // next frame, remembering that it is increased by resyncWindSlotNum_ * 2
    // in the event handler handle_re_synchronize().
    //clk_ -= (resyncWindSlotNum_ * 2 - 2); 
    clk_ -= (resyncWindSlotNum_ * 2);
    resyncCntr_ = 0;

    change_state(RE_SYNC);
    if (resync_ev_.uid_ > 0) {
	s.cancel(&resync_ev_);
    }
    s.schedule(&resync_handler_, &resync_ev_, t);
}

// Event handler
void Baseband::handle_re_synchronize(Event * e)
{
    Scheduler & s = Scheduler::instance();
    if (++resyncCntr_ <= maxResync_) {
	s.schedule(&resync_handler_, e, slotTime() * resyncWindSlotNum_);
    }

    resyncWindStartT_ = s.clock();
#ifdef PRINTRESYN
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		"==== %d:%d resyncWindStartT_ = %f win:%f num:%d\n",
		bd_addr_, resyncCntr_, resyncWindStartT_, resyncWind_,
		resyncWindSlotNum_);
	*/
    }
#endif
    turn_on_rx();
    trxoff_ev_.st = trx_st_;
    s.schedule(&trxtoTimer_, &trxoff_ev_, resyncWind_);	// set up reSync window

    clk_ += (resyncWindSlotNum_ * 2);
}

// perform the re-synchronization.
void Baseband::re_sync()
{
    Scheduler & s = Scheduler::instance();
    s.cancel(&resync_ev_);
    change_state(CONNECTION);

    // Check timing
    t_clk_00_ = s.clock();
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		"%f sched clk at %f by re_syn()\n",
		s.clock(), s.clock() + slotTime() - MAX_SLOT_DRIFT / 2);
	*/
    }
    s.schedule(&clk_handler_, &clk_ev_, slotTime() - MAX_SLOT_DRIFT / 2);

    int clkoffset;
    int slotoffset;
    comp_clkoffset(MAX_SLOT_DRIFT / 2, &clkoffset, &slotoffset);
#ifdef PRINTRESYN
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		"==== %d:%d RE_SYN'd: %f - %f = %f w:%f n:%d\n", bd_addr_,
		resyncCntr_, s.clock(),
		resyncWindStartT_, (s.clock() - resyncWindStartT_),
		resyncWind_, resyncWindSlotNum_);
	*/
    }
#endif
    if (lmp_->curPico) {
	lmp_->curPico->clk_offset = clkoffset;
	lmp_->curPico->slot_offset = slotoffset;
	LMPLink *link = lmp_->curPico->activeLink;
	if (link && link->_sniff_ev && link->_sniff_ev->uid_ > 0) {
	    double t =
		link->_sniff_ev->time_ + (s.clock() - resyncWindStartT_);
	    s.cancel(link->_sniff_ev);
	    s.schedule(&lmp_->_timer, link->_sniff_ev, t - s.clock());
	    link->_lastSynClk = clkn_;
	}
	if (link && link->_sniff_ev_to && link->_sniff_ev_to->uid_ > 0) {
	    double t = link->_sniff_ev_to->time_ +
		(s.clock() - resyncWindStartT_);
	    s.cancel(link->_sniff_ev_to);
	    s.schedule(&lmp_->_timer, link->_sniff_ev_to, t - s.clock());
	}
    }
    s.cancel(&resync_ev_);
}

// for transmitting in connection mode. The master sets clk_ = clkn_
// for master, this event is 1 usec later than clkn event, so clkn_
// is always updated before clk_ is.
void Baseband::handle_clk(Event * e)
{
    Scheduler & s = Scheduler::instance();
    BBState st = state();

    int curSlot;
    Packet *p;
    TxBuffer *txb;
    int turnonrx = 1;

    if ((clk_ & 0x01)) {
	fprintf(stderr, "OOOOps, clk_0 is not 0\n");
	clk_++;
	s.schedule(&clk_handler_, e, tick());
	return;
    }
    // doesn't consider wrap around. This happens once a day, though
    clk_++;
    clk_++;

    if ((clk_ & 0x03) == 0) {
	t_clk_00_ = s.clock();
    }
    // Check SCO slots
    // XXX should we abort a receiving to prepare the SCO t/rx ??
    // consider if it is receiving an page/inquiry response.
    if ((txb = lookupScoTxBuffer())) {

	if (isMaster()) {
	    if ((clk_ & 0x02)) {
		polled_lt_addr_ = txb->link()->lt_addr_;
		scoState_ = SCO_RECV;
	    } else {
		scoState_ = SCO_SEND;
	    }

	} else {
	    scoState_ = ((clk_ & 0x02) ? SCO_SEND : SCO_RECV);
	}

	if (scoState_ == SCO_SEND) {
	    turnonrx = 0;
	    if ((p = txb->transmit()) != NULL) {
		sendDown(p, 0);
	    }
	}
	// fprintf(stdout, "%d clk :%d\n", bd_addr_, clk_);

	goto update;

    } else {

	scoState_ = SCO_IDLE;

	if (trx_st_ != TRX_OFF && trx_st_ != RX_ON) {
	    s.schedule(&clk_handler_, e, slotTime());
	    return;
	}
    }

/*
    if (trx_st_ != TRX_OFF) {
	s.schedule(&clk_handler_, e, slotTime());
	return;
    }
*/

    // consider drift ??
    if (stateLocked()) {
	s.schedule(&clk_handler_, e, slotTime());
	return;
    }

    switch (st) {
    case NEW_CONNECTION_SLAVE:
	--newConnectionTimer_;
	if (--newConnectionTimer_ <= 0) {
	    change_state(PAGE_SCAN);
	    connectable_ = 1;
	    s.cancel(&clk_ev_);
	    turn_on_rx();
	    return;
	}
	// we still use _slave_send() to send response at this phrase.
	break;

    case ROLE_SWITCH_MASTER:	// switch to Master Role from Slave Role
	--newConnectionTimer_;
	if (--newConnectionTimer_ <= 0) {
	    change_state(CONNECTION);
	    lmp_->role_switch_bb_complete(slave_, 0);	// Failed.
	    turn_on_rx_to();
	    return;
	} else if ((clk_ & 0x03) == 0
		   && sched(clk_, sched_word_) == BasebandSlot) {
	    p = genFHSPacket(FHChannel, master_bd_addr_,
			     clk_, bd_addr_, clkn_, slave_lt_addr_,
			     slave_);

	    HDR_BT(p)->comment("F");
	    sendBBSig(p);
	    if (trace_state()) {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_, BTPREFIX1 "master send clk: %d\n",
			clkn_);
		*/
	    }
	} else if ((clk_ & 0x02)) {
	    turn_on_rx_to();
	}
	turnonrx = 0;
	break;
    case ROLE_SWITCH_SLAVE:	// Switch to Slave Role
	--newConnectionTimer_;
	if (--newConnectionTimer_ <= 0) {
	    change_state(CONNECTION);
	    lmp_->role_switch_bb_complete(slave_, 0);	// Failed.
	    return;
	}
	break;
    case RS_NEW_CONNECTION_MASTER:
	--newConnectionTimer_;
	if (--newConnectionTimer_ <= 0) {
	    change_state(CONNECTION);
	    lmp_->role_switch_bb_complete(slave_, 0);	// Failed.
	    turn_on_rx_to();
	    return;
	} else if ((clk_ & 0x01) == 0
		   && sched(clkn_, sched_word_) == BasebandSlot) {
	    p = genPollPacket(FHChannel, master_bd_addr_, clk_, slave_);
	    HDR_BT(p)->lt_addr_ = slave_lt_addr_;
	    HDR_BT(p)->comment("P");
	    sendBBSig(p);
	    turnonrx = 0;
	}
	break;
    case RS_NEW_CONNECTION_SLAVE:
	--newConnectionTimer_;
	if (--newConnectionTimer_ <= 0) {
	    change_state(CONNECTION);
	    lmp_->role_switch_bb_complete(slave_, 0);	// Failed.
	    turn_on_rx_to();
	    return;
	}
	break;
#ifdef PARK_STATE
    case UNPARK_SLAVE:
	if ((clk_ & 0x02)) {
	    // FIXME: reset access_request_slot_, POLL
	    access_request_slot_++;
	    access_request_slot_++;
	    if (ar_addr_ == access_request_slot_) {
		p = genIdPacket(FHChannel, master_bd_addr_, clk_,
				master_bd_addr_);
		sendBBSig(p);	// send in the first half slot
		turnonrx = 0;
	    } else if (ar_addr_ == access_request_slot_ + 1) {
		p = genIdPacket(FHChannel, master_bd_addr_, clk_,
				master_bd_addr_);
		// I need to send at the second half slot.
		// Yes. Both packets at the same slot have the same freq.
		s.schedule(&slaveSendnonConnHandler_, p, tick());
		turnonrx = 0;
	    }
	}
	break;
#endif

    case CONNECTION:

	curSlot = sched(clk_, sched_word_);

	if (curSlot >= BcastSlot) {
	    if ((p = txBuffer_[curSlot]->transmit()) != NULL) {
		txSlot_ = curSlot;
		txClk_ = clk_;
		hdr_bt *bh = HDR_BT(p);
		sendDown(p, 0);
		if (isMaster()) {
		    txBuffer_[txSlot_]->txType_ = bh->type;
		    txBuffer_[txSlot_]->deficit_ -=
			hdr_bt::slot_num(bh->type);
		    txBuffer_[txSlot_]->ttlSlots_ +=
			hdr_bt::slot_num(bh->type);
		}
	    }
	    turnonrx = 0;
	}
#if 1
	if (isMaster()) {	// XXX: this violate schedword mechnism.
	    if ((clk_ & 0x02) == 0) {
		turnonrx = 0;
	    }
	} else {
	    if ((clk_ & 0x02) == 2) {
		turnonrx = 0;
	    }
	}
#endif
	break;
    default:
	curSlot = sched(clk_, sched_word_);	// SCO
	if (curSlot >= MinTxBufferSlot
	    && txBuffer_[curSlot]->type() == SCO) {
	    if ((p = txBuffer_[curSlot]->transmit()) != NULL) {
		sendDown(p, 0);
	    }
	    turnonrx = 0;
	}
#if 1
	if (isMaster()) {	// XXX: this violate schedword mechnism.
	    if ((clk_ & 0x02) == 0) {
		turnonrx = 0;
	    }
	} else {
	    if ((clk_ & 0x02) == 2) {
		turnonrx = 0;
	    }
	}
#endif
    }

  update:
    if (turnonrx) {
	turn_on_rx_to();
    }
    // TODO: fine tune about t_clk_00_
    if (isMaster() || driftType_ == BT_DRIFT_OFF || (clk_ & 0x10) == 0) {
	s.schedule(&clk_handler_, e, slotTime());
    } else {
	// fprintf(stderr, "%d drift_clk_: %f\n", bd_addr_, drift_clk_ * 1E6);
	s.schedule(&clk_handler_, e, slotTime() + drift_clk_);
	drift_clk_ = 0;
    }
}

void Baseband::setdest(double destx, double desty, double destz, double speed)
{
    double d =
	sqrt((destx - node_->X_) * (destx - node_->X_) +
	     (desty - node_->Y_) * (desty - node_->Y_) +
	     (destz - node_->Z_) * (destz - node_->Z_));
    if (d > 1E-10) {
	double dX_ = speed * (destx - node_->X_) / d;
	double dY_ = speed * (desty - node_->Y_) / d;
	double dZ_ = speed * (destz - node_->Z_) / d;
	dXperTick_ = dX_ * tick();
	dYperTick_ = dY_ * tick();
	dZperTick_ = dZ_ * tick();
    } else {
	dXperTick_ = dYperTick_ = dZperTick_ = 0.0;
    }
}

int Baseband::wakeupClkn()
{
    Scheduler & s = Scheduler::instance();
    double sleept = s.clock() - t_clkn_suspend_;
    int nt = int (sleept / tick());
    clkn_ = clkn_suspend_ + nt;
    int clkndiff = clkn_ - (clkn_suspend_ & 0xfffffffc);
    t_clkn_00_ += (clkndiff / 4) * slotTime() * 2;

    double nextclkt = t_clkn_suspend_ + tick() * nt + tick();
    double offsetinframe = s.clock() - t_clkn_00_;

    // There may have round up error, so compared to 5 tick()
    if (offsetinframe >= slotTime() * 2) {
	t_clkn_00_ += slotTime() * 2;
    }
    double t = nextclkt - s.clock();
    if (t <= 0) {
	t = tick();
    }

    node_->X_ = X_suspend_ + dXperTick_ * nt;
    node_->Y_ = Y_suspend_ + dYperTick_ * nt;
    node_->Z_ = Z_suspend_ + dZperTick_ * nt;
    s.cancel(&clkn_ev_);
    s.schedule(&clkn_handler_, &clkn_ev_, t);
    inSleep_ = 0;

    return 0;
}

int Baseband::trySuspendClknSucc()
{
    if (lmp_->curPico) {
	return 0;
    }
    // check the earliest wait up time

    Scheduler & s = Scheduler::instance();
    double wakeupt = 1E10;

    Piconet *pico = lmp_->suspendPico;
    LMPLink *link;
    do {
	if (pico->activeLink) {
	/** Commented by Barun [07 March 2013]
	    fprintf(stderr, "%d act link in suspnedPico.\n", bd_addr_);
	*/
	    return 0;
	}
	link = pico->suspendLink;
	do {
	    if (!link->_in_sniff) {
		return 0;
	    }
	    if (link->_sniff_ev->time_ < wakeupt) {
		wakeupt = link->_sniff_ev->time_;
	    }
	} while ((link = link->next) != pico->suspendLink);
    } while ((pico = pico->next) != lmp_->suspendPico);

    int dist = int ((wakeupt - s.clock()) / tick()) - 12;

    if (dist <= 0) {
	return 0;
    }

    X_suspend_ = node_->X_;
    Y_suspend_ = node_->Y_;
    Z_suspend_ = node_->Z_;
    node_->X_ += dXperTick_ * dist;
    node_->Y_ += dYperTick_ * dist;
    node_->Z_ += dZperTick_ * dist;

    // clkn_--;
    clkn_suspend_ = clkn_;
    t_clkn_suspend_ = s.clock();
    clkn_ += (dist - 1);
    s.schedule(&clkn_handler_, &clkn_ev_, dist * tick());
    if (trace_state()) {
	fprintf(BtStat::log_, "%d %s cancel clk\n", bd_addr_,
		__FUNCTION__);
    }
    s.cancel(&clk_ev_);
    // fprintf(stderr, "%d Suspend clkn for %d ticks.\n", bd_addr_, dist);
    return 1;
}

// handle baseband events
//
// can handle page scan and inquiry scan co-exists. Need some tests.
void Baseband::handle_clkn(Event * e)
{
    Scheduler & s = Scheduler::instance();

    Packet *p;
    hdr_bt *bh;

    // doesn't consider wrap around. This happens once a day, though
    ++clkn_;
    ++clke_;			// meaningfull only if being set.

    // update position
    node_->X_ += dXperTick_;
    node_->Y_ += dYperTick_;

    if ((clkn_ & 0x03) == 0) {
	t_clkn_00_ = s.clock();

	// Optimization: if the device are going to sleep for a long time,
	// the clock can be turn off temporarily.
	if (suspendClkn_) {
	    suspendClkn_ = 0;
	    if (trySuspendClknSucc()) {
		inSleep_ = 1;
		return;
	    }
	}
    }
    inSleep_ = 0;

    // specs says slave_rsps_count_ should increase when clkn_10 turns to 0.
    // This is a bug. Since a turn around message may only take
    // 1.5 slots of time to back when 'slave_rsps_count_' still not increased.
    if ((clkn_ & 0x03) == slave_rsps_count_incr_slot_) {
	slave_rsps_count_++;
    }
    // meaningfull only if clke_ being set.
    if ((clke_ & 0x03) == 0) {
	master_rsps_count_++;
    }

    if (trx_st_ != TRX_OFF && trx_st_ != RX_ON) {
	s.schedule(&clkn_handler_, e, tick());
	return;
    }

    if (stateLocked()) {
	s.schedule(&clkn_handler_, e, tick());
	return;
    }
    // Check InqScan first.  Give it preferrence over PageScan if both Requests
    // are issued at the same time.
    if (discoverable_) {
	if (--T_inquiry_scan_timer_ <= 0) {
	    T_inquiry_scan_timer_ = T_inquiry_scan_;

	    if (state() == PAGE_SCAN) {
		// Page Scan in progress, wait ...
		// This makes scan interval a little larger.
		// But should not be a problem.
		T_inquiry_scan_timer_ = T_w_page_scan_timer_;

	    } else if ((!inBackoff_)
		       && (state() != INQUIRY_RESP)) {
		// if in INQUIRY_RESP, skip INQUIRY_SCAN.     
		change_state(INQUIRY_SCAN);
	    }
	    // should we reset it ?? spec 1.2 says it is arbitrary chosen.
	    // inquiry_rsps_count_ = 0; 
	}
    }

    if (inBackoff_) {		// inquiry scan back off in progress
	if (--backoffTimer_ <= 0) {
	    inBackoff_ = 0;

	    if (state() == PAGE_SCAN) {
		// page scan in progress, sleep for a while
		inBackoff_ = 1;
		backoffTimer_ = T_w_page_scan_timer_;
	    } else {
		if (ver_ <= 11) {
		    change_state(INQUIRY_RESP);
		} else {
		    change_state(INQUIRY_SCAN);
		}
	    }
	}
    }
    // FIXME: need to handle special case: PSCAN after pageresp/newconn timeout
    if (connectable_) {
	// if (state() != SLAVE_RESP && state() != NEW_CONNECTION_SLAVE
	if (pageRespTimer_ <= 0 && state() != NEW_CONNECTION_SLAVE
	    && --T_page_scan_timer_ == 0) {
	    // if (--T_page_scan_timer_ == 0) {
	    T_page_scan_timer_ = T_page_scan_;
	    if (state() == INQUIRY_SCAN) {
		// delay until inq scan finish
		T_page_scan_timer_ = T_w_inquiry_scan_timer_;
	    } else if (state() == INQUIRY_RESP) {	// delay ...
		T_page_scan_timer_ = inqRespTimer_;
	    } else {
		change_state(PAGE_SCAN);
	    }
	}
    }

    switch (state()) {

    case PAGE_SCAN:
	if (!connectable_) {
	    fprintf(stderr,
		    "**OOps, PAGE_SCAN in non-connectable mode.\n");
	    break;
	}
	if (--T_w_page_scan_timer_ <= 0) {
	    trxoff_ev_.clearSt();
	    turn_off_trx();
	    change_state(stableState_);
	    break;

	    // interlaced scan
	} else if (T_w_page_scan_timer_ <= T_w_page_scan_
		   && Page_Scan_Type_ == Interlaced
		   && (T_w_page_scan_ * 2) <= T_page_scan_) {
	    Page_Scan_Type_ = InterlacedSecondPart;
	}
	if (trxIsOff()) {
	    turn_on_rx();
	}
	break;

    case SLAVE_RESP:
	if (--pageRespTimer_ <= 0) {
	    // change_state(PAGE_SCAN); // also set pageRespTimer_
	    connectable_ = 1;
	    T_page_scan_timer_ = 1;	// at next CLK, pscan will be performed.
	}
	// should do nothing ??
	// timing should be aligned to half or one slot
	if (trxIsOff()) {
	    turn_on_rx();
	}
	// turn_on_rx();                //XXX: what is the timing ref??
	// s.schedule(&_rxtoTimer, &_rxoff_ev, tick() + MAX_SLOT_DRIFT);

	break;

    case PAGE:

	if (--page_timer_ <= 0) {
	    change_state(stableState_);
	    lmp_->page_complete(slave_, 0);	// failed.
	    break;
	}
	// Change train only at frame boundary
	if (--page_train_switch_timer_ <= 0 && (clkn_ & 0x03) == 0) {
	    change_train();
	    page_train_switch_timer_ = N_page_ * 32;
	}
	if (sched(clkn_, sched_word_) == BasebandSlot) {
/*
	    // this should never happen as slave's response is ID pkt, 68us.
	    if (trx_st_ == RX_RECV) {
		break;
	    }
*/
	    p = genIdPacket(FHPage, slave_, clke_, slave_);
	    HDR_BT(p)->comment("P");
	    HDR_BT(p)->clk = clke_;
	    sendBBSig(p);
	} else {		// Check if I'm receiving a resp
	    turn_on_rx_to();
	}
	break;

    case MASTER_RESP:
	if (--pageRespTimer_ <= 0) {

	    // Spec: -- change to PAGE
#ifdef TRY_CONN_AFTER_PAGERESP_TO
	    newConnectionTimer_ = newConnectionTO_;
	    change_state(NEW_CONNECTION_MASTER);
	    clk_ = clkn_;
	    master_bd_addr_ = bd_addr_;
#else

	    change_state(PAGE);
#endif

	} else if ((clkn_ & 0x01) == 0
		   && sched(clkn_, sched_word_) == BasebandSlot) {
	    slave_lt_addr_ = lmp_->get_lt_addr(slave_);
	    p = genFHSPacket(FHMasterResp, slave_,
			     clke_, bd_addr_, clkn_, slave_lt_addr_,
			     slave_);
	    // HDR_BT(p)->u.fhs.clk = clkn_ & 0xFFFFFFFC;
	    HDR_BT(p)->comment("MR");
	    if (trace_state()) {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_, BTPREFIX1 "LT: %d\n",
			slave_lt_addr_);
		*/
	    }
	    HDR_BT(p)->clk = clke_;
	    sendBBSig(p);
	} else {
	    turn_on_rx_to();
	}
	break;

    case NEW_CONNECTION_MASTER:
	if (--newConnectionTimer_ <= 0) {
	    change_state(PAGE);
	} else if ((clkn_ & 0x01) == 0
		   && sched(clkn_, sched_word_) == BasebandSlot) {
	    p = genPollPacket(FHChannel, bd_addr_, clkn_, slave_);
	    HDR_BT(p)->lt_addr_ = slave_lt_addr_;
	    HDR_BT(p)->comment("NP");
	    sendBBSig(p);
	} else {
	    turn_on_rx_to();
	}
	break;

    case NEW_CONNECTION_SLAVE:
	// moved to clk_handler_;
	break;

    case INQUIRY_SCAN:
	if (!discoverable_) {
	    fprintf(stderr,
		    "**OOps, INQUIRY_SCAN in non-discovable mode.\n");
	    break;
	}
	if (--T_w_inquiry_scan_timer_ <= 0) {
	    trxoff_ev_.clearSt();
	    turn_off_trx();
	    change_state(stableState_);
	    break;

	    // interlaced scan
	} else if (T_w_inquiry_scan_timer_ <= T_w_inquiry_scan_
		   && Inquiry_Scan_Type_ == Interlaced
		   && (T_w_inquiry_scan_ * 2) <= T_inquiry_scan_) {
	    Inquiry_Scan_Type_ = InterlacedSecondPart;
	}
	if (trxIsOff()) {
	    turn_on_rx();
	}
	break;

    case INQUIRY_RESP:
	if (--inqRespTimer_ <= 0) {
	    // change_state(stableState_);
	    turn_off_trx();
	    discoverable_ = 1;
	    // change_state(INQUIRY_SCAN);
	    change_state(stableState_);
	} else {
	    if (trxIsOff()) {
		turn_on_rx();
	    }
	    // turn_on_rx();    // XXX
	}
	break;

    case INQUIRY:
	if (--inq_timer_ <= 0) {
	    change_state(stableState_);

	    // the following may change _state by lmp schuduler.
	    lmp_->inquiry_complete(inq_num_responses_);
	    break;
	}
	if (--inquiry_train_switch_timer_ <= 0 && (clkn_ & 0x03) == 0) {
	    change_train();
	    inquiry_train_switch_timer_ = N_inquiry_ * 32;
	}

	if (sched(clkn_, sched_word_) == BasebandSlot) {
	    if (trx_st_ == RX_RECV) {
		break;
	    }
	    p = genIdPacket(FHInquiry, inqAddr_, clkn_, BD_ADDR_BCAST);
	    bh = HDR_BT(p);
	    bh->comment("Q");
	    sendBBSig(p);
	} else {		// Check if I'm receiving a resp
	    turn_on_rx_to();
	}
	break;

    default:
	break;
    }
    s.schedule(&clkn_handler_, e, tick());
}

// Check if first bit of an incoming pkt is within the +-10us shredhold.
// Since we set delay as 10us.  We check to see if it is in [0,20us] range
// instead.  MAX_SLOT_DRIFT = 20us.
// offset is either a Slot or Half Slot.
int Baseband::recvTimingIsOk(double clk00, double offset, double *drift)
{
    *drift = Scheduler::instance().clock() - offset - clk00;

    if (*drift > MAX_SLOT_DRIFT || *drift < 0) {
#ifdef PRINT_RECV_TIMING_MISMATCH
	if (bd_addr_ == receiver_) {
	    if (trace_state()) {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_,
			BTPREFIX1 "%d recvTiming: t:%f t00:%f dr:%f \n",
			bd_addr_, Scheduler::instance().clock(), clk00,
			*drift);
		*/
	    }
	}
#endif
	*drift = 0;
	// *drift = *drift - BTDELAY;
	return 0;
    } else {
	*drift = *drift - BTDELAY;
	return 1;
    }
}

int Baseband::ltAddrIsValid(uchar ltaddr)
{
    return lmp_->curPico->ltAddrIsValid(ltaddr);
}

int Baseband::comp_clkoffset(double timi, int *clkoffset, int *sltoffset)
{
    Scheduler & s = Scheduler::instance();
    *clkoffset = (int) ((clk_ & 0xfffffffc) - (clkn_ & 0xfffffffc));
    *sltoffset =
	// (int) ((s.clock() - bh->txtime() - BTDELAY +
	(int) ((s.clock() - timi + BT_CLKN_CLK_DIFF - t_clkn_00_) * 1E6);
    if (*sltoffset < 0) {
	*sltoffset += 1250;
	*clkoffset += 4;
    }

    return 1;
}

// A real syn should be done by tuning the slave to a specific frequency,
// to sniff a master's packet.
int Baseband::synToChannelByGod(hdr_bt * bh)
{
    if (isMaster()) {
	return 0;
    }
    Scheduler & s = Scheduler::instance();
    int i;
    int clk = clk_;
    if (clk & 0x01) {
	clk--;
    }
    for (i = 0; i < SYNSLOTMAX; i++, i++) {
	if (FH_kernel(clk + i, 0, FHChannel, master_bd_addr_) == bh->fs_) {
	    clk = clk + i;
	    break;
	}
	if (FH_kernel(clk - i - 2, 0, FHChannel, master_bd_addr_) ==
	    bh->fs_) {
	    clk = clk - i - 2;
	    break;
	}
    }
    if (i == SYNSLOTMAX) {	// Correct clk is not found.
	return 0;
    }
#ifdef DUMPSYNBYGOD
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_, "%d-%d at %f %d SynToChByGod: clkdiff: %d ",
		bd_addr_, master_bd_addr_, s.clock(),
		(int) (s.clock() / BTSlotTime), clk - clk_);
	*/
    }
#endif
    clk_ = clk;
    if (clk_ev_.uid_ > 0) {
#ifdef DUMPSYNBYGOD
	double t =
	    slotTime() - (clk_ev_.time_ - s.clock()) - MAX_SLOT_DRIFT / 2;
	if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, " timing %f.\n", t);
	*/
	}
#endif
	fprintf(BtStat::log_, "%d %s cancel clk\n", bd_addr_,
		__FUNCTION__);
	s.cancel(&clk_ev_);
    }
    if (trace_state()) {
	fprintf(BtStat::log_, "%f sched clk at %f by %s\n",
		s.clock(),
		s.clock() + slotTime() - BTDELAY + BT_CLKN_CLK_DIFF,
		__FUNCTION__);
    }
    s.schedule(&clk_handler_, &clk_ev_,
	       slotTime() - BTDELAY + BT_CLKN_CLK_DIFF);
    double new_t_clk_00_ = s.clock() - BTDELAY + BT_CLKN_CLK_DIFF;
#ifdef DUMPSYNBYGOD
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_, " t_clk_00_ : %f -> %f, %f\n", t_clk_00_,
		new_t_clk_00_, new_t_clk_00_ - t_clk_00_);
	*/
    }
#endif
    t_clk_00_ = new_t_clk_00_;

    int clkoffset;
    int slotoffset;
    comp_clkoffset(0, &clkoffset, &slotoffset);
    if (lmp_->curPico) {
	lmp_->curPico->clk_offset = clkoffset;
	lmp_->curPico->slot_offset = slotoffset;
    }

    return 1;
}

bd_addr_t Baseband::getCurAccessCode()
{
    bd_addr_t ac;

    switch (state()) {

    case PAGE_SCAN:
    case SLAVE_RESP:
	ac = bd_addr_;
	break;
    case PAGE:
    case MASTER_RESP:
	ac = slave_;
	break;
    case NEW_CONNECTION_MASTER:
    case NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_MASTER:
    case ROLE_SWITCH_MASTER:
    case ROLE_SWITCH_SLAVE:
    case PARK_SLAVE:
    case UNPARK_SLAVE:
    case CONNECTION:
	ac = master_bd_addr_;
	break;
    case RE_SYNC:
	ac = master_bd_addr_;
	break;
    case INQUIRY_SCAN:
    case INQUIRY_RESP:
    case INQUIRY:
	ac = inqAddr_;
	break;

    default:
	ac = -1;
    }
    return ac;
}

// A channel has 3 elements: access code, frequency and slot timing
// This function is used to filter out mismatched packets.
int Baseband::channel_filter(Packet * p)
{
    hdr_bt *bh = HDR_BT(p);
    int clk;
    int addr;

    // check slot timing
    if (trx_st_ != RX_ON) {
	return 4;
    }

    if (getCurAccessCode() != bh->ac) {
	return 2;
    }

    if (getCurRxFreq(&clk, &addr) != bh->fs_) {
	return 3;
    }
    return 1;
}

char Baseband::getCurRxFreq(int *clk, int *fsaddr)
{
    // Scheduler & s = Scheduler::instance();
    // int clk;
    // bd_addr_t fsaddr;
    int clk_bak = -1;
    FH_sequence_type fstype;

    switch (state()) {

    case PAGE_SCAN:
	if (clkn_ > pscan_fs_clk_) {
	    computePScanFs();
	}
	*clk = (Page_Scan_Type_ == InterlacedSecondPart ?
		_interlaceClk(clkn_) : clkn_);
	*fsaddr = bd_addr_;
	return (Page_Scan_Type_ == InterlacedSecondPart ?
		pscan_interlaced_fs_ : pscan_fs_);

    case PAGE:
	*clk = clke_;
	fstype = FHPage;
	*fsaddr = slave_;
	break;

    case MASTER_RESP:
	// clk = clke_;  // specs says so. only clke_1 accounts. set to 2.
	clk_bak = clke_;
	*clk = 2;
	fstype = FHMasterResp;
	*fsaddr = slave_;
	break;

    case SLAVE_RESP:
	// clk = clkn_;  // specs says so. only clkn_1 accounts. Set to 0.
	*clk = 0;
	clk_bak = clkn_;
	fstype = FHSlaveResp;
	*fsaddr = bd_addr_;
	break;

    case CONNECTION:
	if (afhEnabled_) {
	    if (isMaster()) {
		return transmit_fs_;	// same channel mechanism
	    } else {
		fstype = FHAFH;
	    }
	} else {
	    fstype = FHChannel;
	}
	*clk = clk_;
	*fsaddr = master_bd_addr_;
	break;

    case NEW_CONNECTION_MASTER:
    case NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_MASTER:
    case ROLE_SWITCH_MASTER:
    case ROLE_SWITCH_SLAVE:
    case UNPARK_SLAVE:

	if (state() == NEW_CONNECTION_MASTER) {
	    *clk = clkn_;
	    t_clk_00_ = t_clkn_00_ + BT_CLKN_CLK_DIFF;
	} else {
	    *clk = clk_;
	}
	*fsaddr = master_bd_addr_;
	fstype = FHChannel;
	break;

    case RE_SYNC:
	*fsaddr = master_bd_addr_;
	fstype = FHChannel;
	*clk = clk_;
	break;

    case INQUIRY_SCAN:
    case INQUIRY_RESP:
	if (clkn_ > iscan_fs_clk_ || inqAddr_ != iscan_addr_ ||
	    iscan_N_ != inquiry_rsps_count_) {
	    computeIScanFs();
	}
	*fsaddr = inqAddr_;
	*clk = (Inquiry_Scan_Type_ == InterlacedSecondPart ?
		_interlaceClk(clkn_) : clkn_);
	return (Inquiry_Scan_Type_ == InterlacedSecondPart ?
		iscan_interlaced_fs_ : iscan_fs_);

    case INQUIRY:
	// FIXME: if slave a replies at first half slot and slave b replies
	//        at the second half.  The inquirer cannot get the second
	//        reply.  Moreover, if a slave replies at the second half
	//        slot.  The inquirer can not send the ID package at the
	//        first half slot of the following master slot.  Anyway,
	//        The impact should be minimal.
	*clk = clkn_;
	fstype = FHInquiry;
	*fsaddr = inqAddr_;
	break;

    default:
	// return 0;
	return -1;
    }

    char freq = FH_kernel(*clk, clkf_, fstype, *fsaddr);
    if (clk_bak >= 0) {
	*clk = clk_bak;
    }
    return freq;
}

int Baseband::packetType_filter(Packet * p)
{
    hdr_bt *bh = HDR_BT(p);

    switch (state()) {

    case PAGE_SCAN:
    case PAGE:
    case MASTER_RESP:
    case INQUIRY_SCAN:
    case INQUIRY_RESP:
	if (bh->type != hdr_bt::Id) {
	    return 0;
	}
	break;

    case SLAVE_RESP:
    case INQUIRY:
	if (bh->type != hdr_bt::FHS) {
	    return 0;
	}
	break;

	// FIXME: if slave a replies at first half slot and slave b replies
	//        at the second half.  The inquirer cannot get the second
	//        reply.  Moreover, if a slave replies at the second half
	//        slot.  The inquirer can not send the ID package at the
	//        first half slot of the following master slot.  Anyway,
	//        The impact should be minimal.

    default:
	return 1;
    }
    return 1;
}

int Baseband::handle_fs_mismatch(Packet * p, char fs, int clk,
				 int accesscode)
{
    Scheduler & s = Scheduler::instance();
    hdr_bt *bh = HDR_BT(p);
    if ((bh->receiver == bd_addr_ && state() != PAGE_SCAN &&
	 bh->type != hdr_bt::Id) ||
	(state() == RE_SYNC && bh->clk % 4 == 0) || (trace_inAir()
						     && (state() ==
							 INQUIRY_SCAN
							 || state() ==
							 PAGE_SCAN))) {
	bh->dump(BtStat::log_, 'x', bd_addr_, state_str_s());
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		" -- %f fs:%d(p)-%d clk:%d-%d ac:%d-%d clk00:%f %f\n",
		s.clock(), bh->fs_, fs,
		bh->clk, clk, bh->ac, accesscode, t_clk_00_, t_clkn_00_);
	*/
    }

    if (useSynByGod_ && !isMaster() && state() == CONNECTION
	&& bh->receiver == bd_addr_ && synToChannelByGod(bh)) {
	drift_clk_ = 0;
	// clk = clk_;
	return 1;
    } else {
	return 0;
    }

}

// This is a filter to filter out mismatching frequency and access code
// This is applied when first bit arrives.
//   returns:
//      0: fails to receive, ie. ac/fs mismatch
//      1: receiving whole packet
//      2: decoding hdr only
//      3: mismatch, donot set RX_OFF, eg. in page scan state.
//      4: RX not on
//      5: stateLocked
//      6: fs mismatch
int Baseband::recv_filter(Packet * p)
{
    Scheduler & s = Scheduler::instance();
    hdr_bt *bh = HDR_BT(p);
    int clk;
    bd_addr_t fsaddr;
    int fs;

    if (stateLocked()) {
	// return false;
	return 5;
    }

    if (trx_st_ != RX_ON) {
	// bh->dump(BtStat::log_, 'd', bd_addr_, state_str_s());
	// return false;
	return 4;
    }
    ////
    receiver_ = bh->receiver;
    // int fsMatched = 0;

    if (scoState_ == SCO_IDLE) {
	if ((fs = getCurRxFreq(&clk, &fsaddr)) != bh->fs_) {
	    if (!handle_fs_mismatch(p, fs, clk, fsaddr)) {
		return 6;
	    }
	}
	if (!packetType_filter(p)) {
	    set_trx_off(126E-6);
	    return 2;
	}
	if (state() == PAGE || state() == INQUIRY) {
	    if ((clkn_ & 0x02) == 0) {
		return 20;
	    }
	}

/*
	if (state() == NEW_CONNECTION_MASTER) {
	    t_clk_00_ = t_clkn_00_ + BT_CLKN_CLK_DIFF;
	}
*/
	// FIXME: if slave a replies at first half slot and slave b replies
	//        at the second half.  The inquirer cannot get the second
	//        reply.  Moreover, if a slave replies at the second half
	//        slot.  The inquirer can not send the ID package at the
	//        first half slot of the following master slot.  Anyway,
	//        The impact should be minimal.

    } else if (scoState_ == SCO_SEND) {
	// return 0;
	return 12;
    } else {
	if ((fs =
	     FH_kernel(clk_, 0, FHChannel, master_bd_addr_)) != bh->fs_) {
/*
	    if (!handle_fs_mismatch(p, fs, clk, fsaddr)) {
		return 6;
	    }
*/
	    return 6;
	}
    }

    // Check if it is addressed to me.
    // Note that master should process any packet it receives
    // Note Id packet does not have am field because it does not has a header
    // But it doesn't matter for simulation purpose since it's set to 0 always,
    // or takes as a broadcast pkt.
    if (isMaster()) {
	if (lmp_->curPico && state() == CONNECTION) {
#if 0
	    if (!ltAddrIsValid(bh->lt_addr_)) {
		// This is ok.  Maybe the link is just suspended at the moment.
		fprintf(BtStat::log_, "** %d recving invalid LT_ADDR, ",
			bd_addr_);
		bh->dump(BtStat::log_, BTERRPREFIX, bd_addr_,
			 state_str_s());
		// return 2;
		set_trx_off(126E-6);
		return false;
	    }
#endif
	    if (bh->lt_addr_ != polled_lt_addr_) {

		// An observable case here is when this device is a master
		// in connection state, while another device is trying to
		// page this device.  Occationally, the fs and tx timing match,
		// as ac already matches.

		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_, "** %d recving from slave not "
			"polled: %d, expect %d ",
			bd_addr_, bh->lt_addr_, polled_lt_addr_);
		*/
		bh->dump(BtStat::log_, BTERRPREFIX, bd_addr_,
			 state_str_s());
		set_trx_off(126E-6);
		// return false;
		return 2;
	    }
	}
    } else {
	if (state() == RE_SYNC) {
	    re_sync();
	} else if (state() == CONNECTION && drift_clk_ == 0) {
	    // Synchronize to the channel
	    double drift = s.clock() - t_clk_00_ - BTDELAY;
	    if (drift < -MAX_SLOT_DRIFT || drift > MAX_SLOT_DRIFT) {
		if (drift < BTSlotTime - MAX_SLOT_DRIFT ||
		    drift > BTSlotTime + MAX_SLOT_DRIFT) {
		/** Commented by Barun [07 March 2013]
		    fprintf(BtStat::log_,
			    "Werid drift %d :%f t_clk_00_: %f now: %f %d\n",
			    bd_addr_, drift, t_clk_00_, s.clock(), clk_);
		*/
		    // bh->dump(BtStat::log_, '-', bd_addr_, state_str_s());
		}
	    } else {
		drift_clk_ = -drift;
	    }
	}
	// Check if I'm the intended receiver
	if (bh->lt_addr_ != 0 && bh->lt_addr_ != lt_addr_) {
	    set_trx_off(126E-6);
	    // return false;
	    return 2;
	}
    }

#ifdef	FILTER_OUT_UNDETECTED_UNINTENED_RCVR
    // There is a case, where a staled Link/slave has the same lt_addr with
    // a newly join slave, and the old slave may receive packets addressed
    // to the newly joined slave.  Baseband shouldn't handle this.
    // However, at the moment, since LMP can't handle this, let's add a
    // artificial filter here
    if (state() == CONNECTION && bh->receiver != bd_addr_) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_, "** %d recv :not intened receiver.\n", bd_addr_);
	*/
	set_trx_off(126E-6);
	return 2;
    }
#endif

    handle_recv_1stbit(p);

    // set_trx_off(bh->txtime() + trxTurnarndTime_);
    trx_st_ = RX_RECV;
    trxoff_ev_.st = trx_st_;
    s.cancel(&trxoff_ev_);

    rxPkt_ = p;
    // return true;
    return 1;
}

// Some time critical thing needs to be done when the first bit arrives,
// such as freezing the clock in some cases.
void Baseband::handle_recv_1stbit(Packet * p)
{
    hdr_bt *bh = HDR_BT(p);
    int clk;
    activeBuffer_ = lookupTxBuffer(bh);

    polling_clk_ = clk_ + tranceiveSlots(bh);

    // we need to record some slave parameters at this moment
    // we generate slave reply package here.  Later on, if the received package
    // pass the collision and possible loss test, the reply is scheduled for
    // transmitting.  Then at transmitting time, we check if free slot
    // available for transmitting it.
    switch (state()) {
    case PAGE_SCAN:
	if (Page_Scan_Type_ == InterlacedSecondPart) {
	    clkf_ = _interlaceClk(clkn_);
	} else {
	    clkf_ = clkn_;
	}
	slave_rsps_count_ = 0;
	slave_rsps_count_incr_slot_ = (clkn_ + 3) & 0x03;
	bh->reply = genIdPacket(FHSlaveResp, bd_addr_, 2, bh->sender);
	HDR_BT(bh->reply)->comment("SR");
	HDR_BT(bh->reply)->nextState = SLAVE_RESP;
	break;

    case SLAVE_RESP:
	// Note:  upon receiving FHS packet from the master, the slave
	// reply with a ID package and enter CONNECTION state.  If the
	// replying ID package lost, the slave can't receive from the 
	// master any more until timeout to force them back to paging and
	// page scan states.  Anyway, ID package is supposed to be robust,
	// and it never gets lost in the effective range. 
	bh->reply = genIdPacket(FHSlaveResp, bd_addr_, 2, bh->sender);
	HDR_BT(bh->reply)->comment("FR");
	HDR_BT(bh->reply)->nextState = NEW_CONNECTION_SLAVE;
	break;

    case INQUIRY_SCAN:
	if (ver_ < 12) {
	    break;
	}
	// spec 1.2 : respond before back off.
	if (Inquiry_Scan_Type_ == InterlacedSecondPart) {
	    clk = _interlaceClk(clkn_);
	} else {
	    clk = clkn_;
	}
	// we are going to froze clkn_ for 1 slot duration.
	// clk += 2;
	bh->reply =
	    genFHSPacket(FHInqResp, inqAddr_, clk, bd_addr_, clkn_,
			 0, bh->sender);
	HDR_BT(bh->reply)->comment("QR");
	break;

    case INQUIRY_RESP:
	/*
	   Spec1.2+: 
	   1. receive an inquiry message.
	   2. respose an FHS pkt.
	   3. compute a back off window at [0, RNDMAX), RNDMAX = 127-1023.
	   begin back off
	   4. may enter Page Scan,
	   5. continue back off in STANDBY or CONNECTION state.
	   6. enter page scan

	   So, this substate last only a single slot if don't count
	   backoff period.
	   (vol2, p147).

	   Spec1.1:
	   The protocol in the slave’s inquiry response:
	   1. receive an inquiry message.
	   2. compute a back off window at [0, 1023) slots.
	   begin back off
	   3. may enter Page Scan, using mandatory page scan scheme.
	   4. continue back off in STANDBY or CONNECTION state.
	   5. enter inquiry response substate, up to inqRespTO_.
	   upon timeout, return to STANDBY or CONNECTION state.
	   6. receive an ID pkt and response 625 µs after. Then,
	   enter inquiry scan substate, with N++.

	   Spec claims that ``During a 1.28 s probing window, a slave on average
	   responses 4 times, but on different frequencies and at different
	   times. Possible SCO slots should have priority over response packets."
	   (vol1, p108)

	   What isn't clear to me is should it start a full InqScan windows?
	 */
	if (Inquiry_Scan_Type_ == InterlacedSecondPart) {
	    clk = _interlaceClk(clkn_);
	} else {
	    clk = clkn_;
	}
	// we are going to froze clkn_ for 1 slot duration.
	// clk += 2;
	bh->reply =
	    genFHSPacket(FHInqResp, inqAddr_, clk, bd_addr_, clkn_,
			 0, bh->sender);
	HDR_BT(bh->reply)->comment("QR");
	HDR_BT(bh->reply)->nextState = INQUIRY_SCAN;
	break;

    case NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_SLAVE:
	bh->reply = genNullPacket(FHChannel, master_bd_addr_,
				  clk_ + 2, master_bd_addr_);
	HDR_BT(bh->reply)->lt_addr_ = lt_addr_;
	HDR_BT(bh->reply)->comment("NR");
	HDR_BT(bh->reply)->nextState = INVALID;	// does't change
	break;

    case CONNECTION:
	if (afhEnabled_ && !isMaster()) {
	    transmit_fs_ = bh->fs_;	// same channel mechanism
	}
#if 0
	// check if receiving FHS to be taken over by new master
	if (bh->type != hdr_bt::FHS) {
	    break;
	}
#endif
	break;

    case ROLE_SWITCH_SLAVE:
	bh->reply = genIdPacket(FHChannel, master_bd_addr_,
				clk_ + 2, slave_);
	HDR_BT(bh->reply)->lt_addr_ = 0;
	HDR_BT(bh->reply)->comment("i");
	HDR_BT(bh->reply)->nextState = RS_NEW_CONNECTION_SLAVE;
	break;

    case PAGE:
	clkf_ = clke_;		// froze clock clkn16_12
	break;

    case NEW_CONNECTION_MASTER:
	clk_ = clkn_;
	t_clk_00_ = t_clkn_00_ + BT_CLKN_CLK_DIFF;
	break;
    default:
	break;
    }
}

//  slave send response in 6 cases:
//  send id in PAGE_SCAN -- slave response
//  send id in SLAVE_RESP -- reply to FHS
//  send fhs in INQUIRY_RESP
//  send null in NEW_CONNECTION_SLAVE
//  send id in ROLE_SWITCH_SLAVE
//  send null in RS_NEW_CONNECTION_SLAVE
//  special care need to take of the schduling problem, since the sched_word
//     was aligned to clkn_ .
void Baseband::_slave_reply(hdr_bt * bh)
{
    Scheduler & s = Scheduler::instance();
    Packet *p = bh->reply;

    if (!p) {
	// Barun
	//fprintf(BtStat::log_,
		//BTPREFIX1
		//"%d Baseband::_slave_reply(): reply with NULL pointer ",
		//bd_addr_);
	//bh->dump(BtStat::log_, BTRXPREFIX, bd_addr_, state_str_s());	// "r "
	return;
    }

    setStateLock();

    switch (state()) {

    case SLAVE_RESP:
	HDR_BT(p)->clk = clkn_;
	break;
    case PAGE_SCAN:
	HDR_BT(p)->clk = clkf_ + slave_rsps_count_;
	break;
    case ROLE_SWITCH_SLAVE:
    case CONNECTION:		// being taken over by a new master

    case INQUIRY_RESP:
    case NEW_CONNECTION_SLAVE:
    case RS_NEW_CONNECTION_SLAVE:
	break;

    default:
	fprintf(stderr, "OOOOps... %d slave sends in unknown state.\n",
		bd_addr_);
    }

    bh->reply = 0;
    // s.schedule(&slaveSendnonConnHandler_, p, slotTime() - BTDELAY - bh->txtime());
    s.schedule(&slaveSendnonConnHandler_, p, slotTime() - bh->txtime());
}

// slave send response packet.  The timing is a little tricky, since the
// clock is not synchronized and the slave transmitting has to be adapted
// to the master's clock.  ie. reply exactly 1 slot later after receiving the
// first bit.
void Baseband::slave_send_nonconn(Packet * p)
{

#if 0
    // Let's leave this job to the sender.  The sender should ensure this 
    // is enough time for the reply to be transmitted.

    // check if free slot available to transmit
    if (scoState_ == SCO_SEND || scoState_ == SCO_RECV ||
	(scoState_ == SCO_PRIO && (Scheduler::instance().clock()
				   + HDR_BT(p)->txtime() >
				   _sco_slot_start_time))) {
	Packet::free(p);
	return;
    }
#endif

    BBState st = (BBState) HDR_BT(p)->nextState;
    sendBBSig(p);

    if (state() == INQUIRY_SCAN || state() == INQUIRY_RESP) {
	inquiry_rsps_count_++;
    } else if (state() == PAGE_SCAN) {
    }

    if (st != INVALID) {
	double t = (st == SLAVE_RESP ? tick() : slotTime()) - 20E-6;
	slaveChangeStEv_.setState(st);

	Scheduler::instance().schedule(&slaveChangeStHandler_,
				       &slaveChangeStEv_, t);
    } else {
	clearStateLock();
    }
#if 0
    // disable Page Scan timer
    if (st == SLAVE_RESP) {
	// Note: this causes serious problem if slave's ID packet got
	// collided.
	connectable_ = 0;
    }
#endif
}

void Baseband::setScoLTtable(LMPLink ** t)
{
    for (int i = 0; i < 6; i++) {
	lt_sco_table_[i] = (t[i] ? t[i]->txBuffer : NULL);
    }
}

TxBuffer *Baseband::lookupTxBuffer(hdr_bt * bh)
{
    if (!lmp_->curPico || bh->lt_addr_ == 0) {
	return NULL;
    }
    return lmp_->curPico->lookupTxBuffer(bh);
}

// find the txBuffer for the SCO piconet link
TxBuffer *Baseband::lookupScoTxBuffer()
{
    return lt_sco_table_[(clk_ % 24) / 4];
}

void Baseband::sendUp(Packet * p, Handler * h)
{
    int st;
    hdr_bt *bh = HDR_BT(p);

    if (bh->receiver != BD_ADDR_BCAST && bh->receiver != bd_addr_) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		"& bonus recv (ok for certain inq resps) -- ");
	*/
	bh->dump(BtStat::log_, '?', bd_addr_, state_str_s());
    }

    if ((st = handle_recv_lastbit(p)) == 1) {
	return;

    } else if (st == 0) {
    } else if (st == 2 && bh->receiver != bd_addr_
	       && bh->receiver != BD_ADDR_BCAST) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		"$ bonus recv (ok for certain inq resps) -- ");
	*/
	bh->dump(BtStat::log_, '?', bd_addr_, state_str_s());
    }

    if (bh->reply) {
	Packet::free(bh->reply);
    }
    Packet::free(p);
}

// The last bit finishes.
int Baseband::handle_recv_lastbit(Packet * p)
{
    Scheduler & s = Scheduler::instance();
    hdr_bt *bh = HDR_BT(p);

    rxPkt_ = NULL;

    if (trx_st_ != RX_RECV) {
	
	// at the time of switch piconet, receving maybe interrupted.

	// This happens during the process of receiving, the baseband state
	// has changed or piconet has been changed.  A better way would be
	// to handle those cases.

	const char *st =
	    ((bh->receiver == bd_addr_ || bh->receiver == BD_ADDR_BCAST) ?
	     "Recv" : "Not intended Receiver");
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_, "%d (%s) ***RX OFF:%d %f when receiving: ",
		bd_addr_, st, trx_st_, s.clock());
	*/
	bh->dump(BtStat::log_, '?', bd_addr_, state_str_s());
	return 0;
    }

    if (state() == PASSIVE_LISTEN) {
	trx_st_ = RX_ON;
	trxoff_ev_.st = trx_st_;

	// fprintf(BtStat::log_, "%d handle_recv_lastbit: RX_ON\n", bd_addr_);
    } else {
	trxoff_ev_.clearSt();
	turn_off_trx();

	// lock the receiver when receives a short multi-slot pkt.
	int numSlot = hdr_bt::slot_num(bh->type);
	if (numSlot > 1) {
	    double t = numSlot * slotTime() - bh->txtime();
	    if (t >= slotTime()) {
		lockRx();
		s.schedule(&unlockRxHandler_, &unlockRxEv_, t - 150E-6);
	    }
	}
    }
/*
    if (stateLocked()) {
	return 0;
    }
*/

    if (ch_->collide(node_, bh)) {

#if 0
	if (bh->type == hdr_bt::FHS && state() == INQUIRY) {
	    BTNode *nd = node_->lookupNode(bh->sender);
	    if (nd) {
		nd->getStat()->recordInqScanRespFail(bh->pid);
	    }
	} else if (state() == PAGE && bh->type == hdr_bt::Id) {
	    BTNode *nd = node_->lookupNode(bh->sender);
	    if (nd) {
		nd->getStat()->recordPageScanRespFail(bh->pid);
	    }
	}
#endif

	return 0;
    }

    // suppose loss is not complete zero radio signal, otherwise,
    // it should be executed before collision().
    if (ch_->lost(node_, bh)) {
	bh->dump(BtStat::log_, BTLOSTPREFIX, bd_addr_, state_str_s());

	/*
	BTNode *nd = node_->lookupNode(bh->sender);
	fprintf(BtStat::log_, " -- dist:%f(%f %f %f %f) rng:%f %d %f %f\n", 
		node_->distance(bh->X_, bh->Y_), 
		node_->X_, node_->Y_, bh->X_, bh->Y_, 
		node_->radioRange_, bh->sender, nd->X(), nd->Y());
	*/
	return 0;
    }

    if ((trace_null() && bh->type == hdr_bt::Null) ||
	(trace_poll() && bh->type == hdr_bt::Poll) ||
	(trace_rx() && bh->type != hdr_bt::Null
	 && bh->type != hdr_bt::Poll)) {

	bh->dump(BtStat::log_, BTRXPREFIX, bd_addr_, state_str_s());
	if (bh->extinfo >= 10) {
	    bh->dump_sf();
	}
    }

    if (scoState_ == SCO_SEND) {
	return 0;
    } else if (scoState_ == SCO_RECV) {
	bh->txBuffer = lookupScoTxBuffer();
	if (bh->txBuffer && bh->txBuffer->handle_recv(p)) {
	    if (bh->txBuffer->dstTxSlot() < MinTxBufferSlot) {
		bh->txBuffer->dstTxSlot(bh->srcTxSlot);
	    }
	    uptarget_->recv(p, (Handler *) 0);
	    return 1;
	} else {
	    return 0;
	}
    }

    // sender_ = bh->sender;

    switch (state()) {

    case PAGE_SCAN:
	_slave_reply(bh);
	break;

    case SLAVE_RESP:
	handleFhsMsg_Pscan(bh);
	_slave_reply(bh);
	break;

    case ROLE_SWITCH_SLAVE:
	handleFhsMsg_RS(bh);
	_slave_reply(bh);
	break;

    case ROLE_SWITCH_MASTER:
	handleMsg_RS_MA(bh);
	break;

    case PAGE:
	// clkf_ = clke_;   // froze clock clkn16_12  // move forward
	if (bh->type == hdr_bt::Id) {
	    change_state(MASTER_RESP);
	    master_rsps_count_ = 0;
	    if (trace_state()) {
		// Barun
		//fprintf(stderr, "Page time: %f %d\n",
			//s.clock() - page_start_time_,
			//int ((s.clock() - page_start_time_) / BTSlotTime +
			     //0.5));
	    }
	}
	break;

    case RS_NEW_CONNECTION_MASTER:
	change_state(CONNECTION);
	stableState_ = CONNECTION;

	lmp_->role_switch_bb_complete(slave_, 1);
	ch_->clearPicoChannel(old_master_bd_addr_);
	break;

    case RS_NEW_CONNECTION_SLAVE:
	if (bh->type == hdr_bt::Poll && bh->reply) {
	    _slave_reply(bh);

	    change_state(CONNECTION);
	    stableState_ = CONNECTION;
	    isMaster_ = 0;
	    comp_clkoffset(bh->txtime(), &clock_offset_, &slot_offset_);
	    lmp_->role_switch_bb_complete(slave_, 1);

	    ch_->clearPicoChannel(old_master_bd_addr_);
	}
	break;

    case NEW_CONNECTION_SLAVE:
	handleMsg_NC_SL(bh);
	break;

    case NEW_CONNECTION_MASTER:
	handleMsg_NC_MA(bh);
	break;

    case MASTER_RESP:
	if (bh->type == hdr_bt::Id) {
	    newConnectionTimer_ = newConnectionTO_;
	    change_state(NEW_CONNECTION_MASTER);
	    clk_ = clkn_;
	    master_bd_addr_ = bd_addr_;
	}
	break;

    case INQUIRY_RESP:
	if (bh->type == hdr_bt::Id && bh->reply) {
	    if (pagescan_after_inqscan_) {
		connectable_ = 1;
		HDR_BT(bh->reply)->nextState = PAGE_SCAN;
		lmp_->HCI_Write_Page_Scan_Activity(4096, 4096);
		discoverable_ = 0;
	    }
	    _slave_reply(bh);
	    if (trace_state()) {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_, BTPREFIX1
			"%d send INQUIRY_RESP.\n", bd_addr_);
		*/
	    }
	}
	break;

    case INQUIRY_SCAN:
	handleMsg_IScan(bh);
	break;

    case INQUIRY:
	if (bh->type == hdr_bt::FHS) {
	    handle_inquiry_response(bh);
	    if (trace_state()) {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_, BTPREFIX1
			"%d recv INQUIRY_RESP.\n", bd_addr_);
		*/
	    }
	}
	break;

    case STANDBY:
	break;

    default:			// CONNECTION
	return handleMsg_ConnState(p);
    }

    return 2;
}

void Baseband::handleFhsMsg_Pscan(hdr_bt * bh)
{
    Scheduler & s = Scheduler::instance();
    clk_ = bh->u.fhs.clk;
    master_bd_addr_ = bh->u.fhs.bd_addr_;
    lt_addr_ = bh->u.fhs.lt_addr_;

    if (clk_ev_.uid_ > 0) {
	if (trace_state()) {
	    fprintf(BtStat::log_, "%d %s cancel clk\n", bd_addr_,
		    __FUNCTION__);
	}
	s.cancel(&clk_ev_);
    }
    if (trace_state()) {
	fprintf(BtStat::log_,
		"%f sched clk at %f by %s (SLAVE_RESP)\n", s.clock(),
		s.clock() + slotTime() - bh->txtime() + BT_CLKN_CLK_DIFF,
		__FUNCTION__);
    }
    s.schedule(&clk_handler_, &clk_ev_,
	       slotTime() - bh->txtime() + BT_CLKN_CLK_DIFF);
}

void Baseband::handleFhsMsg_RS(hdr_bt * bh)
{
    Scheduler & s = Scheduler::instance();
    double t;
    clk_ = bh->u.fhs.clk;
    if (master_bd_addr_ != bh->u.fhs.bd_addr_) {
	old_master_bd_addr_ = master_bd_addr_;
	master_bd_addr_ = bh->u.fhs.bd_addr_;
    }
    lt_addr_ = bh->u.fhs.lt_addr_;

    if (clk_ev_.uid_ > 0) {
	if (trace_state()) {
	    fprintf(BtStat::log_, "%d %s cancel clk\n", bd_addr_,
		    __FUNCTION__);
	}
	s.cancel(&clk_ev_);
    }

    t = slotTime() + slotTime() - bh->txtime() + slot_offset_ * 1E-6;
    t += BT_CLKN_CLK_DIFF;
    clk_ += 6;

    if (trace_state()) {
	fprintf(BtStat::log_,
		"%f sched clk at %f by %s (ROLE_SWITCH_SLAVE)\n",
		s.clock(), s.clock() + t, __FUNCTION__);
    }
    s.schedule(&clk_handler_, &clk_ev_, t);
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		BTPREFIX1 "RS_slave: clk_:%d master:%d lt:%d t:%f\n",
		clk_, master_bd_addr_, lt_addr_, t + s.clock());
	*/
    }
}

void Baseband::handleMsg_RS_MA(hdr_bt * bh)
{
    Scheduler & s = Scheduler::instance();
    if (bh->type == hdr_bt::Id) {
	newConnectionTimer_ = newConnectionTO_;
	change_state(RS_NEW_CONNECTION_MASTER);

	if (master_bd_addr_ != bd_addr_) {
	    old_master_bd_addr_ = master_bd_addr_;
	    master_bd_addr_ = bd_addr_;
	}
	isMaster_ = 1;
	if (clk_ev_.uid_ > 0) {
	    if (trace_state()) {
		fprintf(BtStat::log_, "%d %s cancel clk\n", bd_addr_,
			__FUNCTION__);
	    }
	    s.cancel(&clk_ev_);
	}
	double t = slotTime() - bh->txtime() + slot_offset_ * 1E-6 +
	    BT_CLKN_CLK_DIFF;
	clk_ = (clkn_ & 0xFFFFFFFC) + 2;
	if (t >= slotTime() + slotTime()) {
	    clk_ += 4;
	}
	if (trace_state()) {
	    fprintf(BtStat::log_,
		    "%f sched clk at %f by %s (ROLE_SWITCH_MASTER)\n",
		    s.clock(), s.clock() + t, __FUNCTION__);
	}
	s.schedule(&clk_handler_, &clk_ev_, t);

	if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1 "RS_master: clk_:%d master:%d t:%f\n",
		    clk_, master_bd_addr_, t + s.clock());
	*/
	}
    }
}

void Baseband::handleMsg_NC_SL(hdr_bt * bh)
{
    if (bh->type == hdr_bt::Poll && bh->reply) {
	_slave_reply(bh);

	connectable_ = 0;
	change_state(CONNECTION);
	stableState_ = CONNECTION;
	isMaster_ = 0;
	if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, BTPREFIX1 "Slave lt_addr_: %d\n",
		    lt_addr_);
	*/
	}
	int clkoffset;
	int sltoffset;
	comp_clkoffset(bh->txtime(), &clkoffset, &sltoffset);
	lmp_->connection_ind(master_bd_addr_, lt_addr_,
			     clkoffset, sltoffset);
	// Why return clkoffset, sltoffset since they change over time.
	inquiryScan_cancel();
    }
}

void Baseband::handleMsg_NC_MA(hdr_bt * bh)
{
    Scheduler & s = Scheduler::instance();
    change_state(CONNECTION);
    stableState_ = CONNECTION;
    if (clk_ev_.uid_ > 0) {
	if (trace_state()) {
	    fprintf(BtStat::log_, "%d %s cancel clk\n", bd_addr_,
		    __FUNCTION__);
	}
	s.cancel(&clk_ev_);
    }

    if (trace_state()) {
	fprintf(BtStat::log_,
		"%f sched clk at %f by %s (NEW_CONNECTION_MASTER)\n",
		s.clock(),
		s.clock() + slotTime() - bh->txtime() + BT_CLKN_CLK_DIFF,
		__FUNCTION__);
    }
    s.schedule(&clk_handler_, &clk_ev_, slotTime() - bh->txtime()
	       + BT_CLKN_CLK_DIFF);
    t_clk_00_ = t_clkn_00_ + BT_CLKN_CLK_DIFF;
    clk_ = (clkn_ & 0xFFFFFFFC) + 2;
    master_bd_addr_ = bd_addr_;
    isMaster_ = 1;
    lmp_->page_complete(slave_, 1);
}

void Baseband::handleMsg_IScan(hdr_bt * bh)
{
    if (bh->type == hdr_bt::Id) {
	inBackoff_ = 1;
	int maxBackOff = backoffParam_;
	if (ver_ >= 12 && T_inquiry_scan_ < 4096) {
	    maxBackOff = backoffParam_small_;
	}
	backoffTimer_ = Random::integer(maxBackOff);
	if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_, BTPREFIX1 "%d backoffTimer_:%d\n",
		    bd_addr_, backoffTimer_);
	*/
	}
	if (ver_ < 12) {
	    change_state(stableState_);

	    // Since spec1.2, a slave sends reponses before backing off.
	} else {

	    //XXX: why it needed??
	    state_ = INQUIRY_RESP;	// for FH purpose

	    HDR_BT(bh->reply)->nextState = stableState_;
	    if (pagescan_after_inqscan_) {
		connectable_ = 1;
		HDR_BT(bh->reply)->nextState = PAGE_SCAN;
		lmp_->HCI_Write_Page_Scan_Activity(4096, 4096);
		discoverable_ = 0;
	    }
	    _slave_reply(bh);
	    if (trace_state()) {
		/** Commented by Barun [07 March 2013]
		fprintf(BtStat::log_, BTPREFIX1
			"%d send INQUIRY_RESP.\n", bd_addr_);
		*/
	    }
	}
    }
}

int Baseband::handleMsg_ConnState(Packet * p)
{
    hdr_bt *bh = HDR_BT(p);
    Scheduler & s = Scheduler::instance();
    double t;

    // being taken over by a new master
    if (bh->type == hdr_bt::FHS && !isMaster() && bh->reply
	&& state() == CONNECTION) {
	clk_ = bh->u.fhs.clk;
	master_bd_addr_ = bh->u.fhs.bd_addr_;
	lt_addr_ = bh->u.fhs.lt_addr_;

	if (clk_ev_.uid_ > 0) {
	    if (trace_state()) {
		fprintf(BtStat::log_, "%d %s cancel clk\n", bd_addr_,
			__FUNCTION__);
	    }
	    s.cancel(&clk_ev_);
	}

	t = slotTime() - bh->txtime() + slotTime() + slot_offset_ * 1E-6;
	// FIXME
	// BT_CLKN_CLK_DIFF ??
	clk_ += 6;

	s.schedule(&clk_handler_, &clk_ev_, t);
	if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	    fprintf(BtStat::log_,
		    BTPREFIX1 "RS_slave: clk_:%d master:%d t:%f\n",
		    clk_, master_bd_addr_, t + s.clock());
	    fprintf(BtStat::log_, "%f start clk at %f\n", s.clock(),
		    t + s.clock());
	*/
	}

	_slave_reply(bh);
	// break;
	return 2;
    }
#ifdef PARK_STATE
    if (isMaster() && inBeacon_ && bh->type == hdr_bt::Id) {
	int ntick = (int) ((s.clock() - beacon_instant_) / tick());
	lmp_->unpark_req(ntick);
	// break;
	return 2;
    }
#endif

    if (bh->lt_addr_ == 0) {	// broadcasting pkt
	bh->bcast = 1;

	if (state() == UNPARK_SLAVE) {	// allowed to response next slot.
	}
	// set sender to the master ??
	uptarget_->recv(p, (Handler *) 0);
	return 1;

    } else if ((bh->txBuffer = lookupTxBuffer(bh))) {
	if (bh->txBuffer->dstTxSlot() < MinTxBufferSlot) {
	    bh->txBuffer->dstTxSlot(bh->srcTxSlot);
	}
	// printf("txBuffer->slot(): %d\n", bh->txBuffer->slot());
	if (bh->txBuffer->handle_recv(p)) {
	    uptarget_->recv(p, (Handler *) 0);
	    return 1;
	}
    }

    return 2;
}

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

// prev state: STANDBY / CONNECTION
// next state: Slave-Response
//
// sched word in page_scan mode:
//
// the slave's transmission is aligned to the master's clock by recording
// the time the first bit arrives.  It can transmit anytime unless colliding
// to a SCO link. So the transmission is determined by SCO link state
// instead of a sched_word of its own.
//
// The following variables should be set before calling this function.
//      T_w_page_scan_ default to 36 (18 slots or 11.25ms)
//      T_page_scan_ default to 4096 (2048 slots or 1.28s)
//
// SR mode:         T_page_scan_
//      R0      <=1.28s & = T_w_page_scan_
//      R1      <=1.28s
//      R2      <=2.56s
//
void Baseband::page_scan(BTSchedWord * sched_word)
{
    connectable_ = 1;

    // at next clkn event, it decreases to 0 and triggers setting timer
    T_page_scan_timer_ = 1;
    isMaster_ = 0;
    set_sched_word(sched_word);

    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		BTPREFIX1 "%d start page scan. clkn:%d \n", bd_addr_,
		clkn_);
	*/
	lmp_->dump(BtStat::log_, 1);
    }
}

void Baseband::page_scan_cancel()
{
    if (connectable_) {
	connectable_ = 0;
	if (state() == PAGE_SCAN) {
	    change_state(stableState_);
	}
    }

    if (state() != PASSIVE_LISTEN) {
	trxoff_ev_.clearSt();
	turn_off_trx();
    }
}

// The spec only specify N_inquiry_ and N_page_ for HV3 or EV3 links,
// that is, T_poll = 6.
// suppose that numSco <= 2.
// A HV2 link counts as 2 HV3 links here, although it take 1/2 load instead
// of 2/3 loads of two HV3 links.  Anyway, the spec doesn't say anything
// about HV2 links.
void Baseband::setNInquiry(int numSco)
{
    N_inquiry_ = 256 * (numSco + 1);
}

void Baseband::setNPage(int numSco)
{
    int base = (SR_mode_ == 0 ? 1 : SR_mode_ * 128);
    N_page_ = base * (numSco + 1);
}

// prev state: STANDBY / CONNECTION
// next state: Master-Response
// Terminate: pageTO / receive response
void Baseband::page(bd_addr_t slave, uint8_t ps_rep_mod, uint8_t psmod,
		    // int16_t clock_offset, BTSchedWord * sched_word,
		    int32_t clock_offset, BTSchedWord * sched_word,
		    int pageto)
{
    // N_page: repeat trainType times
    // SR mode      N_page (no SCO)  one SCO        two SCO
    //   R0           >= 1            >=2           >=3
    //   R1           >= 128          >=256         >=384
    //   R2           >= 256          >=512         >=768

    // page_conn_handle = connhand;
    slave_ = slave;
    page_sr_mod_ = ps_rep_mod;
    page_ps_mod_ = psmod;
    page_timer_ = pageTO_;
    page_train_switch_timer_ = N_page_ * 32;
    set_sched_word(sched_word);

#if 0
    /* specs says that the highest bit of bd->offset indicates if it's valid
     * and the last 2 bits of clke_ should always equal to last 2 bits of
     * clkn_.
     */
    int offset = 0;
    if (bd->offset & 0x8000) {
	offset = (bd->offset & 0x7FFF) << 2;
    }
    clke_ = clkn_ + offset;
#endif

    // clke_ = clkn_ + (clock_offset & 0xfffc);
    clke_ = clkn_ + (clock_offset & 0xFFFFFFFC);
    if (trace_state()) {
	/** Commented by Barun [07 March 2013]
	fprintf(BtStat::log_,
		BTPREFIX1
		"%d start to page %d. clkn:%d, offset:%d, clke:%d\n",
		bd_addr_, slave, clkn_, clock_offset, clke_);
	*/
	lmp_->dump(BtStat::log_, 1);
    }
    train_ = Train_A;
    change_state(PAGE);
    // notices that PAGE should not be interrupted (expt SCO), otherwise
    // it cannot get back, unlike PAGE SCAN
    isMaster_ = 1;

    page_start_time_ = Scheduler::instance().clock();

    // bypass pageTO_
    if (pageto > 0) {
	page_timer_ = pageto;
	// pageTO_ = pageto;
    }
}

void Baseband::page_cancel()
{
    if (state() != PAGE) {
	return;
    }
    // page_timer_ = 1;  // reset timer to force it run out
    trxoff_ev_.clearSt();
    turn_off_trx();
    change_state(stableState_);
}

// The following variables should be set before calling this function.
//      inqAddr_ = giac
//      T_w_inquiry_scan_ default to 36 (18 slots or 11.25ms)
//      T_inquiry_scan_ default to 4096 (2048 slots or 1.28s)
void Baseband::inquiry_scan(BTSchedWord * sched_word)
{
    discoverable_ = 1;
    T_inquiry_scan_timer_ = 1;	// let the clkn_ routine to set it up
    inquiry_rsps_count_ = 0;	// Is it necessary to reset N to 0 ??
    set_sched_word(sched_word);
}

void Baseband::inquiryScan_cancel()
{
    if (inBackoff_) {
	inBackoff_ = 0;
    }
    if (state() == INQUIRY_RESP) {
	change_state(stableState_);
    }
    if (discoverable_) {
	discoverable_ = 0;
	if (state() == INQUIRY_SCAN) {
	    change_state(stableState_);
	}
    }
    if (state() != PASSIVE_LISTEN) {
	trxoff_ev_.clearSt();
	turn_off_trx();
    }
}

void Baseband::inquiry_cancel()
{
    if (state() != INQUIRY) {
	return;
    }
    // Specs says don't return anything to upper layer.
    trxoff_ev_.clearSt();
    turn_off_trx();
    change_state(stableState_);
}

void Baseband::inquiry(int Lap, int inquiry_length, int num_responses,
		       BTSchedWord * schedword)
{
    //              no SCO          one SCO         two SCO
    //  N_inquiry    >=256            >=512           >=768
    //  
    //  at least 3 tran switches
    //
    change_state(INQUIRY);
    // _inqParam.periodic = 0;
    inqAddr_ = Lap;
    inq_max_num_responses_ = num_responses;
    inq_num_responses_ = 0;
    inquiryTO_ = inquiry_length;	// 1.28 - 61.44s
    inq_timer_ = inquiry_length;	// 1.28 - 61.44s
    // discoved_bd_ = 0;
    inquiry_train_switch_timer_ = N_inquiry_ * 32;
    set_sched_word(schedword);
    train_ = Train_A;

    inq_start_time_ = Scheduler::instance().clock();
}

void Baseband::handle_inquiry_response(hdr_bt * bh)
{
    double now = Scheduler::instance().clock();
    Bd_info *bd =
	new Bd_info(bh->u.fhs.bd_addr_, bh->u.fhs.clk, bh->u.fhs.clk -
		    (clkn_ & 0xFFFFFFFC));
    bd->last_seen_time_ = now;
    bd->dist_ = node_->distance(bh->X_, bh->Y_, bh->Z_);
    bd->dump();

    int isnew;
    bd = lmp_->_add_bd_info(bd, &isnew);	// old bd may got deleted.

    if (page_after_inq_) {
	// page(bd->bd_addr, 0, 0, bd->offset, sched_word_);
	node_->bnep_->connect(bd->bd_addr_);
	return;
    }

    if (!isnew) {		// bd is not newly discovered.
	return;
    }

    if (++inq_num_responses_ == inq_max_num_responses_) {
	change_state(stableState_);
	lmp_->inquiry_complete(inq_num_responses_);	// may change _state.
    }

    double it = now - inq_start_time_;
    if (trace_state()) {
	fprintf(stderr, "Inq time: %f %d num: %d ave: %f %d\n", it,
		int (it / BTSlotTime + 0.5), inq_num_responses_,
		it / inq_num_responses_,
		int (it / BTSlotTime / inq_num_responses_ + 0.5));
    }
}

Packet *Baseband::_allocPacket(FH_sequence_type fs, bd_addr_t addr,
			       clk_t clk, bd_addr_t recv,
			       hdr_bt::packet_type t)
{
    Packet *p = Packet::alloc();
    hdr_bt *bh = HDR_BT(p);
    hdr_cmn *ch = HDR_CMN(p);

    ch->ptype() = PT_BT;

    bh->pid = hdr_bt::pidcntr++;
    bh->ac = addr;
    bh->lt_addr_ = 0;
    bh->type = t;

    bh->sender = bd_addr_;
    bh->receiver = recv;
    bh->srcTxSlot = bh->dstTxSlot = 0;

    bh->fs_ = FH_kernel(clk, clkf_, fs, addr);
    bh->clk = clk;
    bh->size = hdr_bt::packet_size(t);
    bh->transmitCount = 0;
    bh->comment("");
    bh->nextState = STANDBY;

    bh->X_ = node_->X();
    bh->Y_ = node_->Y();
    bh->Z_ = node_->Z();

    return p;
}

Packet *Baseband::genIdPacket(FH_sequence_type fs, bd_addr_t addr,
			      clk_t clk, bd_addr_t recv)
{
    return _allocPacket(fs, addr, clk, recv, hdr_bt::Id);
}

Packet *Baseband::genFHSPacket(FH_sequence_type fs, bd_addr_t addr,
			       clk_t clk, bd_addr_t myaddr,
			       clk_t myclk, int am, bd_addr_t recv)
{
    Packet *p = _allocPacket(fs, addr, clk, recv, hdr_bt::FHS);
    hdr_bt *bh = HDR_BT(p);
    bh->u.fhs.bd_addr_ = myaddr;
    bh->u.fhs.clk = myclk & 0xFFFFFFFC;
    bh->u.fhs.lt_addr_ = am;

    return p;
}

Packet *Baseband::genPollPacket(FH_sequence_type fs, bd_addr_t addr,
				clk_t clk, bd_addr_t recv)
{
    return _allocPacket(fs, addr, clk, recv, hdr_bt::Poll);
}

Packet *Baseband::genNullPacket(FH_sequence_type fs, bd_addr_t addr,
				clk_t clk, bd_addr_t recv)
{
    return _allocPacket(fs, addr, clk, recv, hdr_bt::Null);
}

// Baseband frequency hopping kernel
// bd_addr is LAP + 4 LSB bits of UAP = 28 bits
int Baseband::FH_kernel(clk_t CLK, clk_t CLKF,
			FH_sequence_type FH_seq, bd_addr_t bd_addr)
{
    int X, Y1, Y2, A, B, C, D, E, F;	// see specs for the meanings
    int Z, P;
    int Ze[5];			// Z expanded
    int reg_bank_index;
    int Xp79, Xprs79, Xprm79, Xir79;
    // unsigned int Xi79;
    int k_offset = (train_ == Train_A ? 24 : 8);
    int N;
    int i;
    int ret;

    A = (bd_addr >> 23) & 0x1F;
    B = (bd_addr >> 19) & 0x0F;
    C = ((bd_addr >> 4) & 0x10)
	+ ((bd_addr >> 3) & 0x08)
	+ ((bd_addr >> 2) & 0x04) + ((bd_addr >> 1) & 0x02) +
	(bd_addr & 0x01);
    D = (bd_addr >> 10) & 0x01FF;
    E = ((bd_addr >> 7) & 0x40)
	+ ((bd_addr >> 6) & 0x20)
	+ ((bd_addr >> 5) & 0x10)
	+ ((bd_addr >> 4) & 0x08)
	+ ((bd_addr >> 3) & 0x04) + ((bd_addr >> 2) & 0x02) +
	((bd_addr >> 1) & 0x01);
    F = 0;

    switch (FH_seq) {
    case FHPage:
    case FHInquiry:		// Xi79 == Xp79
	// a offset 64 added to make sure it the difference is positive.
	// It does change the result and make output to agree with
	// the reference result in the specification.
	Xp79 = (((CLK >> 12) & 0x1F) + k_offset +
		(((CLK >> 1) & 0x0E) + (CLK & 0x01) + 64 -
		 ((CLK >> 12) & 0x1F)) % 16) % 32;
	X = Xp79 & 0x1F;
	Y1 = (CLK >> 1) & 0x01;
	Y2 = Y1 * 32;
	break;
    case FHPageScan:
	X = (CLK >> 12) & 0x1F;
	Y1 = 0;
	Y2 = 0;
	break;
    case FHMasterResp:
	N = master_rsps_count_;
	Xprm79 = (((CLKF >> 12) & 0x1F) + k_offset +
		  (((CLKF >> 1) & 0x0E) + (CLKF & 0x01) + 64 -
		   ((CLKF >> 12) & 0x1F)) % 16 + N) % 32;
	X = Xprm79 & 0x1F;
	Y1 = (CLK >> 1) & 0x01;
	Y2 = Y1 * 32;
	break;
    case FHSlaveResp:
	N = slave_rsps_count_;
	Xprs79 = (((CLKF >> 12) & 0x1F) + N) % 32;
	X = Xprs79 & 0x1F;
	Y1 = (CLK >> 1) & 0x01;
	Y2 = Y1 * 32;
	break;
    case FHInqScan:
	N = inquiry_rsps_count_;
	Xir79 = (((CLK >> 12) & 0x1F) + N) % 32;
	X = Xir79 & 0x1F;
	Y1 = 0;
	Y2 = 0;
	break;
    case FHInqResp:
	N = inquiry_rsps_count_;
	// CLK -= 2;    // a hack to eliminate fs mistach
	Xir79 = (((CLK >> 12) & 0x1F) + N) % 32;
	X = Xir79 & 0x1F;
	Y1 = 1;
	Y2 = 32;
	break;
    case FHChannel:
	X = (CLK >> 2) & 0x1F;
	Y1 = (CLK >> 1) & 0x01;
	Y2 = Y1 * 32;
	A ^= ((CLK >> 21) & 0x1F);
	C ^= ((CLK >> 16) & 0x1F);
	D ^= ((CLK >> 7) & 0x1FF);
	F = (((CLK >> 7) & 0x1FFFFF) * 16) % 79;
	break;
    case FHAFH:
	X = (CLK >> 2) & 0x1F;
	Y1 = 0;			// Same channel mechanisim. master channel always.
	Y2 = 0;
	A ^= ((CLK >> 21) & 0x1F);
	C ^= ((CLK >> 16) & 0x1F);
	D ^= ((CLK >> 7) & 0x1FF);
	F = (((CLK >> 7) & 0x1FFFFF) * 16) % 79;
	break;
    }

    /* First addition & XOR operations */
    Z = ((X + A) % 32) ^ B;

    /* Permutation operation */
    Y1 = (Y1 << 4) + (Y1 << 3) + (Y1 << 2) + (Y1 << 1) + Y1;
    P = ((C ^ Y1) << 9) + D;

    for (i = 0; i < 5; i++) {
	Ze[i] = Z & 0x01;
	Z >>= 1;
    }
#define BUTTERFLY(k,i,j) if((P>>k)&0x01) {int t=Ze[i]; Ze[i]=Ze[j]; Ze[j]=t; }
    /*
       if ( (p >> 13) & 0x01) {
       int t = Z & 0x02;
       Z =  (Z & 0xFFFFFFFC) | ((Z >> 1) & 0x02);
       Z =  (Z & 0xFFFFFFFB) | t << 1;
       }
     */
    BUTTERFLY(13, 1, 2);
    BUTTERFLY(12, 0, 3);
    BUTTERFLY(11, 1, 3);
    BUTTERFLY(10, 2, 4);
    BUTTERFLY(9, 0, 3);
    BUTTERFLY(8, 1, 4);
    BUTTERFLY(7, 3, 4);
    BUTTERFLY(6, 0, 2);
    BUTTERFLY(5, 1, 3);
    BUTTERFLY(4, 0, 4);
    BUTTERFLY(3, 3, 4);
    BUTTERFLY(2, 1, 2);
    BUTTERFLY(1, 2, 3);
    BUTTERFLY(0, 0, 1);
#undef BUTTERFLY
    Z = (Ze[4] << 4) + (Ze[3] << 3) + (Ze[2] << 2) + (Ze[1] << 1) + Ze[0];

    /* Second addition operation */
    reg_bank_index = (Z + E + F + Y2) % 79;

    /* select register in Register bank */
    ret = (reg_bank_index <= 39 ? reg_bank_index * 2 :
	   (reg_bank_index - 39) * 2 - 1);

    /* Adapted Frequency Hopping */
    // if (FH_seq == FHAFH && _notUsedCh[ret]) {
    if (FH_seq == FHAFH && usedChSet_.notUsed(ret)) {
	F = (((CLK >> 7) & 0x1FFFFF) * 16) % usedChSet_.numUsedCh();
	reg_bank_index = (Z + E + F + Y2) % usedChSet_.numUsedCh();
	ret = usedChSet_.usedCh(reg_bank_index);
    }

    return ret;
}

// This method returns the sequence.
signed char *Baseband::seq_analysis(bd_addr_t bdaddr, FH_sequence_type fs,
				    int clk, int clkf, int len, int step,
				    signed char *buf)
{
    if (!buf) {
	buf = new signed char[len];
    }

    int i;
    for (i = 0; i < len; i++) {
	buf[i] = FH_kernel(clk + i * step, clkf, fs, bdaddr);
    }

    int prev[79];
    int max[79];
    int min[79];
    int sum[79];
    int cntr[79];

    for (i = 0; i < 79; i++) {
	prev[i] = -1;
	max[i] = 0;
	min[i] = 255;
	sum[i] = 0;
	cntr[i] = 0;
    }

    for (i = 0; i < len; i++) {
	if (prev[buf[i]] < 0) {
	    prev[buf[i]] = i * step;
	} else {
	    int diff = i * step - prev[buf[i]];
	    prev[buf[i]] = i * step;
	    if (diff > max[buf[i]]) {
		max[buf[i]] = diff;
	    }
	    if (diff < min[buf[i]]) {
		min[buf[i]] = diff;
	    }
	    sum[buf[i]] += diff;
	    cntr[buf[i]]++;
	}
    }

    for (i = 0; i < 79; i++) {
	/** Commented by Barun [07 March 2013]
	fprintf(stdout, "%d %d %d %0.2f %d\n",
		i, min[i], max[i], double (sum[i]) / cntr[i], cntr[i] + 1);
	*/
    }

    return buf;
}

// This method intends to produce the sample data on specs 1.1 pp963-968
// addr[] = { 0x0, 0x2a96ef25, 0x6587cba9 };
// 0x9e8b33 is used for inquiry and inquiry scan
int Baseband::test_fh(bd_addr_t bdaddr)
{
    unsigned int clk, clkf, fh;
    FH_sequence_type fs;
    int i, j;

    printf("\nNEW SET\n\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //              test inquiry scan/page scan                   //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    inquiry_rsps_count_ = 0;
    fs = FHInqScan;
    clk = 0x00;

    printf("Hop sequence {k} for PAGE SCAN/INQUIRY SCAN SUBSTATE:\n");
    printf("CLKN start:     0x%07x\n", clk);
    printf("UAP / LAP:      0x%08x\n", bdaddr);
    printf
	("#ticks:         0000 | 1000 | 2000 | 3000 | 4000 | 5000 | 6000 | 7000 |\n");
    printf
	("                --------------------------------------------------------");

    for (i = 0; i < 64; i++) {
	fh = FH_kernel(clk + 0x1000 * i, 0, fs, bdaddr);
	if (i % 8 == 0) {
	    printf("\n0x%07x:     ", clk + 0x1000 * i);
	}
	printf("   %.02d |", fh);
    }
    printf("\n");
    printf("\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //                      test inquiry /page                    //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    inquiry_rsps_count_ = 0;
    fs = FHPage;
    clk = 0x00;
    train_ = Train_A;

    printf("Hop sequence {k} for PAGE STATE/INQUIRY SUBSTATE:\n");
    printf("CLKE start:     0x%07x\n", clk);
    printf("UAP / LAP:      0x%08x\n", bdaddr);
    printf
	("#ticks:         00 01 02 03 | 04 05 06 07 | 08 09 0a 0b | 0c 0d 0e 0f |\n");
    printf
	("                --------------------------------------------------------");

    for (j = 0; j < 4; j++) {
	for (i = 0; i < 64; i++) {
	    fh = FH_kernel(clk + i, 0, fs, bdaddr);
	    if (i % 16 == 0) {
		printf("\n0x%07x:      ", clk + i);
	    }
	    printf("%.02d ", fh);
	    if (i % 4 == 3) {
		printf("| ");
	    }
	}
	if (j < 3) {
	    printf("\n...");
	}
	clk += 0x1000;

	train_ = (train_ == Train_A ? Train_B : Train_A);
    }
    printf("\n");


    ////////////////////////////////////////////////////////////////
    //                                                            //
    //               test slave page response                     //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    slave_rsps_count_ = 0;
    fs = FHSlaveResp;
    clkf = 0x10;

    printf("\nHop sequence {k} for SLAVE PAGE RESPONSE SUBSTATE:\n");
    printf("CLKN* =         0x%07x\n", clkf);
    printf("UAP / LAP:      0x%08x\n", bdaddr);
    printf
	("#ticks:         00 | 02 04 | 06 08 | 0a 0c | 0e 10 | 12 14 | 16 18 | 1a 1c | 1e\n");
    printf
	("                ----------------------------------------------------------------");

    for (i = 0; i < 64; i++) {
	clk = clkf + 2 + i * 2;
	fh = FH_kernel(clk, clkf, fs, bdaddr);
	if (i % 2 == 0) {	// right before CLK1 becomes 0
	    slave_rsps_count_++;
	}

	if (i % 16 == 0) {
	    printf("\n0x%07x:      ", clk);
	}
	printf("%.02d ", fh);
	if (i % 2 == 0) {
	    printf("| ");
	}
    }
    printf("\n");
    printf("\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //               test master page response                    //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    fs = FHMasterResp;
    train_ = Train_A;
    clkf = 0x12;
    master_rsps_count_ = 0;

    printf("\nHop sequence {k} for MASTER PAGE RESPONSE SUBSTATE:\n");
    printf("Offset value:   %d\n", (train_ == Train_A ? 24 : 8));
    printf("CLKE* =         0x%07x\n", clkf);
    printf("UAP / LAP:      0x%08x\n", bdaddr);
    printf
	("#ticks:         00 02 | 04 06 | 08 0a | 0c 0e | 10 12 | 14 16 | 18 1a | 1c 1e |\n");
    printf
	("                ----------------------------------------------------------------");

    for (i = 0; i < 64; i++) {
	clk = clkf + 2 + i * 2;
	if (i % 2 == 0) {	// right before CLK1 becomes 0
	    master_rsps_count_++;
	}
	fh = FH_kernel(clk, clkf, fs, bdaddr);
	if (i % 16 == 0) {
	    printf("\n0x%07x:      ", clk);
	    // printf("\n");
	}
	printf("%.02d ", fh);
	if (i % 2 == 1) {
	    printf("| ");
	}
    }
    printf("\n");
    printf("\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //               test basic channel hopping                   //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    clk = 0x10;
    fs = FHChannel;
    printf
	("\nHop sequence {k} for CONNECTION STATE (Basic channel hopping sequence; ie, non-AFH):\n");
    printf("CLK start:       0x%07x\n", clk);
    printf("UAP / LAP:       0x%08x\n", bdaddr);
    printf
	("#ticks:          00 02 | 04 06 | 08 0a | 0c 0e | 10 12 | 14 16 | 18 1a | 1c 1e |\n");
    printf
	("                 ----------------------------------------------------------------");

    for (i = 0; i < 512; i++) {
	fh = FH_kernel(clk + 2 * i, 0, fs, bdaddr);
	if (i % 16 == 0) {
	    printf("\n0x%07x:       ", clk + 2 * i);
	}
	printf("%.02d ", fh);
	if (i % 2 == 1) {
	    printf("| ");
	}
    }
    printf("\n");
    printf("\n");

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //             test adapted channel hopping                   //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    clk = 0x10;
    test_afh(clk, bdaddr, "0x7FFFFFFFFFFFFFFFFFFF",
	     "Hop Sequence {k} for CONNECTION STATE (Adapted channel hopping sequence with all channel used; ie, AFH(79)):");

    test_afh(clk, bdaddr, "0x7FFFFFFFFFFFFFC00000",
	     "Hop Sequence {k} for CONNECTION STATE (Adapted channel hopping sequence with channels 0 to 21 unused):");

    test_afh(clk, bdaddr, "0x55555555555555555555",
	     "Hop Sequence {k} for CONNECTION STATE (Adapted channel hopping sequence with even channels used):");

    test_afh(clk, bdaddr, "0x2AAAAAAAAAAAAAAAAAAA",
	     "Hop Sequence {k} for CONNECTION STATE (Adapted channel hopping sequence with odd channels used):");

    return 0;
}

    // test adapted channel hopping
void Baseband::test_afh(int clk, int bdaddr, const char *map,
			const char *s)
{
    unsigned int fh;
    FH_sequence_type fs;
    int i;
    fs = FHAFH;
    usedChSet_.importMapByHexString(map);

    printf("%s\n", s);
    printf("CLK start:    0x%07x\n", clk);
    printf("ULAP:         0x%08x\n", bdaddr);
    printf("Used Channels:0x");
    usedChSet_.dump();
    printf
	("#ticks:       00 02 | 04 06 | 08 0a | 0c 0e | 10 12 | 14 16 | 18 1a | 1c 1e |\n");
    printf
	("              ---------------------------------------------------------------");
    for (i = 0; i < 512; i++) {
	fh = FH_kernel(clk + 2 * i, 0, fs, bdaddr);
	if (i % 16 == 0) {
	    printf("\n0x%07x     ", clk + 2 * i);
	}
	printf("%.02d ", fh);
	if (i % 2 == 1) {
	    printf("| ");
	}
    }
    printf("\n\n\n");
}


//////////////////////////////////////////////////////////
//                      Handlers                        //
//////////////////////////////////////////////////////////

void BTCLKNHandler::handle(Event * e)
{
    bb_->handle_clkn(e);
}

void BTCLKHandler::handle(Event * e)
{
    bb_->handle_clk(e);
}

void BTRESYNCHandler::handle(Event * e)
{
    bb_->handle_re_synchronize(e);
}

void BTTRXTOTimer::handle(Event * e)
{
    bb_->turn_off_trx((BTTRXoffEvent *) e);
}

// Slave send packet in non-connection state
void BTslaveSendnonConnHandler::handle(Event * e)
{
    bb_->slave_send_nonconn((Packet *) e);
}

void BTChangeStHander::handle(Event * e)
{
    bb_->clearStateLock();
    bb_->change_state(((Baseband::ChangeStEvent *) e)->getState());
}

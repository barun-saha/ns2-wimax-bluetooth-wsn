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

#include "bt-lossmod.h"
#include "wnode.h"
#include "bt-node.h"
#include "hdr-bt.h"
#include "random.h"

bool BTLossMod::lost(WNode * nd, hdr_bt * bh)
{
    // Lost if far away
    return (nd->distance(bh->X_, bh->Y_, bh->Z_) > nd->radioRange_);
}

bool BTLossMod::collide(BTNode * nd, hdr_bt * bh)
{
    return (nd->collisionDist_ <= 0.0 ? false :
	    BTChannel::interfQ(bh->fs_).collide(nd, bh));
}

bool LMBlueHoc::lost(WNode * nd, hdr_bt * bh)
{
    int dist = (int) nd->distance(bh->X_, bh->Y_, bh->Z_);

    if (dist > nd->radioRange_) {	// too far away
	return 1;
    }

    if (bh->type == hdr_bt::Id) {	// Id packet is supposed to be robust.
	return 0;
    }

    dist -= 2;
    if (dist < 0) {
	dist = 0;
    } else if (dist > 22) {
	return 1;
    }

    hdr_bt::packet_type type = bh->type;
    switch (type) {
    case hdr_bt::EV4:		// need work
    case hdr_bt::EV5:
    case hdr_bt::EV3:
    case hdr_bt::EV3_2:
    case hdr_bt::EV3_3:
    case hdr_bt::EV5_2:
    case hdr_bt::EV5_3:
	type = hdr_bt::HV1;
	break;

    case hdr_bt::DH1_2:
    case hdr_bt::DH1_3:
    case hdr_bt::HLO:
	type = hdr_bt::DH1;
	break;

    case hdr_bt::DH3_2:
    case hdr_bt::DH3_3:
	type = hdr_bt::DH3;
	break;

    case hdr_bt::DH5_2:
    case hdr_bt::DH5_3:
	type = hdr_bt::DH5;
	break;

    default:
	break;
    }

    double ran = Random::uniform();

    return (ran < FERvsDist[type][dist]);
}

bool LMCoChBlueHoc::collide(BTNode * nd, hdr_bt * bh)
{
    return BTChannel::interfQ(bh->fs_).cochannel_collide(nd, bh);
}

//////////////////////////////////////////////////////////
//                                                      //
//                  BTInterferQue                       //
//                                                      //
//////////////////////////////////////////////////////////
int BTInterferQue::add(int s, double x, double y, double z, double st,
		       double et)
{
    _q[_head].sender_ = s;
    _q[_head].collided_ = 0;
    _q[_head].X_ = x;
    _q[_head].Y_ = y;
    _q[_head].Z_ = z;
    _q[_head].st = st;
    _q[_head].et = et;

    int numCollided = 0;
    int ind = _head;
    while (ind != _tail) {
	if (--ind < 0) {
	    ind = BTInterferQueMaxLen - 1;
	}
	if (_q[ind].st + MAX_PKT_TX_TIME < st) {
	    _tail = ind + 1;
	    if (_tail == BTInterferQueMaxLen) {
		_tail = 0;
	    }
	    break;
	}
	if (_q[ind].et > st) {
	    numCollided++;
	    _q[_head].collided_ = 1;
	    if (_q[ind].collided_ == 0) {
		_q[ind].collided_ = 1;
		numCollided++;
	    }
	    break;
	}
    }
    if (++_head == BTInterferQueMaxLen) {
	_head = 0;
    }
    if (_head == _tail) {	// overflow
	fprintf(stderr, "!!! **** !!! interfereQ overflow.\n");
	if (++_tail == BTInterferQueMaxLen) {
	    _tail = 0;
	}
    }
    return numCollided;
}

int BTInterferQue::collide(BTNode * nd, hdr_bt * bh)
{
    if (_head == _tail) {
	return 0;
    }

    double pst = bh->ts_;
    double pet = bh->ts_ + bh->txtime();

    // purge old entries
    for (;;) {
	if (_q[_tail].et + MAX_PKT_TX_TIME < pet) {
	    if (++_tail == BTInterferQueMaxLen) {
		_tail = 0;
	    }
	    if (_head == _tail) {
		return 0;
	    }
	} else {
	    break;
	}
    }

    int ind = _tail;
    while (ind != _head) {
	if (bh->sender != _q[ind].sender_
	    && !(pst >= _q[ind].et || pet <= _q[ind].st)) {
	    double interfsrc =
		nd->distance(_q[ind].X_, _q[ind].Y_, _q[ind].Z_);
	    double pktsrc = nd->distance(bh->X_, bh->Y_, bh->Z_);
	    if (interfsrc < nd->collisionDist_) {
		if (nd->bb_->trace_all_rx_ || nd->bb_->trace_me_rx_) {
		    bh->dump(BtStat::log_, BTCOLLISONPREFIX,
			     nd->bb_->bd_addr_, nd->bb_->state_str_s());
		    fprintf(BtStat::log_,
			    "  %d i:%d (%f %f) i-d:%f p-d:%f\n",
			    nd->bb_->bd_addr_, _q[ind].sender_, _q[ind].st,
			    _q[ind].et, interfsrc, pktsrc);
		}
		return 1;
	    }
	}

	if (++ind == BTInterferQueMaxLen) {
	    ind = 0;
	}
    }

    return 0;
}

/**
 * The following 2 functions were contributed by Guanhua Yan on 
 * Feb 21, 2006. It incorporates a simple
 * propagation model and drops packets when co-channel interference
 * exceeds 11dB, the threshold given in the spec.
 */
int BTInterferQue::cochannel_collide(BTNode * nd, hdr_bt * bh)
{
    if (_head == _tail) {
	return 0;
    }

    double pst = bh->ts_;
    double pet = bh->ts_ + bh->txtime();

    // purge old entries
    for (;;) {
	if (_q[_tail].et + MAX_PKT_TX_TIME < pet) {
	    if (++_tail == BTInterferQueMaxLen) {
		_tail = 0;
	    }
	    if (_head == _tail) {
		return 0;
	    }
	} else {
	    break;
	}
    }

    // compute the received power from the packet sender
    double pkt_dist = nd->distance(bh->X_, bh->Y_, bh->Z_);
    double rcvd_power = -path_loss(pkt_dist);
    // double dist_thresh = max_distance(rcvd_power - 11);
    double interference = 0;
    // double thresh = rcvd_power - 11;
    // double total_d = 0;

    // printf("pkt_dist = %f rcvd_power = %f dist_thresh = %f\n",
    //   pkt_dist, rcvd_power, dist_thresh);
    bool no_interference = true;

    int ind = _tail;
    while (ind != _head) {
	if (bh->sender != _q[ind].sender_
	    && ((pst >= _q[ind].st && pst < _q[ind].et)
		|| (pet > _q[ind].st && pet <= _q[ind].et))) {
	    double interfsrc =
		nd->distance(_q[ind].X_, _q[ind].Y_, _q[ind].Z_);

	    // if the device is sending, the received packet should be
	    // dropped.
	    if (interfsrc <= 0.000001)
		return 1;

	    interference += -path_loss(interfsrc);
	    no_interference = false;

	    // if (interfsrc <= dist_thresh)
	    // if (interference >= thresh)
	    //   return 1;
	}
	if (++ind == BTInterferQueMaxLen) {
	    ind = 0;
	}
    }

    if (no_interference) {
	return 0;
    } else if (interference >= 0) {	// interference too much...
	return 1;
    } else if (rcvd_power >= 0) {
	return 0;
    }

    double c_i_ratio;
    c_i_ratio = rcvd_power / interference;
    c_i_ratio = 10 * log(c_i_ratio) / log(10);

    return (c_i_ratio <= 11);
}

double BTInterferQue::path_loss(double distance)
{
    double p = 0;
    if (distance > 8) {
	p = 58.3 + 33 * log(distance / 8) / log(10);
    } else if (distance <= 0.00001) {
	p = 0;
    } else {
	p = 20 * log(4 * 3.1416 * distance / 0.1224) / log(10);
    }
    return p;
}

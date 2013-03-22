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


#ifndef __statis_h__
#define __statis_h__

#include <stdlib.h>
#include <stdio.h>

// #define BTMAXHOPS 128
// #define BTMAXPKTNUM 12800


class WNode;
class Statistic {
  public:
    struct FlowRecEntry {
	int bytes_;
	int numPkt_;
	int numPktLoss_;
	int numPktLossLastHop_;

	double delay_;
	double delayPower2_;
	double minDelay_;
	double maxDelay_;
	int hops_;		// hops may change if routing changes dynamically
	// int numDelay_;  // == numPkt_;
	double delayPerhop_;


	 FlowRecEntry():bytes_(0), numPkt_(0), 
	    numPktLoss_(0), numPktLossLastHop_(0),
	    delay_(0), 
	    delayPower2_(0), minDelay_(0), maxDelay_(0),
	    hops_(0), 
	    delayPerhop_(0)
	{}
    };

    struct FlowRec {
	int addr_;
	int port_;
	int isSrc_;
	int isDst_;
	int size_;
	int srcSeq_;

	int seq_;
	int seq_ini_;
	int numPktLoss_;

	int seqLastHop_;
	int seqLastHop_ini_;
	int numPktLossLastHop_;

	int numPktRecv_;
	double hops_;
	double minDelay_;
	double maxDelay_;
	double delay_;
	double delayStdDev_;

	FlowRecEntry *unit_;

	FlowRec(int addr, int port, int size)
	:addr_(addr), port_(port), isSrc_(0), isDst_(0), size_(size), 
	    srcSeq_(0), seq_(0), seq_ini_(0), numPktLoss_(0),
	    seqLastHop_(0), seqLastHop_ini_(0), numPktLossLastHop_(0),
	    numPktRecv_(0),
	    hops_(0), minDelay_(0), maxDelay_(0), delay_(0), delayStdDev_(0)
	{
	    unit_ = new FlowRecEntry[size];
	}
    };

    Statistic(double b, double e, double s, int ad, WNode * nd);
    virtual ~ Statistic();
    void reset(double b, double e, double s, int ad);

    static void init_logfiles();
    void dump(int noprint = 0);
    void send(int nbytes, int dst, int port, int *hopcnt, double *flow_ts,
	      int *flow_seq, double *flow_ts_lasthop,
	      int *flow_seq_lasthop, int isSrc);
    void recv(int nbytes, int dst, int dport, int hopcnt, double flow_ts,
	      int flow_seq, double flow_ts_lasthop, int flow_seq_lasthop);

    FlowRec *addFlowRec(int addr, int port);
    FlowRec *getFlowRec(int addr, int port);
    void sortFlowRec();
    int getMaxIndex();

    // protected:
  public:
    WNode * node_;
    int addr_;
    int isDst_;
    int hasTraffic_;
    int proto_adj_;		// ttl recv'd = ch->size() + proto_adj_
    double maxRate_;

    int size_;

    double begin_;
    double end_;
    double step_;

    double nextstop_;
    int index_;

    static FILE *log_;
    static FILE *logstat_;
    static FILE *logtoscreen_;

    FILE *flowLog_;		// for indivial nodes
    int trace_me_flow_stat_;

    static int trace_all_node_stat_;
    int trace_me_node_stat_;

    int numFlow_;
    int maxNumFlow_;
    FlowRec **flowRec_;

    double stat_ts_;

    double ttlTime_;
    int ttlRecvd_;
    int ttlHop_;
    double ttlDelay_;
    double ttlDelayPower2_;
    double ttlMinDelay_;
    double ttlMaxDelay_;
    double ttlDelayPerhop_;
    int ttlNumRecvPkt;
    int ttlNumPkt_;
    int ttlNumPktLoss_;
    int ttlNumPktLastHop_;
    int ttlNumPktLossLastHop_;
};

class StatisticByPkt : public Statistic {
  public:
    StatisticByPkt(double b, double e, double s, int ad, WNode * nd);

  public:
    FILE *rawFile_;
};

#endif

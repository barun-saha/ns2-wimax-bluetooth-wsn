/*
 * Copyright (c) 2005, University of Cincinnati, Ohio.
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
 * statis.cc
 */

//** Added by Barun; for MIN/MAX
#include <sys/param.h>

#include "statis.h"
#include <string.h>
#include "scheduler.h"
#include "wnode.h"

static class StatisticInit {
  public:
    StatisticInit() {
	Statistic::init_logfiles();
}} statisticinit;

FILE *Statistic::logtoscreen_ = NULL;
FILE *Statistic::logstat_ = NULL;
FILE *Statistic::log_ = NULL;
int Statistic::trace_all_node_stat_ = 0;

//double Statistic::delay_[BTMAXHOPS][BTMAXPKTNUM];
//int Statistic::delay_cnt_[BTMAXHOPS];
//int Statistic::sinit_ = 0;

Statistic::Statistic(double b, double e, double s, int ad, WNode * nd)
:node_(nd)
{
    trace_me_node_stat_ = 0;
    trace_me_flow_stat_ = 0;
    flowLog_ = stdout;

    proto_adj_ = 0;

    // reset everything else
    maxNumFlow_ = 0;
    flowRec_ = 0;
    reset(b, e, s, ad);
}

Statistic::~Statistic()
{
    for (int i = 0; i < maxNumFlow_; i++) {
	delete flowRec_[i];
    }
    delete[]flowRec_;
}

void Statistic::reset(double b, double e, double s, int ad)
{
    for (int i = 0; i < maxNumFlow_; i++) {
	delete flowRec_[i];
    }
    if (flowRec_) {
	delete[]flowRec_;
    }
    flowRec_ = 0;
    numFlow_ = 0;
    maxNumFlow_ = 0;

    isDst_ = 0;
    hasTraffic_ = 0;
    stat_ts_ = -1;

    addr_ = ad;
    begin_ = b;
    end_ = e;
    step_ = s;

    nextstop_ = begin_ + step_;
    index_ = 0;
    size_ = (step_ == 0 ? 0 : (int) ((end_ - begin_) / step_));
}

void Statistic::init_logfiles()
{
    static int flag = 0;
    if (flag == 0) {
	flag = 1;
	logtoscreen_ = stderr;
	logstat_ = stdout;
	log_ = stdout;
    }
}

Statistic::FlowRec * Statistic::addFlowRec(int addr, int port)
{
    int idx;
    if (numFlow_ == maxNumFlow_) {
	maxNumFlow_++;
	FlowRec **n_flowRec_ = new FlowRec *[maxNumFlow_];
	for (int i = 0; i < numFlow_; i++) {
	    n_flowRec_[i] = flowRec_[i];
	}
	n_flowRec_[numFlow_] = 0;
	delete[]flowRec_;
	flowRec_ = n_flowRec_;
	idx = numFlow_;
    } else {
	for (int i = 0; i < maxNumFlow_; i++) {
	    if (flowRec_[i] == 0) {
		idx = i;
		break;
	    }
	}
    }
    numFlow_++;
    if (port == 255) {
	addr = -1;
    }
    FlowRec *ret = flowRec_[idx] = new FlowRec(addr, port, size_);
    sortFlowRec();
    return ret;
}

Statistic::FlowRec * Statistic::getFlowRec(int addr, int port)
{
    int i;
    for (i = 0; i < maxNumFlow_; i++) {
	if (flowRec_[i] && (flowRec_[i]->port_ == port)
	    && (flowRec_[i]->addr_ == addr || port == 255)) {
	    return flowRec_[i];
	}
    }
    return addFlowRec(addr, port);
}

void Statistic::sortFlowRec()
{
    int i;
    for (i = 0; i < maxNumFlow_; i++) {
	if (flowRec_[i] && (flowRec_[i]->port_ == 255)) {
	    flowRec_[i]->addr_ = -1;
	    break;
	}
    }

    // buble sort
    for (i = 0; i < maxNumFlow_; i++) {
	if (!flowRec_[i]) {
	    continue;
	}
	for (int j = i + 1; j < maxNumFlow_; j++) {
	    if (!flowRec_[j]) {
		continue;
	    }
	    if (flowRec_[j]->addr_ < flowRec_[i]->addr_ ||
		(flowRec_[j]->addr_ == flowRec_[i]->addr_ &&
		 flowRec_[j]->port_ < flowRec_[i]->port_)) {
		FlowRec *tmp = flowRec_[j];
		flowRec_[j] = flowRec_[i];
		flowRec_[i] = tmp;
	    }
	}
    }
}

// The source will set flow seqno, time stamp and clear hopcount.
// An intermediate node will update hopcount.
void Statistic::send(int nbytes, int dst, int port, int *hopcnt,
		     double *flow_ts, int *flow_seq,
		     double *flow_ts_lasthop, int *flow_seq_lasthop,
		     int isSrc)
{
    double now = Scheduler::instance().clock();
    FlowRec *rec = getFlowRec(dst, port);

    hasTraffic_ = 1;
    if (isSrc) {
	// if (port != 255) {   // exclude rt pkt
	rec->isSrc_ = 1;
	*hopcnt = 0;
	*flow_ts = now;
	*flow_ts_lasthop = now;
	*flow_seq = rec->srcSeq_++;
	*flow_seq_lasthop = *flow_seq;

/*
	recv(nbytes, dst, port, *hopcnt, *flow_ts, *flow_seq,
	     *flow_ts_lasthop, *flow_seq_lasthop);
*/

	if (now < begin_ || index_ >= size_) {
	    return;
	}
	// Check if long period has passed without data
	while (now >= nextstop_) {
	    if (++index_ == size_) {
		return;
	    }
	    nextstop_ += step_;
	}
	rec->unit_[index_].bytes_ += (nbytes + proto_adj_);

    } else {
	(*hopcnt)++;
	*flow_ts_lasthop = now;
	*flow_seq_lasthop = rec->srcSeq_++;
    }
}

void Statistic::recv(int nbytes, int dst, int dport, int hopcnt,
		     double flow_ts, int flow_seq, double flow_ts_lasthop,
		     int flow_seq_lasthop)
{
    double now = Scheduler::instance().clock();

    FlowRec *rec = getFlowRec(dst, dport);
    int numPktLoss = 0;
    int numPktLossLastHop = 0;

    if (flow_seq == rec->seq_) {
	rec->seq_++;
    } else if (flow_seq > rec->seq_) {
	numPktLoss = (flow_seq - rec->seq_);
	rec->numPktLoss_ += numPktLoss;
	rec->seq_ = flow_seq + 1;
    } else {
/*
	fprintf(stderr, "flow_seq %d < %d.  Somthing wrong.\n", 
		flow_seq, rec->seq_);
*/
    }

    if (flow_seq_lasthop == rec->seqLastHop_) {
	rec->seqLastHop_++;
    } else if (flow_seq_lasthop > rec->seqLastHop_) {
	numPktLossLastHop = flow_seq_lasthop - rec->seqLastHop_;
	rec->numPktLossLastHop_ += numPktLossLastHop;
	rec->seqLastHop_ = flow_seq_lasthop + 1;
    }

    if (now < begin_) {
	rec->seq_ini_ = rec->seq_;
	rec->seqLastHop_ini_ = rec->seqLastHop_;
	return;
    } else if (index_ >= size_) {
	return;
    }

    hasTraffic_ = 1;
    if (dst == addr_ && dport != 255) {
	isDst_ = 1;
    }
    // Check if long period has passed without data
    while (now >= nextstop_) {
	if (++index_ == size_) {
	    return;
	}
	nextstop_ += step_;
    }

    rec->unit_[index_].bytes_ += (nbytes + proto_adj_);
    rec->unit_[index_].numPkt_++;
    rec->unit_[index_].delayPerhop_ += (now - flow_ts_lasthop);
    rec->unit_[index_].delay_ += (now - flow_ts);
    rec->unit_[index_].hops_ += (hopcnt + 1);

    rec->unit_[index_].delayPower2_ += (now - flow_ts) * (now - flow_ts);
    rec->unit_[index_].minDelay_ =
	(rec->unit_[index_].minDelay_ == 0 ? (now - flow_ts) :
	 MIN((now - flow_ts), rec->unit_[index_].minDelay_));
    rec->unit_[index_].maxDelay_ =
	MAX((now - flow_ts), rec->unit_[index_].maxDelay_);

    rec->unit_[index_].numPktLoss_ += numPktLoss;
    rec->unit_[index_].numPktLossLastHop_ += numPktLossLastHop;
}

int Statistic::getMaxIndex()
{
    int ind = 0;
    WNode *wk = node_;
    do {
	if (wk->stat_->index_ > ind) {
	    ind = wk->stat_->index_;
	}
    } while ((wk = wk->getNext()) != node_);
    return ind;
}

/*
 *   per node statistis (flows) format:
 *    	  <flow 0> ad port seq seqini pktLoss seqLastHop seqLastHopIni 
			pktLossLasthop size ttltime <TAB>
      	  <flow 1> ....
       	  ...
      	  <flow n>

	| <step 0> <flow 0> ... <flow n>
	| <step 1> <flow 0> ... <flow n>
	...
	| <step m> ...
 */

void Statistic::dump(int noprint)
{
    int i, j;
    int sum = 0;
    int size = getMaxIndex();
    double nstep = step_ * size;

    int numPktGross = 0;	// ttl pkt
    int numPktLossGross = 0;

    int numPkt = 0;		// ttl pkt after begin_
    int numPktLoss = 0;
    int numPktLastHop = 0;	// previous hop
    int numPktLossLastHop = 0;

    int numRecvPkt = 0;
    int numHops = 0;
    double delay = 0;
    double delayPower2 = 0;	// X^2
    double minDelay = 10e10;
    double maxDelay = 0;

    double delayPerhop = 0;

    for (j = 0; j < maxNumFlow_; j++) {
	if (!flowRec_[j]) {
	    continue;
	}
	if (!flowRec_[j]->isSrc_ && flowRec_[j]->addr_ >= 0) {
	    numPktGross += flowRec_[j]->seq_;
	    numPkt += (flowRec_[j]->seq_ - flowRec_[j]->seq_ini_);
	    numPktLossGross += flowRec_[j]->numPktLoss_;
	    numPktLastHop +=
		(flowRec_[j]->seqLastHop_ - flowRec_[j]->seqLastHop_ini_);
	}

	if (trace_me_flow_stat_ && !noprint) {
	    fprintf(flowLog_, "%d %d %d %d %d %d %d %d %d %f",
		    flowRec_[j]->addr_, flowRec_[j]->port_,
		    flowRec_[j]->seq_, flowRec_[j]->seq_ini_,
		    flowRec_[j]->numPktLoss_,
		    flowRec_[j]->seqLastHop_,
		    flowRec_[j]->seqLastHop_ini_,
		    flowRec_[j]->numPktLossLastHop_, size, nstep);
	    if (j < maxNumFlow_ - 1) {
		fprintf(flowLog_, "\t");
	    }
	}

	if (flowRec_[j]->isSrc_) {
	    continue;
	}

	int flowSum = 0;
	int flowNumRecvPkt = 0;
	int flowNumPktLoss = 0;
	int flowNumPktLossLastHop = 0;

	int flownumHops = 0;
	double flowDelay = 0;
	double flowDelayPower2 = 0;
	double flowMinDelay = 10e10;
	double flowMaxDelay = 0;

	double flowDelayPerhop = 0;

	for (i = 0; i < size; i++) {
	    flowSum += flowRec_[j]->unit_[i].bytes_;
	    flowNumRecvPkt += flowRec_[j]->unit_[i].numPkt_;
	    if (flowRec_[j]->addr_ >= 0) {
		flowNumPktLoss += flowRec_[j]->unit_[i].numPktLoss_;
		flowNumPktLossLastHop +=
		    flowRec_[j]->unit_[i].numPktLossLastHop_;
	    }

	    flowDelay += flowRec_[j]->unit_[i].delay_;
	    flowDelayPower2 += flowRec_[j]->unit_[i].delayPower2_;
	    flowMinDelay =
		MIN(flowMinDelay, flowRec_[j]->unit_[i].minDelay_);
	    flowMaxDelay =
		MAX(flowMaxDelay, flowRec_[j]->unit_[i].maxDelay_);
	    flownumHops += flowRec_[j]->unit_[i].hops_;

	    flowDelayPerhop += flowRec_[j]->unit_[i].delayPerhop_;
	}

	flowRec_[j]->numPktRecv_ = flowNumRecvPkt;
	if (flowNumRecvPkt > 0) {
	    flowRec_[j]->hops_ = double (flownumHops) / flowNumRecvPkt;
	    flowRec_[j]->minDelay_ = flowMinDelay;
	    flowRec_[j]->maxDelay_ = flowMaxDelay;
	    flowRec_[j]->delay_ = flowDelay / flowNumRecvPkt;
	    double dev = flowDelayPower2 / flowNumRecvPkt - 
			 flowRec_[j]->delay_ * flowRec_[j]->delay_;
	    // may have float point inaccuracy.
	    flowRec_[j]->delayStdDev_ = (dev <= 0 ? 0 : sqrt(dev));

	    sum += flowSum;
	    numRecvPkt += flowNumRecvPkt;
	    numHops += flownumHops;
	    delayPerhop += flowDelayPerhop;
	    delay += flowDelay;
	    delayPower2 += flowDelayPower2;	// with really dubious meaning
	    minDelay = MIN(minDelay, flowMinDelay);
	    maxDelay = MIN(maxDelay, flowMaxDelay);
	    numPktLoss += flowNumPktLoss;
	    numPktLossLastHop += flowNumPktLossLastHop;
	}
    }

    stat_ts_ = Scheduler::instance().clock();

    ttlTime_ = nstep;
    ttlRecvd_ = sum;
    ttlHop_ = numHops;
    ttlDelay_ = delay;

    // XXX this sum is meaning for one a single flow, to compute
    // stardard deviation of delay, ie, delay-jilter.
    // summing alltogether is just meaningless.
    // Well, sometimes, there is only a single flow.
    ttlDelayPower2_ = delayPower2;	// with really dubious meaning

    ttlMinDelay_ = minDelay;
    ttlMaxDelay_ = maxDelay;
    ttlDelayPerhop_ = delayPerhop;
    ttlNumRecvPkt = numRecvPkt;
    ttlNumPkt_ = numPkt;
    ttlNumPktLoss_ = numPktLoss;
    ttlNumPktLastHop_ = numPktLastHop;
    ttlNumPktLossLastHop_ = numPktLossLastHop;

    if (noprint) {
	return;
    }

    for (i = 0; i < size; i++) {
	if (trace_me_flow_stat_) {
	    fprintf(flowLog_, " | ");
	}

	for (j = 0; j < maxNumFlow_; j++) {
	    if (flowRec_[j]) {

		if (trace_me_flow_stat_ && !noprint) {
		    fprintf(flowLog_, "%d %d %d %d %f %f %f %f %d %f",
			    flowRec_[j]->unit_[i].bytes_,
			    flowRec_[j]->unit_[i].numPkt_,
			    flowRec_[j]->unit_[i].numPktLoss_,
			    flowRec_[j]->unit_[i].numPktLossLastHop_,
			    flowRec_[j]->unit_[i].delay_,
			    flowRec_[j]->unit_[i].delayPower2_,
			    flowRec_[j]->unit_[i].minDelay_,
			    flowRec_[j]->unit_[i].maxDelay_,
			    flowRec_[j]->unit_[i].hops_,
			    flowRec_[j]->unit_[i].delayPerhop_);
		    if (j < maxNumFlow_ - 1) {
			fprintf(flowLog_, "\t");
		    }
		}
	    }
	}
    }
    if (trace_me_flow_stat_) {
	fprintf(flowLog_, "\n");
    }

    double rateByte = (nstep == 0 ? 0 : sum / nstep);
    double rateBit = (nstep == 0 ? 0 : sum * 8 / nstep);
    double eff = rateBit / maxRate_;
    double aveDelay = (numHops == 0 ? 0 : delay / numHops);
    double aveDelayHopByHop =
	(numRecvPkt == 0 ? 0 : delayPerhop / numRecvPkt);
    double loss = (numPkt == 0 ? 1 : double (numPktLoss) / numPkt);
    double lossLastHop =
	(numPktLastHop ==
	 0 ? 1 : double (numPktLossLastHop) / numPktLastHop);
    double lossGross =
	(numPktGross == 0 ? 1 : double (numPktLossGross) / numPktGross);

	// Barun
    //fprintf(logtoscreen_,
	    //"%d %.2f (%d/%.2f) %.3f (%.2f/%.2f)"
	    //" d: %.2f %.2f l: %.2f %.2f %.2f\n", addr_, rateByte, sum,
	    //nstep, eff, rateBit, maxRate_, aveDelay, aveDelayHopByHop,
	    //loss, lossLastHop, lossGross);

    fprintf(log_,
	    "%d %.2f (%d/%.2f) %.3f (%.2f/%.2f)"
	    " d: %.2f %.2f l: %.2f %.2f %.2f\n", addr_, rateByte, sum,
	    nstep, eff, rateBit, maxRate_, aveDelay, aveDelayHopByHop,
	    loss, lossLastHop, lossGross);

    if (trace_all_node_stat_ || trace_me_node_stat_) {
	if (rateBit == 0) {
	    fprintf(logstat_, "0\t");
	} else {
	    fprintf(logstat_, "%f %f %f %f %f\t", rateBit, aveDelay,
		    aveDelayHopByHop, 1 - loss, 1 - lossLastHop);
	}
    }
}

StatisticByPkt::StatisticByPkt(double b, double e, double s, int ad,
			       WNode * nd)
:Statistic(b, e, s, ad, nd)
{
    rawFile_ = stdout;
}

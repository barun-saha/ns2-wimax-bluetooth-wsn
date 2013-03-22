/* -*-   Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) Xerox Corporation 1997. All rights reserved.
 *  
 * License is granted to copy, to use, and to make and to use derivative
 * works for research and evaluation purposes, provided that Xerox is
 * acknowledged in all documentation pertaining to any such copy or derivative
 * work. Xerox grants no other licenses expressed or implied. The Xerox trade
 * name should not be used in any advertising without its written permission.
 *  
 * XEROX CORPORATION MAKES NO REPRESENTATIONS CONCERNING EITHER THE
 * MERCHANTABILITY OF THIS SOFTWARE OR THE SUITABILITY OF THIS SOFTWARE
 * FOR ANY PARTICULAR PURPOSE.  The software is provided "as is" without
 * express or implied warranty of any kind.
 *  
 * These notices must be retained in any copies of any part of this software.
 */

/*
   Modified by Claudio Cicconetti to model Weibull modelled traffic
      <claudio.cicconetti@iet.unipi.it>
   Dpt. of Information Engineering at the University of Pisa - ITALY
*/

#include <stdlib.h>
 
#include "random.h"
#include "trafgen.h"
#include "ranvar.h"


/* implement an on/off source with weibull distributed on and
 * off times.  parameterized by shape/scale weibull parameters,
 * burst rate and packet size
 */

class Weibull_Traffic : public TrafficGenerator {
 public:
   Weibull_Traffic();
   virtual double next_interval(int&);
   virtual void timeout();
 protected:
   void init();
   double talk_shape_;     /* Weibull distribution shape during talkspurt */
   double talk_scale_;     /* Weibull distribution scale during talkspurt */
   double idle_shape_;     /* Weibull distribution shape during silence */
   double idle_scale_;     /* Weibull distribution scale during silence */
   double rate_;      /* send rate during on time (bps) */
   double interval_;  /* packet inter-arrival time during burst (sec) */
   unsigned int rem_; /* number of packets left in current burst */

   WeibullRandomVariable talk_rv_;
   WeibullRandomVariable idle_rv_;

};


static class Weibull_TrafficClass : public TclClass {
 public:
   Weibull_TrafficClass() : TclClass("Application/Traffic/Weibull") {}
   TclObject* create(int, const char*const*) {
      return (new Weibull_Traffic());
   }
} class_weibull_traffic;

Weibull_Traffic::Weibull_Traffic() :
	talk_rv_(1.0, 1.0, 0.0), idle_rv_(1.0, 1.0, 1.0)
{
   bind("talk_shape_", talk_rv_.shapep());
   bind("talk_scale_", talk_rv_.scalep());
   bind("idle_shape_", idle_rv_.shapep());
   bind("idle_scale_", idle_rv_.scalep());
   bind_bw("rate_", &rate_);
   bind("packetSize_", &size_);
}

void Weibull_Traffic::init()
{
    /* compute inter-packet interval during bursts based on
    * packet size and burst rate.  then compute average number
    * of packets in a burst.
    */
   interval_ = (double)(size_ << 3)/(double)rate_;
   rem_ = 0;
   if (agent_)
      agent_->set_pkttype(PT_EXP); // use exponential traffic type tag
}

double Weibull_Traffic::next_interval(int& size)
{
   double t = interval_;

   if (rem_ == 0) {
      /* compute number of packets in next burst */
      rem_ = int(.5 + talk_rv_.value() / interval_);
      /* make sure we got at least 1 */
      if (rem_ == 0)
         rem_ = 1;
      /* start of an idle period, compute idle time */
      t += idle_rv_.value();
   }   
   rem_--;

   size = size_;
   return(t);
}

void Weibull_Traffic::timeout()
{
   if (! running_)
      return;

   /* send a packet */
   // The test tcl/ex/test-rcvr.tcl relies on the "NEW_BURST" flag being 
   // set at the start of any exponential burst ("talkspurt").  
   if (nextPkttime_ != interval_ || nextPkttime_ == -1) 
      agent_->sendmsg(size_, "NEW_BURST");
   else 
      agent_->sendmsg(size_);
   /* figure out when to send the next one */
   nextPkttime_ = next_interval(size_);
   /* schedule it */
   if (nextPkttime_ > 0)
      timer_.resched(nextPkttime_);
}




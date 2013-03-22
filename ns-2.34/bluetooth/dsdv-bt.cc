/*
 * Copyright (c) 1997 Regents of the University of California.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Ported from CMU/Monarch's code, nov'98 -Padma.
 *
 */

/*
 *	dsdv-bt.cc
 */

/*
 *  This code is derived from dsdv/dsdv.cc.  
 *				-qw
 */

#include "dsdv-bt.h"
#include "random.h"
#include <lmp.h>
#include <bt-node.h>
#include <cmu-trace.h>


static class DSDV_BTclass:public TclClass {
  public:
    DSDV_BTclass():TclClass("Agent/DSDV/BT") { }
    TclObject *create(int argc, const char *const *argv) {
	return new DSDV_BT();
    }
} class_rtProtoDSDV_bt;

int DSDV_BT::command(int argc, const char *const *argv)
{
    if (argc == 3) {
	if (strcmp(argv[1], "node") == 0) {
	    RoutingIF::node_ = (BTNode *) TclObject::lookup(argv[2]);
	    if (RoutingIF::node_ == 0) {
		return TCL_ERROR;
	    }
	    RoutingIF::node_->setRagent(this);
	    node_ = (BTNode *) RoutingIF::node_;
	    return TCL_OK;
	}
    }
    return DSDV_Agent::command(argc, argv);
}

nsaddr_t DSDV_BT::nextHop(nsaddr_t dst)
{
    rtable_ent *rt = table_->GetEntry(dst);
    return (rt ? rt->hop : -1);
}

void DSDV_BT::sendInBuffer(nsaddr_t dst)
{
    rtable_ent *rt = table_->GetEntry(dst);
    if (!rt) {
	fprintf(stderr, "Ooops, No route is found from %d to %d\n",
		node_->bb_->bd_addr_, dst);
	return;
    }
    rt->flag = CRTENT_UP;
    Packet *buf_pkt;

    if (!rt->q) {
	return;
    }
    while ((buf_pkt = rt->q->deque())) {
	recv(buf_pkt, 0);
    }
    delete rt->q;
    rt->q = NULL;
}

void DSDV_BT::addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag)
{
    double now = Scheduler::instance().clock();
    rtable_ent rte;
    bzero(&rte, sizeof(rte));

    rte.dst = dst;
    rte.hop = nexthop;
    rte.metric = 0;
    rte.seqnum = seqno_;
    seqno_ += 2;

    rte.advertise_ok_at = 0.0;	// can always advert ourselves
    rte.advert_seqnum = true;
    rte.advert_metric = true;
    rte.changed_at = now;
    rte.new_seqnum_at = now;
    rte.wst = 0;
    rte.timeout_event = 0;	// Don't time out our localhost :)

    rte.q = 0;			// Don't buffer pkts for self!

    table_->AddEntry(rte);
}

// dsdv.{h,cc} don't have this function.  Should we invoke something
// similar to DSDV_BT::processUpdate() ?
// Or invoke lost_link() ?
void DSDV_BT::delRtEntry(nsaddr_t nexthop)
{
}

void DSDV_BT::prapare_for_traffic(nsaddr_t dst)
{
    if (((BTNode *) node_)->lmp_->rpScheduler) {
        ((BTNode *) node_)->lmp_->rpScheduler->rpAdjustStart(dst);
        return;
    }
}


///////////////////////////////////////////////////////
// The following code are adapted from dsdv/dsdv.cc  //
///////////////////////////////////////////////////////

void
DSDV_BT::lost_link (Packet *p)
{
  hdr_cmn *hdrc = HDR_CMN (p);
  rtable_ent *prte = table_->GetEntry (hdrc->next_hop_);

  if(use_mac_ == 0) {
          drop(p, DROP_RTR_MAC_CALLBACK);
          return;
  }

  //DEBUG
  //printf("(%d)..Lost link..\n",myaddr_);
  if (verbose_ && hdrc->addr_type_ == NS_AF_INET)
    trace("VLL %.8f %d->%d lost at %d",
    Scheduler::instance().clock(),
	   hdr_ip::access(p)->saddr(), hdr_ip::access(p)->daddr(),
	   myaddr_);

  if (!use_mac_ || !prte || hdrc->addr_type_ != NS_AF_INET)
    return;

  if (verbose_)
    trace ("VLP %.5f %d:%d->%d:%d lost at %d [hop %d]",
  Scheduler::instance ().clock (),
	   hdr_ip::access (p)->saddr(),
	   hdr_ip::access (p)->sport(),
	   hdr_ip::access (p)->daddr(),
	   hdr_ip::access (p)->dport(),
	   myaddr_, prte->dst);

  if (prte->timeout_event)
    {
      Scheduler::instance ().cancel (prte->timeout_event);
      helper_callback (prte->timeout_event);
    }
  else if (prte->metric != BIG)
    {
      assert(prte->timeout_event == 0);
      prte->timeout_event = new Event ();
      helper_callback (prte->timeout_event);
    }

  // Queue these packets up...
  recv(p, 0);

#if 0
  while (p2 = ((PriQueue *) target_)->filter (prte->dst))
    {
      if (verbose_)
      trace ("VRS %.5f %d:%d->%d:%d lost at %d", Scheduler::instance ().clock (),
	       hdr_ip::access (p2)->saddr(),
	       hdr_ip::access (p2)->sport(),
	       hdr_ip::access (p2)->daddr(),
	       hdr_ip::access (p2)->dport(), myaddr_);

      recv(p2, 0);
    }

  while (p2 = ll_queue->filter (prte->dst))
    {
      if (verbose_)
      trace ("VRS %.5f %d:%d->%d:%d lost at %d", Scheduler::instance ().clock (),
	       hdr_ip::access (p2)->saddr(),
	       hdr_ip::access (p2)->sport(),
	       hdr_ip::access (p2)->daddr(),
	       hdr_ip::access (p2)->dport(), myaddr_);

      recv (p2, 0);
    }
#endif
}

static void
mac_callback (Packet * p, void *arg)
{
  ((DSDV_BT *) arg)->lost_link (p);
}

void
DSDV_BT::forwardPacket (Packet * p)
{
  hdr_ip *iph = HDR_IP(p);
  Scheduler & s = Scheduler::instance ();
  double now = s.clock ();
  hdr_cmn *hdrc = HDR_CMN (p);
  int dst;
  rtable_ent *prte;
  
  // We should route it.
  //printf("(%d)-->forwardig pkt\n",myaddr_);
  // set direction of pkt to -1 , i.e downward
  hdrc->direction() = hdr_cmn::DOWN;

  // if the destination is outside mobilenode's domain
  // forward it to base_stn node
  // Note: pkt is not buffered if route to base_stn is unknown

  dst = Address::instance().get_nodeaddr(iph->daddr());  
  if (diff_subnet(iph->daddr())) {
	   prte = table_->GetEntry (dst);
	  if (prte && prte->metric != BIG) 
		  goto send;
	  
	  //int dst = (node_->base_stn())->address();
	  // dst = node_->base_stn();
	  dst = -1;
	  prte = table_->GetEntry (dst);
	  if (prte && prte->metric != BIG) 
		  goto send;
	  
	  else {
		  //drop pkt with warning
		  fprintf(stderr, "warning: Route to base_stn not known: dropping pkt\n");
		  Packet::free(p);
		  return;
	  }
  }
  
  prte = table_->GetEntry (dst);


  
  //  trace("VDEBUG-RX %d %d->%d %d %d 0x%08x 0x%08x %d %d", 
  //  myaddr_, iph->src(), iph->dst(), hdrc->next_hop_, hdrc->addr_type_,
  //  hdrc->xmit_failure_, hdrc->xmit_failure_data_,
  //  hdrc->num_forwards_, hdrc->opt_num_forwards);

  if (prte && prte->metric != BIG)
    {
       //printf("(%d)-have route for dst\n",myaddr_);
       goto send;
    }
  else if (prte)
    { /* must queue the packet */
	    //printf("(%d)-no route, queue pkt\n",myaddr_);
      if (!prte->q)
	{
	  prte->q = new PacketQueue ();
	}

      prte->q->enque(p);

      if (verbose_)
	trace ("VBP %.5f _%d_ %d:%d -> %d:%d", now, myaddr_, iph->saddr(),
	       iph->sport(), iph->daddr(), iph->dport());

      while (prte->q->length () > MAX_QUEUE_LENGTH)
	      drop (prte->q->deque (), DROP_RTR_QFULL);
      return;
    }
  else
    { // Brand new destination
      rtable_ent rte;
      double now = s.clock();
      
      bzero(&rte, sizeof(rte));
      rte.dst = dst;
      rte.hop = dst;
      rte.metric = BIG;
      rte.seqnum = 0;

      rte.advertise_ok_at = now + 604800;	// check back next week... :)
      rte.changed_at = now;
      rte.new_seqnum_at = now;	// was now + wst0_, why??? XXX -dam
      rte.wst = wst0_;
      rte.timeout_event = 0;

      rte.q = new PacketQueue();
      rte.q->enque(p);
	  
      assert (rte.q->length() == 1 && 1 <= MAX_QUEUE_LENGTH);
      table_->AddEntry(rte);
      
      if (verbose_)
	      trace ("VBP %.5f _%d_ %d:%d -> %d:%d", now, myaddr_,
		     iph->saddr(), iph->sport(), iph->daddr(), iph->dport());
      return;
    }


 send:
  hdrc->addr_type_ = NS_AF_INET;
  hdrc->xmit_failure_ = mac_callback;
  hdrc->xmit_failure_data_ = this;
  if (prte->metric > 1)
	  hdrc->next_hop_ = prte->hop;
  else
	  hdrc->next_hop_ = dst;
  if (verbose_)
	  trace ("Routing pkts outside domain: \
VFP %.5f _%d_ %d:%d -> %d:%d", now, myaddr_, iph->saddr(),
		 iph->sport(), iph->daddr(), iph->dport());  

  assert (!HDR_CMN (p)->xmit_failure_ ||
	  HDR_CMN (p)->xmit_failure_ == mac_callback);

//ucbt-addon
#if 1
        if (prte->flag != CRTENT_UP
            && ((BTNode *) node_)->lmp_->rpScheduler
            && ((BTNode *) node_)->lmp_->rpScheduler->type()
                != RPSched::LPDRP
            && ((BTNode *) node_)->lmp_->rpScheduler->type()
                != RPSched::MDRP) {
            if (prte->flag == CRTENT_UNINI) {
                prte->flag = CRTENT_ININI;
                prapare_for_traffic(dst);
            }
	    if (!prte->q) {
		prte->q = new PacketQueue ();
	    }
	    prte->q->enque(p);
            return;
        }
#endif
// end ucbt-addon

  target_->recv(p, (Handler *)0);
  return;
  
}

void
DSDV_BT::processUpdate (Packet * p)
{
  hdr_ip *iph = HDR_IP(p);
  Scheduler & s = Scheduler::instance ();
  double now = s.clock ();
  
  // it's a dsdv packet
  int i;
  unsigned char *d = p->accessdata ();
  unsigned char *w = d + 1;
  rtable_ent rte;		// new rte learned from update being processed
  rtable_ent *prte;		// ptr to entry *in* routing tbl

  //DEBUG
  //int src, dst;
  //src = Address::instance().get_nodeaddr(iph->src());
  //dst = Address::instance().get_nodeaddr(iph->dst());
  //printf("Received DSDV packet from %d(%d) to %d(%d) [%d)]\n", src, iph->sport(), dst, iph->dport(), myaddr_);

  for (i = *d; i > 0; i--)
    {
      bool trigger_update = false;  // do we need to do a triggered update?
      nsaddr_t dst;
      prte = NULL;

      dst = *(w++);
      dst = dst << 8 | *(w++);
      dst = dst << 8 | *(w++);
      dst = dst << 8 | *(w++);

      if ((prte = table_->GetEntry (dst)))
	{
	  bcopy(prte, &rte, sizeof(rte));
	}
      else
	{
	  bzero(&rte, sizeof(rte));
	}

      rte.dst = dst;
      //rte.hop = iph->src();
      rte.hop = Address::instance().get_nodeaddr(iph->saddr());
      rte.metric = *(w++);
      rte.seqnum = *(w++);
      rte.seqnum = rte.seqnum << 8 | *(w++);
      rte.seqnum = rte.seqnum << 8 | *(w++);
      rte.seqnum = rte.seqnum << 8 | *(w++);
      rte.changed_at = now;
      if (rte.metric != BIG) rte.metric += 1;

      if (rte.dst == myaddr_)
	{
	  if (rte.metric == BIG && periodic_callback_)
	    {
	      // You have the last word on yourself...
	      // Tell the world right now that I'm still here....
	      // This is a CMU Monarch optimiziation to fix the 
	      // the problem of other nodes telling you and your neighbors
	      // that you don't exist described in the paper.
	      s.cancel (periodic_callback_);
	      s.schedule (helper_, periodic_callback_, 0);
	    }
	  continue;		// don't corrupt your own routing table.

	}

      /**********  fill in meta data for new route ***********/
      // If it's in the table, make it the same timeout and queue.
      if (prte)
	{ // we already have a route to this dst
	  if (prte->seqnum == rte.seqnum)
	    { // we've got an update with out a new squenece number
	      // this update must have come along a different path
	      // than the previous one, and is just the kind of thing
	      // the weighted settling time is supposed to track.

	      // this code is now a no-op left here for clarity -dam XXX
	      rte.wst = prte->wst;
	      rte.new_seqnum_at = prte->new_seqnum_at;
	    }
	  else 
	    { // we've got a new seq number, end the measurement period
	      // for wst over the course of the old sequence number
	      // and update wst with the difference between the last
	      // time we changed the route (which would be when the 
	      // best route metric arrives) and the first time we heard
	      // the sequence number that started the measurement period

	      // do we care if we've missed a sequence number, such
	      // that we have a wst measurement period that streches over
	      // more than a single sequence number??? XXX -dam 4/20/98
	      rte.wst = alpha_ * prte->wst + 
		(1.0 - alpha_) * (prte->changed_at - prte->new_seqnum_at);
	      rte.new_seqnum_at = now;
	    }
	}
      else
	{ // inititallize the wst for the new route
	  rte.wst = wst0_;
	  rte.new_seqnum_at = now;
	}
	  
      // Now that we know the wst_, we know when to update...
      if (rte.metric != BIG && (!prte || prte->metric != BIG))
	rte.advertise_ok_at = now + (rte.wst * 2);
      else
	rte.advertise_ok_at = now;

      /*********** decide whether to update our routing table *********/
      if (!prte)
	{ // we've heard from a brand new destination
	  if (rte.metric < BIG) 
	    {
	      rte.advert_metric = true;
	      trigger_update = true;
	    }
	  updateRoute(prte,&rte);
	}
      else if ( prte->seqnum == rte.seqnum )
	{ // stnd dist vector case
	  if (rte.metric < prte->metric) 
	    { // a shorter route!
	      if (rte.metric == prte->last_advertised_metric)
		{ // we've just gone back to a metric we've already advertised
		  rte.advert_metric = false;
		  trigger_update = false;
		}
	      else
		{ // we're changing away from the last metric we announced
		  rte.advert_metric = true;
		  trigger_update = true;
		}
	      updateRoute(prte,&rte);
	    }
	  else
	    { // ignore the longer route
	    }
	}
      else if ( prte->seqnum < rte.seqnum )
	{ // we've heard a fresher sequence number
	  // we *must* believe its rt metric
	  rte.advert_seqnum = true;	// we've got a new seqnum to advert
	  if (rte.metric == prte->last_advertised_metric)
	    { // we've just gone back to our old metric
	      rte.advert_metric = false;
	    }
	  else
	    { // we're using a metric different from our last advert
	      rte.advert_metric = true;
	    }

	  updateRoute(prte,&rte);

#ifdef TRIGGER_UPDATE_ON_FRESH_SEQNUM
	  trigger_update = true;
#else
	  trigger_update = false;
#endif
	}
      else if ( prte->seqnum > rte.seqnum )
	{ // our neighbor has older sequnum info than we do
	  if (rte.metric == BIG && prte->metric != BIG)
	    { // we must go forth and educate this ignorant fellow
	      // about our more glorious and happy metric
	      prte->advertise_ok_at = now;
	      prte->advert_metric = true;
	      // directly schedule a triggered update now for 
	      // prte, since the other logic only works with rte.*
	      needTriggeredUpdate(prte,now);
	    }
	  else
	    { // we don't care about their stale info
	    }
	}
      else
	{
	  fprintf(stderr,
		  "%s DFU: unhandled adding a route entry?\n", __FILE__);
	  abort();
	}
      
      if (trigger_update)
	{
	  prte = table_->GetEntry (rte.dst);
	  assert(prte != NULL && prte->advertise_ok_at == rte.advertise_ok_at);
	  needTriggeredUpdate(prte, prte->advertise_ok_at);
	}

      // see if we can send off any packets we've got queued
      if (rte.q && rte.metric != BIG)
	{
	  Packet *queued_p;

// Well we do observe loops here and I give a temp fix, not sure if it is
// correct according to DSDV semantics.
#if 0
	  while ((queued_p = rte.q->deque()))
	  // XXX possible loop here  
	  // while ((queued_p = rte.q->deque()))
	  // Only retry once to avoid looping
	  // for (int jj = 0; jj < rte.q->length(); jj++){
	  //  queued_p = rte.q->deque();
	    recv(queued_p, 0); // give the packets to ourselves to forward
	  // }
#else
	int qlen = rte.q->length();
	for (int jj = 0; jj < qlen; jj++) {
	    queued_p = rte.q->deque();
	    recv(queued_p, 0); // give the packets to ourselves to forward
	}
	while ((queued_p = rte.q->deque())) {
	    drop(queued_p);
	}
#endif
	  delete rte.q;
	  rte.q = 0;
	  table_->AddEntry(rte); // record the now zero'd queue
	}
    } // end of all destination mentioned in routing update packet

  // Reschedule the timeout for this neighbor
  prte = table_->GetEntry(Address::instance().get_nodeaddr(iph->saddr()));
  if (prte)
    {
      if (prte->timeout_event)
	s.cancel (prte->timeout_event);
      else
	{
	  prte->timeout_event = new Event ();
	}
      
      s.schedule (helper_, prte->timeout_event, min_update_periods_ * perup_);
    }
  else
    { // If the first thing we hear from a node is a triggered update
      // that doesn't list the node sending the update as the first
      // thing in the packet (which is disrecommended by the paper)
      // we won't have a route to that node already.  In order to timeout
      // the routes we just learned, we need a harmless route to keep the
      // timeout metadata straight.
      
      // Hi there, nice to meet you. I'll make a fake advertisement
      bzero(&rte, sizeof(rte));
      rte.dst = Address::instance().get_nodeaddr(iph->saddr());
      rte.hop = Address::instance().get_nodeaddr(iph->saddr());
      rte.metric = 1;
      rte.seqnum = 0;
      rte.advertise_ok_at = now + 604800;	// check back next week... :)
      
      rte.changed_at = now;
      rte.new_seqnum_at = now;
      rte.wst = wst0_;
      rte.timeout_event = new Event ();
      rte.q = 0;
      
      updateRoute(NULL, &rte);
      s.schedule(helper_, rte.timeout_event, min_update_periods_ * perup_);
    }
  
  /*
   * Freeing a routing layer packet --> don't need to
   * call drop here.
   */
  Packet::free (p);

}

void 
DSDV_BT::sendOutBCastPkt(Packet *p)
{
  // Scheduler & s = Scheduler::instance ();
  // send out bcast pkt with jitter to avoid sync  -- N/A for BT
  //s.schedule (target_, p, jitter(DSDV_BROADCAST_JITTER, be_random_));
  target_->recv(p, (Handler *) 0);
}


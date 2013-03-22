/*
Copyright (c) 1997, 1998 Carnegie Mellon University.  All Rights
Reserved. 

Permission to use, copy, modify, and distribute this
software and its documentation is hereby granted (including for
commercial or for-profit use), provided that both the copyright notice and this permission notice appear in all copies of the software, derivative works, or modified versions, and any portions thereof, and that both notices appear in supporting documentation, and that credit is given to Carnegie Mellon University in all publications reporting on direct or indirect use of this code or its derivatives.

ALL CODE, SOFTWARE, PROTOCOLS, AND ARCHITECTURES DEVELOPED BY THE CMU
MONARCH PROJECT ARE EXPERIMENTAL AND ARE KNOWN TO HAVE BUGS, SOME OF
WHICH MAY HAVE SERIOUS CONSEQUENCES. CARNEGIE MELLON PROVIDES THIS
SOFTWARE OR OTHER INTELLECTUAL PROPERTY IN ITS ``AS IS'' CONDITION,
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE OR
INTELLECTUAL PROPERTY, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

Carnegie Mellon encourages (but does not require) users of this
software or intellectual property to return any improvements or
extensions that they make, and to grant Carnegie Mellon the rights to redistribute these changes without encumbrance.

The AODV code developed by the CMU/MONARCH group was optimized and tuned by Samir Das and Mahesh Marina, University of Cincinnati. The work was partially done in Sun Microsystems. Modified for gratuitous replies by Anant Utgikar, 09/16/02.

*/


/*
 *  This code is derived from aodv/aodv.cc and aodv/aodv_logs.cc.  
 *
 *  The modification mainly changes some defines to class members so they can
 *  be changed.  Also 'ifqueue' is removed.	-qw
 */

/*
 *  Note: The original code indent is purposedly maintained to ensure changes
 *        easily identifiable.
 */

/*
 *	aodv-bt.cc
 */

#include "aodv-bt.h"
#include "aodv/aodv_packet.h"
#include "random.h"
#include <lmp.h>
#include <bt-node.h>


static class AODV_BTclass:public TclClass {
  public:
    AODV_BTclass():TclClass("Agent/AODV/BT") {} 

    TclObject *create(int argc, const char *const *argv) {
	assert(argc == 5);
#ifdef NS21B7A_
	nsaddr_t id = (nsaddr_t) atoi(argv[4]);
#else
	nsaddr_t id = (nsaddr_t) Address::instance().str2addr(argv[4]);
#endif
	return new AODV_BT(id);
    }
} class_rtProtoAODVBT;


AODV_BT::AODV_BT(nsaddr_t id)
:AODV(id), RoutingIF()
{
    MY_ROUTE_TIMEOUT = MY_ROUTE_TIMEOUT_bt;
    ACTIVE_ROUTE_TIMEOUT = ACTIVE_ROUTE_TIMEOUT_bt;
    REV_ROUTE_LIFE = REV_ROUTE_LIFE_bt;
    BCAST_ID_SAVE = BCAST_ID_SAVE_bt;
    MAX_RREQ_TIMEOUT = MAX_RREQ_TIMEOUT_bt;
}

int AODV_BT::command(int argc, const char *const *argv)
{
    if (argc == 3) {
	if (strcmp(argv[1], "node") == 0) {
	    node_ = (BTNode *) TclObject::lookup(argv[2]);
	    if (node_ == 0) {
		return TCL_ERROR;
	    }
	    RoutingIF::node_->setRagent(this);
	    return TCL_OK;
	}
    }
    return AODV::command(argc, argv);
}

nsaddr_t AODV_BT::nextHop(nsaddr_t dst)
{
    aodv_rt_entry *rt = rtable.rt_lookup(dst);
    return (rt ? rt->rt_nexthop : -1);
}

void AODV_BT::sendInBuffer(nsaddr_t dst)
{
    aodv_rt_entry *rt;
    Packet *buf_pkt;
    double delay = 0.0;

    rt = rtable.rt_lookup(dst);
    if (!rt) {
	return;
    }
    // assert(rt->rt_flags == RTF_WAIT_LINK_OPT);
    rt->rt_flags = RTF_UP;
    while ((buf_pkt = rqueue.deque(rt->rt_dst))) {
	// Delay them a little to help ARP. Otherwise ARP
	// may drop packets. -SRD 5/23/99
	forward(rt, buf_pkt, delay);
	delay += ARP_DELAY;
    }
}

void AODV_BT::addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag)
{
    aodv_rt_entry *rt = rtable.rt_lookup(dst);
    if (!rt) {
	rt = rtable.rt_add(dst);
    }
    rt_update(rt, 0, 0, nexthop, 0);
}

// remove all entry with this nexthop.
void AODV_BT::delRtEntry(nsaddr_t nexthop)
{
    aodv_rt_entry *rt = rtable.head();
    aodv_rt_entry *next;
    while(rt) {
	if (rt->rt_nexthop == nexthop) {
	    next = rt->rt_link.le_next;
	    LIST_REMOVE(rt, rt_link);
	    delete rt;
	    rt = next;
	} else {
	    rt = rt->rt_link.le_next;
	}
    }
}

void
AODV_BT::recvReply(Packet *p) {
//struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);
struct hdr_aodv_reply *rp = HDR_AODV_REPLY(p);
aodv_rt_entry *rt;
char suppress_reply = 0;
double delay = 0.0;
                                                                                
#ifdef DEBUG
// fprintf(stderr, "%d - %s: received a REPLY\n", index, __FUNCTION__);
#endif // DEBUG
                                                                                
                                                                                
 /*
  *  Got a reply. So reset the "soft state" maintained for
  *  route requests in the request table. We don't really have
  *  have a separate request table. It is just a part of the
  *  routing table itself.
  */
 // Note that rp_dst is the dest of the data packets, not the
 // the dest of the reply, which is the src of the data packets.
                                                                                
 rt = rtable.rt_lookup(rp->rp_dst);
                                                                                
 /*
  *  If I don't have a rt entry to this host... adding
  */
 if(rt == 0) {
   rt = rtable.rt_add(rp->rp_dst);
 }
                                                                                
 /*
  * Add a forward route table entry... here I am following
  * Perkins-Royer AODV paper almost literally - SRD 5/99
  */
                                                                                
 if ( (rt->rt_seqno < rp->rp_dst_seqno) ||   // newer route
      ((rt->rt_seqno == rp->rp_dst_seqno) &&
       (rt->rt_hops > rp->rp_hop_count)) ) { // shorter or better route
                                                                                
  // Update the rt entry
  rt_update(rt, rp->rp_dst_seqno, rp->rp_hop_count,
                rp->rp_src, CURRENT_TIME + rp->rp_lifetime);
                                                                                
  // reset the soft state
  rt->rt_req_cnt = 0;
  rt->rt_req_timeout = 0.0;
  rt->rt_req_last_ttl = rp->rp_hop_count;
                                                                                
if (ih->daddr() == index) { // If I am the original source
  // Update the route discovery latency statistics
  // rp->rp_timestamp is the time of request origination
                                                                                
    rt->rt_disc_latency[(signed char) rt->hist_indx] = (CURRENT_TIME - rp->rp_timestamp)
                                         / (double) rp->rp_hop_count;
    // increment indx for next time
    rt->hist_indx = (rt->hist_indx + 1) % MAX_HISTORY;
                                                                                
// #ifdef BTADDON
            /* begin BT addon */
            if (((BTNode *) node_)->lmp_->rpScheduler) {
                rt->rt_flags = RTF_WAIT_LINK_OPT;
                ((BTNode *) node_)->lmp_->rpScheduler->rpAdjustStart(rt->rt_dst);
                Packet::free(p);
                return;
            }
            /* end BT addon */
// #endif
                                                                                
  }
                                                                                
  /*
   * Send all packets queued in the sendbuffer destined for
   * this destination.
   * XXX - observe the "second" use of p.
   */
  Packet *buf_pkt;
  while((buf_pkt = rqueue.deque(rt->rt_dst))) {
    if(rt->rt_hops != INFINITY2) {
          assert (rt->rt_flags == RTF_UP);
    // Delay them a little to help ARP. Otherwise ARP
    // may drop packets. -SRD 5/23/99
      forward(rt, buf_pkt, delay);
      delay += ARP_DELAY;
    }
  }
 }
 else {
  suppress_reply = 1;
 }
                                                                                
 /*
  * If reply is for me, discard it.
  */
                                                                                
if(ih->daddr() == index || suppress_reply) {
   Packet::free(p);
 }
 /*
  * Otherwise, forward the Route Reply.
  */
 else {
 // Find the rt entry
aodv_rt_entry *rt0 = rtable.rt_lookup(ih->daddr());
   // If the rt is up, forward
   if(rt0 && (rt0->rt_hops != INFINITY2)) {
        assert (rt0->rt_flags == RTF_UP);
     rp->rp_hop_count += 1;
     rp->rp_src = index;
     forward(rt0, p, NO_DELAY);
     // Insert the nexthop towards the RREQ source to
     // the precursor list of the RREQ destination
     rt->pc_insert(rt0->rt_nexthop); // nexthop to RREQ source
                                                                                
   }
   else {
   // I don't know how to forward .. drop the reply.
#ifdef DEBUG
//     fprintf(stderr, "%s: dropping Route Reply\n", __FUNCTION__);
#endif // DEBUG
     drop(p, DROP_RTR_NO_ROUTE);
   }
 }
}


void
AODV_BT::recvError(Packet *p) {
struct hdr_ip *ih = HDR_IP(p);
struct hdr_aodv_error *re = HDR_AODV_ERROR(p);
aodv_rt_entry *rt;
u_int8_t i;
Packet *rerr = Packet::alloc();
struct hdr_aodv_error *nre = HDR_AODV_ERROR(rerr);

 nre->DestCount = 0;

 for (i=0; i<re->DestCount; i++) {
 // For each unreachable destination
   rt = rtable.rt_lookup(re->unreachable_dst[i]);
   if ( rt && (rt->rt_hops != INFINITY2) &&
	(rt->rt_nexthop == ih->saddr()) &&
     	(rt->rt_seqno <= re->unreachable_dst_seqno[i]) ) {
	assert(rt->rt_flags == RTF_UP);
	assert((rt->rt_seqno%2) == 0); // is the seqno even?
#ifdef DEBUG
     fprintf(stderr, "%s(%f): %d\t(%d\t%u\t%d)\t(%d\t%u\t%d)\n", __FUNCTION__,CURRENT_TIME,
		     index, rt->rt_dst, rt->rt_seqno, rt->rt_nexthop,
		     re->unreachable_dst[i],re->unreachable_dst_seqno[i],
	             ih->saddr());
#endif // DEBUG
     	rt->rt_seqno = re->unreachable_dst_seqno[i];
     	rt_down(rt);

   // Not sure whether this is the right thing to do
/*
   Packet *pkt;
	while((pkt = ifqueue->filter(ih->saddr()))) {
        	drop(pkt, DROP_RTR_MAC_CALLBACK);
     	}
*/
    node_->flushPkt(ih->saddr());

     // if precursor list non-empty add to RERR and delete the precursor list
     	if (!rt->pc_empty()) {
     		nre->unreachable_dst[nre->DestCount] = rt->rt_dst;
     		nre->unreachable_dst_seqno[nre->DestCount] = rt->rt_seqno;
     		nre->DestCount += 1;
		rt->pc_delete();
     	}
   }
 } 

 if (nre->DestCount > 0) {
#ifdef DEBUG
   fprintf(stderr, "%s(%f): %d\t sending RERR...\n", __FUNCTION__, CURRENT_TIME, index);
#endif // DEBUG
   sendError(rerr);
 }
 else {
   Packet::free(rerr);
 }

 Packet::free(p);
}

void
AODV_BT::rt_ll_failed(Packet *p) {
struct hdr_cmn *ch = HDR_CMN(p);
struct hdr_ip *ih = HDR_IP(p);
aodv_rt_entry *rt;
nsaddr_t broken_nbr = ch->next_hop_;

#ifndef AODV_LINK_LAYER_DETECTION
 drop(p, DROP_RTR_MAC_CALLBACK);
#else 

 /*
  * Non-data packets and Broadcast Packets can be dropped.
  */
  if(! DATA_PACKET(ch->ptype()) ||
     (u_int32_t) ih->daddr() == IP_BROADCAST) {
    drop(p, DROP_RTR_MAC_CALLBACK);
    return;
  }
  log_link_broke(p);
	if((rt = rtable.rt_lookup(ih->daddr())) == 0) {
    drop(p, DROP_RTR_MAC_CALLBACK);
    return;
  }
  log_link_del(ch->next_hop_);

#ifdef AODV_LOCAL_REPAIR
  /* if the broken link is closer to the dest than source, 
     attempt a local repair. Otherwise, bring down the route. */


  if (ch->num_forwards() > rt->rt_hops) {
    local_rt_repair(rt, p); // local repair
    // retrieve all the packets in the ifq using this link,
    // queue the packets for which local repair is done, 
    return;
  }
  else	
#endif // LOCAL REPAIR	

  {
    drop(p, DROP_RTR_MAC_CALLBACK);
/*
    // Do the same thing for other packets in the interface queue using the
    // broken link -Mahesh
while((p = ifqueue->filter(broken_nbr))) {
     drop(p, DROP_RTR_MAC_CALLBACK);
    }	
*/
    node_->flushPkt(broken_nbr);
    nb_delete(broken_nbr);
  }

#endif // LINK LAYER DETECTION
}

// -------------------------------------
// Replace the function in aodv_logs.cc.

// This static const is defined in ../aodv/aodv_logs.cc.  I don't want
// to touch that file, so define it here too.  Need to syn'd if you 
// want to turn verbose on.
static const int verbose = 0;	

void
AODV_BT::log_link_broke(Packet *p)
{
        static int link_broke = 0;
        struct hdr_cmn *ch = HDR_CMN(p);

        if(! logtarget || ! verbose) return;

        sprintf(logtarget->pt_->buffer(),
                "A %.9f _%d_ LL unable to deliver packet %d to %d (%d) (reason = %d, ifqlen = xxx)",
                // "A %.9f _%d_ LL unable to deliver packet %d to %d (%d) (reason = %d, ifqlen = %d)",
                CURRENT_TIME,
                index,
                ch->uid_,
                ch->next_hop_,
                ++link_broke,
                ch->xmit_reason_ /*,
                ifqueue->length() */ );
	logtarget->pt_->dump();
}



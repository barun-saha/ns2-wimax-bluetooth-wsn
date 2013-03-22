/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

extern "C" {
#include <stdarg.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
};

#include <random.h>
#include <address.h>
#include <mobilenode.h>
#include <cmu-trace.h>

#include "noah.h"

#define NOAH_BROADCAST_JITTER 0.01 // jitter for all broadcast packets
//#define NOAH_DEBUG

// Returns a random number between 0 and max
static inline double 
jitter (double max, int be_random_)
{
        return (be_random_ ? Random::uniform(max) : 0);
}

static class NOAHClass:public TclClass
{
public:
        NOAHClass ():TclClass ("Agent/NOAH") {}
        TclObject *create (int, const char *const *) {
                return (new NOAH_Agent ());
        }
} class_noah;

NOAH_Agent::NOAH_Agent (): Agent (PT_MESSAGE),
			   myaddr_ (0), subnet_ (0), node_ (0), port_dmux_(0),
			   table_(0), be_random_ (1), use_mac_ (0), table_size(0)
{
        bind ("use_mac_", &use_mac_);
        bind ("be_random_", &be_random_);
}

void
NOAH_Agent::trace (char *fmt,...)
{
	va_list ap;

	if (!tracetarget)
		return;
	
	va_start (ap, fmt);
	vsprintf (tracetarget->pt_->buffer (), fmt, ap);
	tracetarget->pt_->dump ();
	va_end (ap);
}

void
NOAH_Agent::lost_link (Packet *p)
{
        hdr_cmn *ch = HDR_CMN (p);

        if(use_mac_ == 0) {
                drop(p, DROP_RTR_MAC_CALLBACK);
                return;
        }

        if (!use_mac_ || ch->addr_type_ != NS_AF_INET)
                return;

        // Queue these packets up...
        recv(p, 0);
}

static void 
mac_callback (Packet * p, void *arg)
{
        ((NOAH_Agent *) arg)->lost_link (p);
}

int
NOAH_Agent::getEntry(int dst)
{
	if (table_) {
		for (int i = 0; i < table_size; ++i) {
			if (dst == table_[i].dst)
				return table_[i].hop;
		}
	}
	return -3;
}

void
NOAH_Agent::forwardPacket (Packet * p)
{
        hdr_cmn *ch = HDR_CMN(p);
        hdr_ip *iph = HDR_IP(p);

	int dst;
	int next_hop;

	// if the destination is outside mobilenode's domain
	// forward it to base_stn node
	// Note: pkt is not buffered if route to base_stn is unknown

	if (table_) {
		dst = Address::instance().get_nodeaddr(iph->daddr());  
		next_hop = getEntry(dst);

		if (diff_subnet(iph->daddr())) {
			if (next_hop == -3) {
				dst = node_->base_stn();
				next_hop = getEntry(dst);
				if (next_hop == -3) {
					//drop pkt with warning
					fprintf(stderr, "warning: Route to base_stn not known: dropping pkt\n");
					Packet::free(p);
					return;
				}
			}
		}
  
		if (next_hop == -3) {
			//drop pkt with warning
			fprintf(stderr, "warning: no route known: dropping pkt %f dst %i node %i\n",
				Scheduler::instance().clock(), dst, node_->address());
			for (int i = 0; i < table_size; ++i)
				fprintf(stderr, "%i %i, ", table_[i].dst, table_[i].hop);
			fprintf(stderr, "\n");

			Packet::free(p);
			return;
		}
		ch->next_hop_ = next_hop;
	} else {
		if (node_->base_stn() != -1) {
			// code below is for Mobile IP (base station is present)
			if (node_->base_stn() == node_->address()) { // node is its own BS --> HA or FA
				ch->next_hop_ = Address::instance().get_nodeaddr(iph->daddr());
			} else { // MH sends to BS
				// JCW set next hop in mip-reg.cc when sending request etc.
				// don't set when destination already set by MIPMH
				if (ch->next_hop_ <= -2) // see packet.h
					ch->next_hop_ = Address::instance().get_nodeaddr(node_->base_stn());
			}
		} else {
			// wireless-only network -> try to use ARP to get MAC address
			ch->next_hop_ = Address::instance().get_nodeaddr(iph->daddr());
		}
	}

        ch->direction() = hdr_cmn::DOWN;
        ch->addr_type_ = NS_AF_INET;
        ch->xmit_failure_ = mac_callback;
        ch->xmit_failure_data_ = this;

        assert (!HDR_CMN (p)->xmit_failure_ ||
                HDR_CMN (p)->xmit_failure_ == mac_callback);
#ifdef NOAH_DEBUG
        printf("%f NOAH forward (node %s) to %s (via %s), packet %i\n",
	       Scheduler::instance().clock(),
               Address::instance().print_nodeaddr(node_->address()),
               Address::instance().print_nodeaddr(iph->daddr()),
               Address::instance().print_nodeaddr(ch->next_hop_), ch->uid()); // JCW
#endif
        target_->recv(p, (Handler *)0);
        return;
}

void 
NOAH_Agent::sendOutBCastPkt(Packet *p)
{
        Scheduler & s = Scheduler::instance ();
#ifdef NOAH_DEBUG
        hdr_ip *iph = HDR_IP(p);
        hdr_cmn *ch = HDR_CMN(p);
        printf("%f NOAH broadcast (node %s) to %s, packet %i\n",
	       Scheduler::instance().clock(),
               Address::instance().print_nodeaddr(node_->address()),
               Address::instance().print_nodeaddr(iph->daddr()), ch->uid()); // JCW
#endif
        s.schedule (target_, p, jitter(NOAH_BROADCAST_JITTER, be_random_));
}


void
NOAH_Agent::recv (Packet * p, Handler *)
{
        hdr_ip *iph = HDR_IP(p);
        hdr_cmn *ch = HDR_CMN(p);
        int src = Address::instance().get_nodeaddr(iph->saddr());
        int dst = ch->next_hop();

        /*
         *  Must be a packet I'm originating...
         */
        if(src == myaddr_ && ch->num_forwards() == 0) {
                /*
                 * Add the IP Header
                 */
                ch->size() += IP_HDR_LEN;    
                iph->ttl_ = IP_DEF_TTL;
        }
        /*
         *  I received a packet that I sent.  Probably
         *  a routing loop.
         */
        else if(src == myaddr_) {
#ifdef NOAH_DEBUG
                printf("%f NOAH dropped packet (routing loop)\n", Scheduler::instance().clock());
#endif
                drop(p, DROP_RTR_ROUTE_LOOP);
                return;
        }
        /*
         *  Packet I'm forwarding...
         */
        else {
                /*
                 *  Check the TTL.  If it is zero, then discard.
                 */
                if(--iph->ttl_ == 0) {
#ifdef NOAH_DEBUG
                        printf("%f NOAH dropped packet (TTL)\n", Scheduler::instance().clock());
#endif
                        drop(p, DROP_RTR_TTL);
                        return;
                }
        }
  

        if ((u_int32_t) dst == IP_BROADCAST &&
            (iph->dport() != ROUTER_PORT)) {
                if (src == myaddr_) {
                        // handle brdcast pkt
                        sendOutBCastPkt(p);
                } else {
                        // hand it over to the port-demux (in case there is one, i.e. Mobile IP)
			if (port_dmux_) {
#ifdef NOAH_DEBUG
				printf("%f NOAH broadcast demuxed (node %s) from %s, packet %i\n",
				       Scheduler::instance().clock(),
				       Address::instance().print_nodeaddr(node_->address()),
				       Address::instance().print_nodeaddr(iph->daddr()), ch->uid()); // JCW
#endif
				port_dmux_->recv(p, (Handler*)0);
			} else {
#ifdef NOAH_DEBUG
				printf("%f NOAH broadcast dropped (node %s) from %s, packet %i\n",
				       Scheduler::instance().clock(),
				       Address::instance().print_nodeaddr(node_->address()),
				       Address::instance().print_nodeaddr(iph->daddr()), ch->uid()); // JCW
#endif
				drop(p, DROP_RTR_NO_ROUTE);
			}
                }
        } else {
                forwardPacket(p);
        }
}

int 
NOAH_Agent::command (int argc, const char *const *argv)
{
        if (argc == 3) {
                if (strcasecmp (argv[1], "addr") == 0) {
                        myaddr_ = Address::instance().str2addr(argv[2]);
                        subnet_ = Address::instance().get_subnetaddr(myaddr_);
                        return TCL_OK;
                }
                TclObject *obj;
                if ((obj = TclObject::lookup (argv[2])) == 0) {
                        fprintf (stderr, "%s: %s lookup of %s failed\n", __FILE__, argv[1],
                                 argv[2]);
                        return TCL_ERROR;
                }
                if (strcasecmp (argv[1], "tracetarget") == 0) {
                        tracetarget = (Trace *) obj;
                        return TCL_OK;
                }
                else if (strcasecmp (argv[1], "node") == 0) {
                        node_ = (MobileNode*) obj;
                        return TCL_OK;
                }
                else if (strcasecmp (argv[1], "port-dmux") == 0) {
                        port_dmux_ = (NsObject *) obj;
                        return TCL_OK;
                }
        } else if (argc >= 5) {
		if (strcasecmp (argv[1], "routing") == 0) {
                        table_size = atoi(argv[2]);
			if (2 * table_size != argc - 3) {
				fprintf (stderr, "%s: routing table size mismatch %s\n", __FILE__, argv[2]);
				return TCL_ERROR;
			}

			if (table_)
				delete []table_;
			table_ = new struct NOAH_Table[table_size];
			for (int i = 0; i < table_size; ++i) {
				table_[i].dst = atoi(argv[i * 2 + 3]);
				table_[i].hop = atoi(argv[i * 2 + 4]);
			}
                        return TCL_OK;
                }
	}

        return (Agent::command (argc, argv));
}

int 
NOAH_Agent::diff_subnet(int dst) 
{
	// XXX inefficient but we don't have startUp like DSDV
	subnet_ = Address::instance().get_subnetaddr(myaddr_);

	char* dstnet = Address::instance().get_subnetaddr(dst);
	if (subnet_ != NULL) {
		if (dstnet != NULL) {
			if (strcmp(dstnet, subnet_) != 0) {
				delete [] dstnet;
				return 1;
			}
			delete [] dstnet;
		}
	}
	//assert(dstnet == NULL);
	return 0;
}

#ifndef __noah_h_
#define __noah_h_

#include "config.h"
#include "agent.h"
#include "ip.h"
#include "delay.h"
#include "trace.h"
#include "arp.h"
#include "ll.h"
#include "mac.h"

#define ROUTER_PORT      0xff

struct NOAH_Table {
  int dst;
  int hop;
};

class NOAH_Agent : public Agent {
public:
        NOAH_Agent();
        virtual int command(int argc, const char * const * argv);
        void lost_link(Packet *p);
  
protected:
        Packet* rtable(int);
        virtual void recv(Packet *, Handler *);
        void trace(char* fmt, ...);
        void forwardPacket (Packet * p);
        void sendOutBCastPkt(Packet *p);
	int diff_subnet(int dst);
	int getEntry(int dst);

        Trace *tracetarget;       // Trace Target
        int myaddr_;              // My address...
        char *subnet_;            // My subnet
        MobileNode *node_;        // My node
        NsObject *port_dmux_;     // my port dmux

	// for static routes
	struct NOAH_Table *table_;     // Routing Table

        // Randomness/MAC/logging parameters
        int be_random_;
        int use_mac_;
	int table_size;
};

#endif

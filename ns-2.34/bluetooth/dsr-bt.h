#ifndef __dsr_bt_h__
#define __dsr_bt_h__

#include "dsr/dsragent.h"
#include "wnode.h"

class DSR_BT:public DSRAgent, public RoutingIF {
  public:
    DSR_BT():DSRAgent(), RoutingIF() {} 

    int command(int argc, const char *const *argv);
    virtual void recvReply(Packet *p);

    virtual nsaddr_t nextHop(nsaddr_t dst);
    virtual void sendInBuffer(nsaddr_t dst);
    // virtual void start();
    virtual void addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag);
    virtual void delRtEntry(nsaddr_t nexthop);
};

#endif				// __dsr_bt_h__

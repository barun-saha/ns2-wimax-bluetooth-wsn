#ifndef __tora_bt_h__
#define __tora_bt_h__

#include "tora/tora.h"
#include "wnode.h"

class TORA_BT:public toraAgent, public RoutingIF {
  public:
    TORA_BT(nsaddr_t id):toraAgent(id), RoutingIF() {} 

    int command(int argc, const char *const *argv);
    virtual void recvReply(Packet *p);

    virtual nsaddr_t nextHop(nsaddr_t dst);
    virtual void sendInBuffer(nsaddr_t dst);
    // virtual void start();
    virtual void addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag);
    virtual void delRtEntry(nsaddr_t nexthop);
};

#endif				// __tora_bt_h__

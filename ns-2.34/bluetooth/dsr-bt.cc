/*
 *	dsr-bt.cc
 */

#include "dsr-bt.h"
#include "random.h"
#include <lmp.h>
#include <bt-node.h>


static class DSR_BTclass:public TclClass {
  public:
    DSR_BTclass():TclClass("Agent/DSR/BT") {} 

    TclObject *create(int argc, const char *const *argv) {
	return new DSR_BT();
    }
} class_rtProtoDSRBT;

int DSR_BT::command(int argc, const char *const *argv)
{
    if (argc == 3) {
	if (strcmp(argv[1], "node") == 0) {
	    RoutingIF::node_ = (BTNode *) TclObject::lookup(argv[2]);
	    if (RoutingIF::node_ == 0) {
		return TCL_ERROR;
	    }
	    RoutingIF::node_->setRagent(this);
	    return TCL_OK;
	}
    }
    return DSRAgent::command(argc, argv);
}

nsaddr_t DSR_BT::nextHop(nsaddr_t dst)
{
    return -1;
}

void DSR_BT::sendInBuffer(nsaddr_t dst)
{
}

void DSR_BT::addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag)
{
}

void DSR_BT::delRtEntry(nsaddr_t nexthop)
{
}

void DSR_BT::recvReply(Packet *p) {
}


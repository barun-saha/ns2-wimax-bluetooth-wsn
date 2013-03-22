/*
 *	tora-bt.cc
 */

#include "tora-bt.h"
#include "random.h"
#include <lmp.h>
#include <bt-node.h>


static class TORA_BTclass:public TclClass {
  public:
    TORA_BTclass():TclClass("Agent/TORA/BT") {} 

    TclObject *create(int argc, const char *const *argv) {
	assert(argc == 5);
#ifdef NS21B7A_
	nsaddr_t id = (nsaddr_t) atoi(argv[4]);
#else
	nsaddr_t id = (nsaddr_t) Address::instance().str2addr(argv[4]);
#endif
	return new TORA_BT(id);
    }
} class_rtProtoTORABT;

int TORA_BT::command(int argc, const char *const *argv)
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
    return toraAgent::command(argc, argv);
}

nsaddr_t TORA_BT::nextHop(nsaddr_t dst)
{
    return -1;
}

void TORA_BT::sendInBuffer(nsaddr_t dst)
{
}

void TORA_BT::addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag)
{
}

void TORA_BT::delRtEntry(nsaddr_t nexthop)
{
}

void TORA_BT::recvReply(Packet *p) {
}


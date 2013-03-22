/*
 * Copyright (c) 2004, University of Cincinnati, Ohio.
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
 *	bt-node.cc
 */

#include "bt-node.h"
#include "random.h"
#include "scat-form-law.h"
#include "lmp-piconet.h"
#include "lmp-link.h"

static class BTNodeClass:public TclClass {
  public:
    BTNodeClass():TclClass("Node/BTNode") {}
    TclObject *create(int, const char *const *) {
	return (new BTNode());
    }
} class_btnode;


BTNode *BTNode::chain_ = NULL;

BTNode::BTNode()
:  scoAgent_(0), scatFormator_(0), next_(this)
{
    bind("randomizeSlotOffset_", &randomizeSlotOffset_);
    bind("enable_clkdrfit_in_rp_", &enable_clkdrfit_in_rp_);

    randomizeSlotOffset_ = 1;
    enable_clkdrfit_in_rp_ = 1;

    if (chain_ == NULL) {
	chain_ = this;
	chain_->next_ = chain_;
    } else {
	next_ = chain_->next_;
	chain_->next_ = this;
	chain_ = this;
    }
}

// Trace in UCBT is handled differently from the main ns part.
// There are 2 sets of commands in the forms of
//
//      trace-all-XXX [on|off|<filename>]
//      trace-me-XXX  [on|off|<filename>]
// the 'all' commands affect all bluetooth nodes, while 'me' commands 
// affect the current node only.
int BTNode::setTrace(const char *cmdname, const char *arg, int appnd)
{
    FILE **tfile = NULL;
    int *flag = NULL;
    int all = 0;
    const char *name;

    if (!strncmp(cmdname, "trace-all-", 10)) {
	all = 1;
	name = cmdname + 10;
    } else if (strncmp(cmdname, "trace-me-", 9)) {
	fprintf(stderr, "Unknown command %s %s. \n", cmdname, arg);
	return 0;
    } else {
	name = cmdname + 9;
    }

    if (!strcmp(name, "tx")) {
	if (all) {
	    flag = &Baseband::trace_all_tx_;
	} else {
	    flag = &bb_->trace_me_tx_;
	}
	tfile = &BtStat::log_;
    } else if (!strcmp(name, "rx")) {
	if (all) {
	    flag = &Baseband::trace_all_rx_;
	} else {
	    flag = &bb_->trace_me_rx_;
	}
	tfile = &BtStat::log_;
    } else if (!strcmp(name, "in-air")) {
	if (all) {
	    flag = &Baseband::trace_all_in_air_;
	} else {
	    flag = &bb_->trace_me_in_air_;
	}
	tfile = &BtStat::log_;
    } else if (!strcmp(name, "POLL")) {
	if (all) {
	    flag = &Baseband::trace_all_poll_;
	} else {
	    flag = &bb_->trace_me_poll_;
	}
	tfile = &BtStat::log_;
    } else if (!strcmp(name, "NULL")) {
	if (all) {
	    flag = &Baseband::trace_all_null_;
	} else {
	    flag = &bb_->trace_me_null_;
	}
	tfile = &BtStat::log_;
    } else if (!strcmp(name, "stat") || !strcmp(name, "link")) {
	if (all) {
	    flag = &Baseband::trace_all_stat_;
	} else {
	    flag = &bb_->trace_me_stat_;
	}
	tfile = &BtStat::log_;
    } else if (!strcasecmp(name, "nodeStat") || !strcmp(name, "stat3") ||
	       !strcmp(name, "stat2")) {
	if (!stat_) {
	    fprintf(stderr, "stat_ is not set.");
	    abort();
	}
	if (all) {
	    flag = &BtStat::trace_all_node_stat_;
	} else {
	    flag = &stat_->trace_me_node_stat_;
	}
	tfile = &BtStat::logstat_;
    } else if (!strcasecmp(name, "flowStat") ||
	       !strcasecmp(name, "stat-pernode")) {
	if (!stat_) {
	    fprintf(stderr, "stat_ is not set.");
	    abort();
	}
	if (all) {
	    fprintf(stderr,
		    "trace_all can't be used with stat-pernode.\n");
	    abort();
	} else {
	    flag = &stat_->trace_me_flow_stat_;
	    tfile = &stat_->flowLog_;
	}
    } else if (!strcmp(name, "l2cmd")) {
	if (all) {
	    flag = &L2CAP::trace_all_l2cap_cmd_;
	} else {
	    flag = &l2cap_->trace_me_l2cap_cmd_;
	}
	tfile = &BtStat::log_;
    } else if (!strcmp(name, "bnep")) {
	if (all) {
	    flag = &BNEP::trace_all_bnep_;
	} else {
	    flag = &bnep_->trace_me_bnep_;
	}
	tfile = &BtStat::log_;
    } else if (!strcasecmp(name, "scoAgent")) {
	if (all) {
	    flag = &ScoAgent::trace_all_scoagent_;
	} else {
	    fprintf(stderr, "trace_me can't be used with scoAgent.\n");
	    abort();
	}
	tfile = &BtStat::log_;
    } else {
	fprintf(stderr, "Unknown command %s %s. \n", cmdname, arg);
	return 0;
    }

    if (!strcmp(arg, "off")) {
	*flag = 0;
	return 1;
    }

    *flag = 1;
    if (!strcmp(arg, "on")) {	// Use defined tracefile default stdout
	return 1;
    } else if (!strcmp(arg, "stdout")) {
	*tfile = stdout;
	return 1;
    } else if (!strcmp(arg, "stderr")) {
	*tfile = stderr;
	return 1;
    } else {
	return setTraceStream(tfile, arg, cmdname, appnd);
    }
}

void BTNode::on()
{
    Tcl & tcl = Tcl::instance();
    if (scatFormator_) {
	lmp_->scanWhenOn_ = 0;
	lmp_->on();
	scatFormator_->start();
    } else {
	lmp_->on();
    }
    tcl.evalf("%s start ", (dynamic_cast < Agent * >(ragent_))->name());
}

int BTNode::command(int argc, const char *const *argv)
{
    Tcl & tcl = Tcl::instance();

    if (argc > 2 && !strncmp(argv[1], "make-pico-fast", 9)) {
	hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[2]);
	hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[3]);
	for (int i = 4; i < argc; i++) {
	    BTNode *slave = lookupNode(atoi(argv[i]));
	    if (!slave) {
		slave = (BTNode *) TclObject::lookup(argv[i]);
	    }
	    if (!slave) {
		fprintf(stderr, "make-pico-fast: node lookup fails: %s\n",
			argv[i]);
		return TCL_ERROR;
	    }
	    bnep_join(slave, pt, rpt);
	}
	return (TCL_OK);

    } else if (argc == 2) {

	if (!strcmp(argv[1], "cancel-inquiry-scan")) {
	    bb_->inquiryScan_cancel();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "cancel-inquiry")) {
	    lmp_->HCI_Inquiry_Cancel();
	    return TCL_OK;

	    // don't use this cmd directly. use on in tcl space instead.
	} else if (!strcmp(argv[1], "turn-it-on")) {
	    if (on_) {
		return TCL_ERROR;
	    }
	    on_ = 1;
	    if (initDelay_ > 0) {
		Scheduler::instance().schedule(&timer_, &on_ev_,
					       Random::uniform() *
					       initDelay_);
	    } else if (randomizeSlotOffset_) {
		Scheduler::instance().schedule(&timer_, &on_ev_,
					       Random::integer(1250) *
					       1E-6);
	    } else {
		on();
	    }

	    return TCL_OK;

	} else if (!strcmp(argv[1], "version")) {
	    fprintf(stderr, "%s\n\n", BTVERSION);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "print-lmp-cmd-len")) {
	    fprintf(stderr, "sizeof(LMPLink::ParkReq) : %d\n",
		    sizeof(LMPLink::ParkReq));
	    fprintf(stderr, "sizeof(LMPLink::ModBeacon) : %d\n",
		    sizeof(LMPLink::ModBeacon));
	    fprintf(stderr, "sizeof(LMPLink::UnparkBDADDRreq) : %d\n",
		    sizeof(LMPLink::UnparkBDADDRreq));
	    fprintf(stderr, "sizeof(LMPLink::UnparkPMADDRreq) : %d\n",
		    sizeof(LMPLink::UnparkPMADDRreq));
	    fprintf(stderr, "sizeof(BTNode) : %d\n", sizeof(BTNode));
	    fprintf(stderr, "sizeof(Baseband) : %d\n", sizeof(Baseband));
	    fprintf(stderr, "sizeof(LMP) : %d\n", sizeof(LMP));
	    fprintf(stderr, "sizeof(Piconet) : %d\n", sizeof(Piconet));
	    fprintf(stderr, "sizeof(LMPLink) : %d\n", sizeof(LMPLink));
	    fprintf(stderr, "sizeof(L2CAP) : %d\n", sizeof(L2CAP));
	    fprintf(stderr, "sizeof(L2CAPChannel) : %d\n",
		    sizeof(L2CAPChannel));
	    fprintf(stderr, "sizeof(BNEP) : %d\n", sizeof(BNEP));
	    fprintf(stderr, "sizeof(BNEP::Connection) : %d\n",
		    sizeof(BNEP::Connection));
	    fprintf(stderr, "sizeof(BTChannel) : %d\n", sizeof(BTChannel));
	    fprintf(stderr, "sizeof(SDP) : %d\n", sizeof(SDP));
	    fprintf(stderr, "sizeof(Packet) : %d\n", sizeof(Packet));
	    fprintf(stderr, "sizeof(Packet::hdrlen_) : %d\n",
		    Packet::hdrlen_);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "checkWiFi")) {
	    // Baseband::check_wifi_ = 1;
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "notCheckWiFi")) {
	    // Baseband::check_wifi_ = 0;
	    return TCL_OK;
	}

    } else if (argc == 3) {

	if (!strcasecmp(argv[1], "LossMod")) {
	    if (!strcasecmp(argv[2], "BlueHoc")) {
		BTChannel::setLossMode(new LMBlueHoc());
	    } else if (!strcasecmp(argv[2], "CoChBlueHoc")) {
		BTChannel::setLossMode(new LMCoChBlueHoc());
	    } else if (!strcasecmp(argv[2], "off")) {
		BTChannel::setLossMode(new BTLossMod());
	    } else {
		fprintf(stderr,
			"unknown parameter for LossMod [BlueHoc|off]: %s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "scatForm")) {
	    if (!strcasecmp(argv[2], "law")) {
		scatFormator_ = new ScatFormLaw(this);
	    } else {
		fprintf(stderr,
			"unknown parameter for scatForm [law|]: %s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "setall_scanWhenOn")) {
	    setall_scanwhenon(atoi(argv[2]));
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "TPollDefault")) {
	    Baseband::T_poll_default_ = atoi(argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "TPollMax")) {
	    Baseband::T_poll_max_ = atoi(argv[2]);
	    return TCL_OK;

	    // use it before the node is turned on.
	} else if (!strcasecmp(argv[1], "set-clock")) {
	    lmp_->setClock(atoi(argv[2]));
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "ifqtype")) {
	    l2cap_->ifq_ = new char[strlen(argv[2]) + 1];
	    strcpy(l2cap_->ifq_, argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "SchedAlgo")) {
	    if (!strcasecmp(argv[2], "RR")) {
		if (bb_->linkSched_->type() != BTLinkScheduler::RR) {
		    delete bb_->linkSched_;
		    bb_->linkSched_ = new BTRR(bb_);
		}
	    } else if (!strcasecmp(argv[2], "DRR")) {
		if (bb_->linkSched_->type() != BTLinkScheduler::DRR) {
		    delete bb_->linkSched_;
		    bb_->linkSched_ = new BTDRR(bb_);
		}
	    } else if (!strcasecmp(argv[2], "ERR")) {
		if (bb_->linkSched_->type() != BTLinkScheduler::ERR) {
		    delete bb_->linkSched_;
		    bb_->linkSched_ = new BTERR(bb_);
		}
	    } else if (!strcasecmp(argv[2], "PRR")) {
		if (bb_->linkSched_->type() != BTLinkScheduler::PRR) {
		    delete bb_->linkSched_;
		    bb_->linkSched_ = new BTPRR(bb_);
		}
	    } else if (!strcasecmp(argv[2], "FCFS")) {
		if (bb_->linkSched_->type() != BTLinkScheduler::FCFS) {
		    delete bb_->linkSched_;
		    bb_->linkSched_ = new BTFCFS(bb_);
		}
	    } else {
		fprintf
		    (stderr,
		     "unknown parameter for SchedAlgo "
		     "[DRR|ERR|PRR|AWMMF]: %s\n", argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (scatFormator_ &&
		   scatFormator_->type() == ScatFormator::SFLaw &&
		   !strcasecmp(argv[1], "sf-law-delta")) {
	    ((ScatFormLaw *) scatFormator_)->_delta = atof(argv[2]);
	    return TCL_OK;

	} else if (scatFormator_ &&
		   scatFormator_->type() == ScatFormator::SFLaw &&
		   !strcasecmp(argv[1], "sf-law-p")) {
	    ((ScatFormLaw *) scatFormator_)->_P = atof(argv[2]);
	    return TCL_OK;

	} else if (scatFormator_ &&
		   scatFormator_->type() == ScatFormator::SFLaw &&
		   !strcasecmp(argv[1], "sf-law-k")) {
	    ((ScatFormLaw *) scatFormator_)->_K = atoi(argv[2]);
	    return TCL_OK;

	} else if (scatFormator_ &&
		   scatFormator_->type() == ScatFormator::SFLaw &&
		   !strcasecmp(argv[1], "sf-law-term-schred")) {
	    ((ScatFormLaw *) scatFormator_)->_term_schred = atoi(argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "BrAlgo")) {
	    if (!strcasecmp(argv[2], "DRP")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new DichRP(lmp_);
	    } else if (!strncasecmp(argv[2], "DRPDW", 5)) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		int fac = argv[2][5] - '0';
		if (fac < 2 || fac > 8) {
		    fac = 1;
		} else if (fac != 2 && fac != 4 && fac != 8) {
		    fprintf(stderr,
			    "Wrong command: BrAlgo DRPDW[|2|4|8]\n");
		    return TCL_ERROR;
		}
		fprintf(stderr, "%d %s fac: %d\n", bb_->bd_addr_, argv[2],
			fac);
		lmp_->rpScheduler = new DichRPDynWind(lmp_, fac);
	    } else if (!strcasecmp(argv[2], "DRPB")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new DRPBcast(lmp_);
	    } else if (!strcasecmp(argv[2], "MRDRP")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new MultiRoleDRP(lmp_);
	    } else if (!strcasecmp(argv[2], "TDRP")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new TreeDRP(lmp_);
	    } else if (!strcasecmp(argv[2], "MDRP")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new MaxDistRP(lmp_);
	    } else if (!strcasecmp(argv[2], "RPHSI")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new RPHoldSI(lmp_);
	    } else if (!strcasecmp(argv[2], "RPHMI")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new RPHoldMI(lmp_);
	    } else {
		fprintf(stderr,
			"unknown parameter for BrAlgo "
			"[DRP|DRPDW|DRPB|TDRP|MDRP]: %s\n", argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "T_poll_max")) {
	    Baseband::T_poll_max_ = atoi(argv[2]);
	    if (Baseband::T_poll_max_ <= 2) {
		fprintf(stderr,
			"Invalid parameter for T_poll_max (>=2): %s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcmp(argv[1], "bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    BNEP::Connection * conn = bnep_->connect(dest->bb_->bd_addr_);
	    tcl.result(conn->cid->_queue->name());
	    return (TCL_OK);

	} else if (!strcasecmp(argv[1], "useChannelSyn")) {
	    if (atoi(argv[2]) || !strcmp(argv[2], "yes")) {
		LMP::useReSyn_ = 1;
	    } else {
		LMP::useReSyn_ = 0;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "useSynByGod")) {
	    if (atoi(argv[2]) || !strcmp(argv[2], "yes")) {
		Baseband::useSynByGod_ = 1;
	    } else {
		Baseband::useSynByGod_ = 0;
	    }
	    return TCL_OK;

	} else if (!strcmp(argv[1], "set-rate")) {
	    LMP::setRate(atoi(argv[2]));
	    return TCL_OK;

	} else if (!strcmp(argv[1], "test-fh")) {
	    bd_addr_t addr = strtol(argv[2], NULL, 0);
	    bb_->test_fh(addr);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "bnep-disconnect")) {
	    bnep_->disconnect(atoi(argv[2]), 0);	// bd_addr, reason
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "sco-disconnect")) {
	    ScoAgent *ag1 = (ScoAgent *) TclObject::lookup(argv[2]);
	    uchar reason = 0;
	    if (!ag1) {
		fprintf(stderr, "sco-disconnect: unkown Agent:%s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    lmp_->HCI_Disconnect(ag1->connh, reason);
	    return (TCL_OK);
	}

    } else if (argc == 4) {

	if (!strcmp(argv[1], "pagescan")) {
	    lmp_->HCI_Write_Page_Scan_Activity(atoi(argv[2]),
					       atoi(argv[3]));
	    lmp_->reqOutstanding = 1;
	    lmp_->HCI_Write_Scan_Enable(1);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    hdr_bt::packet_type pt = hdr_bt::NotSpecified;
	    hdr_bt::packet_type rpt = hdr_bt::NotSpecified;
	    Queue *ifq = (Queue *) TclObject::lookup(argv[3]);
	    BNEP::Connection * conn =
		bnep_->connect(dest->bb_->bd_addr_, pt, rpt, ifq);
	    tcl.result(conn->cid->_queue->name());

	} else if (!strcmp(argv[1], "bnep-disconnect")) {
	    bnep_->disconnect(atoi(argv[2]), atoi(argv[3]));	// ad, reason
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "sco-disconnect")) {
	    ScoAgent *ag1 = (ScoAgent *) TclObject::lookup(argv[2]);
	    uchar reason = atoi(argv[3]);
	    if (!ag1) {
		fprintf(stderr, "sco-disconnect: unkown Agent:%s\n",
			argv[2]);
		return TCL_ERROR;
	    }
	    // sco_disconnect(ag1, atoi(argv[3]));      // SCOAgent, reason
	    lmp_->HCI_Disconnect(ag1->connh, reason);
	    return (TCL_OK);

	} else if (!strcasecmp(argv[1], "BrAlgo")) {
	    if (!strcasecmp(argv[2], "DRPDW")) {
		if (lmp_->rpScheduler) {
		    delete lmp_->rpScheduler;
		}
		lmp_->rpScheduler = new DichRPDynWind(lmp_, atoi(argv[3]));
	    } else {
		fprintf(stderr,
			"unknown parameter for BrAlgo DRPDW <factor>: %s %s\n",
			argv[2], argv[3]);
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcmp(argv[1], "inquiryscan") ||
		   !strcmp(argv[1], "inqscan")) {
	    lmp_->HCI_Write_Inquiry_Scan_Activity(atoi(argv[2]),
						  atoi(argv[3]));
	    lmp_->reqOutstanding = 1;
	    lmp_->HCI_Write_Scan_Enable(2);
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "pktType")) {
	    lmp_->defaultPktType_ = hdr_bt::str_to_packet_type(argv[2]);
	    lmp_->defaultRecvPktType_ =
		hdr_bt::str_to_packet_type(argv[3]);
	    if (lmp_->defaultPktType_ == hdr_bt::Invalid
		|| lmp_->defaultRecvPktType_ == hdr_bt::Invalid) {
		fprintf(stderr, "Invalid packet type.\n");
		return (TCL_ERROR);
	    }
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "inquiry")) {
	    lmp_->reqOutstanding = 1;
	    lmp_->HCI_Inquiry(lmp_->giac_, atoi(argv[2]), atoi(argv[3]));
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "cmd-after-bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    BNEP::Connection * conn =
		bnep_->lookupConnection(dest->bb_->bd_addr_);
	    char *cmd = new char[strlen(argv[3]) + 1];
	    int trmL = 0;
	    if (*argv[3] == '{') {
		trmL++;
		if (argv[3][1] == '{') {
		    trmL++;
		}
	    }
	    strcpy(cmd, argv[3] + trmL);
	    cmd[strlen(argv[3]) - 2 * trmL] = '\0';

	    conn->setCmd(cmd);
	    fprintf(stderr, "==== nsCmd:%s\n", cmd);
	    // dump_str(argv[3]);
	    // ch->setCmd("$cbr0 start\n");
	    return TCL_OK;

	} else if (!strcmp(argv[1], "set-prio-level")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    bb_->set_prio(dest->bb_->bd_addr_, atoi(argv[3]));
	    return TCL_OK;

	} else if (!strcmp(argv[1], "switch-role")) {
	    BTNode *peer = (BTNode *) TclObject::lookup(argv[2]);
	    if (!peer) {
		fprintf(stderr,
			"cmd switch-role, can't get peer node: %s",
			argv[2]);
		return TCL_ERROR;
	    }
	    int tobemaster;
	    if (!strcmp(argv[3], "master")) {
		tobemaster = 0;
	    } else if (!strcmp(argv[3], "slave")) {
		tobemaster = 1;
	    } else {
		fprintf(stderr,
			"unknown parameter: %s. "
			"switch-role node [master|slave]\n", argv[3]);
		return TCL_ERROR;
	    }

	    lmp_->HCI_Switch_Role(peer->bb_->bd_addr_, tobemaster);
	    return TCL_OK;
	}

    } else if (argc == 5) {

	if (!strcmp(argv[1], "bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[3]);
	    hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[4]);
	    BNEP::Connection * conn =
		bnep_->connect(dest->bb_->bd_addr_, pt, rpt);
	    tcl.result(conn->cid->_queue->name());
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "sco-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    ScoAgent *ag1 = (ScoAgent *) TclObject::lookup(argv[3]);
	    ScoAgent *ag2 = (ScoAgent *) TclObject::lookup(argv[4]);
	    sco_connect(dest, ag1, ag2);
	    return (TCL_OK);

	    // set up piconet by bypassing the connection set up process
	    // master join slave toslavePkt toMasterPkt
	} else if (!strcmp(argv[1], "join")) {
	    BTNode *slave = (BTNode *) TclObject::lookup(argv[2]);
	    hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[3]);
	    hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[4]);
	    // join(slave, pt, rpt);
	    bnep_join(slave, pt, rpt);
	    return (TCL_OK);
	}

    } else if (argc == 6) {

	if (!strcmp(argv[1], "bnep-connect")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    hdr_bt::packet_type pt = hdr_bt::str_to_packet_type(argv[3]);
	    hdr_bt::packet_type rpt = hdr_bt::str_to_packet_type(argv[4]);
	    Queue *ifq = (Queue *) TclObject::lookup(argv[5]);
	    BNEP::Connection * conn =
		bnep_->connect(dest->bb_->bd_addr_, pt, rpt, ifq);
	    tcl.result(conn->cid->_queue->name());
	    return (TCL_OK);
	}

    } else if (argc == 9) {

	if (!strcmp(argv[1], "setup")) {
	    uint32_t addr = atoi(argv[2]);

	    /* all devices are initiated as slaves. Anyway, the role changes
	     * dynamically, since the one who does inquiry, paging, 
	     * becomes master.  The one who does inquiry scan and page scan
	     * becomes slave.
	     */

	    BTChannel *ch = (BTChannel *) TclObject::lookup(argv[3]);
	    Baseband *bb = (Baseband *) TclObject::lookup(argv[4]);
	    LMP *lmp = (LMP *) TclObject::lookup(argv[5]);
	    L2CAP *l2cap = (L2CAP *) TclObject::lookup(argv[6]);
	    BNEP *bnep = (BNEP *) TclObject::lookup(argv[7]);
	    SDP *sdp = (SDP *) TclObject::lookup(argv[8]);

	    setup(addr, ch, bb, lmp, l2cap, bnep, sdp);

	    return (TCL_OK);

	} else if (!strcmp(argv[1], "bnep-qos-setup")) {
	    BTNode *dest = (BTNode *) TclObject::lookup(argv[2]);
	    L2CAPChannel *ch = bnep_->lookupChannel(dest->bb_->bd_addr_);
	    uint8_t Flags = atoi(argv[3]);
	    uint8_t Service_Type = atoi(argv[4]);
	    int Token_Rate = atoi(argv[5]);
	    int Peak_Bandwidth = atoi(argv[6]);
	    int Latency = atoi(argv[7]);
	    int Delay_Variation = atoi(argv[8]);

	    ch->setQosParam(new QosParam(Flags, Service_Type, Token_Rate,
					 Peak_Bandwidth, Latency,
					 Delay_Variation));
	    return (TCL_OK);
	}
    }
#if 0
    fprintf(stderr, "%s (%d) :", argv[1], argc - 2);
    for (int i = 0; i < argc - 2; i++) {
	fprintf(stderr, "%s ", argv[i + 2]);
    }
    fprintf(stderr, "\n");
#endif

    return WNode::command(argc, argv);
}

void BTNode::setdest(double x, double y, double z, double s)
{
    bb_->setdest(x, y, z, s);
}

void BTNode::getIFQ(WNode * rmt)
{
    Tcl & tcl = Tcl::instance();
    L2CAPChannel *ch = l2cap_->lookupChannel(PSM_BNEP, rmt->getAddr());
    tcl.result(ch->_queue->name());
}

void BTNode::setIFQ(WNode * rmt, Queue * q)
{
    L2CAPChannel *ch = l2cap_->lookupChannel(PSM_BNEP, rmt->getAddr());
    // TODO: need to clean up the old queue??
    ch->_queue = q;
}

void BTNode::energyReset()
{
    bb_->energyReset();
}


void BTNode::setup(uint32_t addr, BTChannel * ch, Baseband * bb, LMP * lmp,
		   L2CAP * l2cap, BNEP * bnep, SDP * sdp)
{
    phy_ = ch;
    bb_ = bb;
    lmp_ = lmp;
    l2cap_ = l2cap;
    bnep_ = bnep;
    sdp_ = sdp;

    setAddr(addr);

    phy_->setup(addr, bb, lmp, this);
    lmp_->setup(addr, ch, bb, l2cap, this);
    l2cap_->setup(addr, lmp, bnep, sdp, this);
    bnep_->setup(addr, lmp, l2cap, sdp, this);
    sdp_->setup(addr, lmp, l2cap, this);

    stat_ = new BtStat(0, 100, 1, addr, this);
}

void BTNode::addScoAgent(ScoAgent * a)
{
    a->next = scoAgent_;
    scoAgent_ = a;
}

void BTNode::sco_connect(BTNode * dest, ScoAgent * ag_here,
			 ScoAgent * ag_dest)
{
    addScoAgent(ag_here);
    dest->addScoAgent(ag_dest);
    uint32_t addr = dest->bb_->bd_addr_;
    ag_here->daddr = addr;
    ag_dest->daddr = bb_->bd_addr_;
    ag_here->lmp_ = lmp_;
    ag_dest->lmp_ = dest->lmp_;

    if (!lmp_->addReqAgent(ag_here)) {
	fprintf(stderr, "BTNode::sco_connect(): sco req pending.\n");
	exit(-1);
    }
    if (!dest->lmp_->addReqAgent(ag_dest)) {
	fprintf(stderr, "BTNode::sco_connect(): sco req pending.\n");
	exit(-1);
    }

    ConnectionHandle *connh = l2cap_->lookupConnectionHandle(addr);

    if (!connh) {
	Bd_info *bd;
	if ((bd = lmp_->lookupBdinfo(addr)) == NULL) {
	    bd = new Bd_info(addr, 0);
	}
	connh =
	    lmp_->HCI_Create_Connection(addr, hdr_bt::DH1, bd->sr_,
					bd->page_scan_mode_, bd->offset_,
					lmp_->allowRS_);
    }
    ag_here->connh =
	lmp_->HCI_Add_SCO_Connection(connh,
				     (hdr_bt::packet_type) ag_here->
				     packetType_);
}

int BTNode::masterLink(bd_addr_t rmt)
{
    ConnectionHandle *connh = l2cap_->lookupConnectionHandle(rmt);
    return (connh->isMaster());
}

void BTNode::linkDetached(bd_addr_t addr, uchar reason)
{
    if (scatFormator_) {
	scatFormator_->linkDetached(addr, reason);
    }
}

void BTNode::printStat()
{
    fprintf(BtStat::log_, "\n*** Stat for node: %d\n", bb_->bd_addr_);
    bb_->dumpEnergy();
    bb_->dumpTtlSlots();
    if (stat_) {
	stat_->dump();
    }
}

void BTNode::printClassified(FILE * out)
{
    struct Rec {
	double activeTime;
	double ttlTime;
	int numTurnOn;
	int num;

	double duty;

	double aveActiveT;
	double aveTtlT;
	double aveNumTurnOn;

	// int numPkt;
	// int numPktLoss;

	Rec():activeTime(0), ttlTime(0), numTurnOn(0), num(0), duty(0),
	    aveActiveT(0), aveTtlT(0), aveNumTurnOn(0) {
	}
	// numPkt(0), numPktLoss(0) {} 
    };

    double now = Scheduler::instance().clock();
    double warmupTime = bb_->energyRec_.warmUpTime_;
    Rec rec[7];
    BTNode *wk = this;
    int ind = 0;

    do {
	//               Master   bridge   slave
	// has_traffic     0       2        4
	//  no_traffic     1       3        5

	ind = (wk->lmp_->masterPico ? 0 :
	       (wk->lmp_->numPico() > 1 ? 2 : 4));
	ind += (wk->stat_->hasTraffic_ ? 0 : 1);

	rec[ind].activeTime += wk->bb_->energyRec_.activeTime_;
	rec[ind].ttlTime += (now - wk->bb_->energyRec_.startTime_);
	rec[ind].numTurnOn += wk->bb_->energyRec_.numTurnOn_;
	rec[ind].num++;

    } while ((wk = wk->next_) != this);

    int i;
    for (i = 0; i < 7; i++) {
	if (rec[i].num == 0) {
	    continue;
	}

	rec[i].duty = (rec[i].activeTime + rec[i].numTurnOn *
		       warmupTime) / rec[i].ttlTime;
	rec[i].aveActiveT = rec[i].activeTime / rec[i].num;
	rec[i].aveTtlT = rec[i].ttlTime / rec[i].num;
	rec[i].aveNumTurnOn = double (rec[i].numTurnOn) / rec[i].num;

	// sum is stored in rec[6]
	if (i < 6) {
	    rec[6].num += rec[i].num;
	    rec[6].activeTime += rec[i].activeTime;
	    rec[6].numTurnOn += rec[i].numTurnOn;
	    rec[6].ttlTime += rec[i].ttlTime;
	}
    }

    /*
       FORMAT:
       <Ma w/ Tr> <Ma w/o Tr> <Ma> <Br w/ Tr> <Br w/o Tr> <Br>
       <Sl w/ tr> <Sl w/o tr> <AVE>
     */
    for (i = 0; i < 7; i++) {
	fprintf(out, " %f", rec[i].duty);
	fprintf(out, " %f %f %f %d",
		rec[i].aveTtlT, rec[i].aveActiveT, rec[i].aveNumTurnOn,
		rec[i].num);

	if (i % 2) {		// average per node type
	    double d = 0;
	    double ttlT = 0;
	    double actT = 0;
	    double numOn = 0;

	    int num;
	    double t;

	    if ((t = (rec[i].ttlTime + rec[i - 1].ttlTime)) > 0) {
		d = (rec[i].activeTime + rec[i].numTurnOn * warmupTime
		     + rec[i - 1].activeTime
		     + rec[i - 1].numTurnOn * warmupTime) / t;
	    }

	    if ((num = (rec[i].num + rec[i - 1].num)) > 0) {
		ttlT = (rec[i].ttlTime + rec[i - 1].ttlTime) / num;
		actT = (rec[i].activeTime + rec[i - 1].activeTime) / num;
		numOn =
		    double (rec[i].numTurnOn + rec[i - 1].numTurnOn) / num;
	    }

	    fprintf(out, " %f %f %f %f %d", d, ttlT, actT, numOn, num);
	}
    }
}

// output file format:
//  stat_: node list <rate, delay, delayHop, deliveryRatio, dlvRHop> \t 
//                        (if rate is 0, the rest is not printed out)   |
//         classified <ma-load ma-idle ma> <br> <sl> <ave> |
//         flow summay <rate, delay, delayLastHop, deliveryRatio, dlvRLstHop> |
//         inq/pag collision statistics |
//         sumTtlDelay, sumTtlHop, sumTtlDelayPerhop, sumTtlNumRecvPkt,
//                      sumTtlNumPktLoss, sumTtlNumPkt,
//                      sumTtlNumPktLossLastHop, sumTtlNumPktLastHop |
//         inq/pag statistics 
//
//  pernode: flow list <dst, port, pkt-seq, pkt-seq-start, pktLoss, 
//                      numRec, recTime> \t |
//           record list <flow list : bytes, delayThisHop, numPktRecv,
//                                delay, ttlHops, pktLoss, pktLossThishop \t> |

void BTNode::printAllStatExtra()
{
    fprintf(BtStat::log_,
	    "\n Flow info: dateRate Delay DeliveryRaio -- \n");

    BTNode *wk = this;
    do {
	if (wk->stat_->isDst_) {
	    double rate = (wk->stat_->ttlTime_ < 1E-13 ? 0 :
			   wk->stat_->ttlRecvd_ / wk->stat_->ttlTime_);
	    double delay = (wk->stat_->ttlHop_ == 0 ? 0 :
			    wk->stat_->ttlDelay_ / wk->stat_->ttlHop_);
	    double loss = (wk->stat_->ttlNumPkt_ == 0 ? 1 :
			   double (wk->stat_->ttlNumPktLoss_) /
			   wk->stat_->ttlNumPkt_);
	    fprintf(BtStat::log_,
		    "%d:\t%f (%d/%f)\t%f (%f/%d)\t%f (1 - %d/%d)\n",
		    wk->bb_->bd_addr_, rate,
		    wk->stat_->ttlRecvd_,
		    wk->stat_->ttlTime_,
		    delay,
		    wk->stat_->ttlDelay_, wk->stat_->ttlHop_,
		    1 - loss, wk->stat_->ttlNumPktLoss_,
		    wk->stat_->ttlNumPkt_);
	}
    } while ((wk = wk->next_) != this);


    if (stat_
	&& (stat_->trace_all_node_stat_ || stat_->trace_me_node_stat_)) {
	fprintf(BtStat::logstat_, "|");
	printClassified(BtStat::logstat_);

	double sumTtlTime = 0;
	double sumTtlRecvd_ = 0;
	int sumTtlHop = 0;
	double sumTtlDelay = 0;
	double sumTtlDelayPerhop = 0;
	int sumTtlNumRecvPkt = 0;
	int sumTtlNumPkt = 0;
	int sumTtlNumPktLoss = 0;
	int sumTtlNumPktLastHop = 0;
	int sumTtlNumPktLossLastHop = 0;
	int numDst = 0;

	wk = this;
	do {
	    if (wk->stat_->isDst_) {
		sumTtlTime += wk->stat_->ttlTime_;
		sumTtlRecvd_ += wk->stat_->ttlRecvd_;
		sumTtlHop += wk->stat_->ttlHop_;
		sumTtlDelay += wk->stat_->ttlDelay_;

		sumTtlDelayPerhop += wk->stat_->ttlDelayPerhop_;
		sumTtlNumRecvPkt += wk->stat_->ttlNumRecvPkt;
		sumTtlNumPkt += wk->stat_->ttlNumPkt_;
		sumTtlNumPktLoss += wk->stat_->ttlNumPktLoss_;

		sumTtlNumPktLastHop += wk->stat_->ttlNumPktLastHop_;
		sumTtlNumPktLossLastHop +=
		    wk->stat_->ttlNumPktLossLastHop_;

		numDst++;
	    }
	} while ((wk = wk->next_) != this);

	double rate = (sumTtlTime < 1E-13 ? 0 : sumTtlRecvd_ / sumTtlTime);
	double delay = (sumTtlHop == 0 ? 0 : sumTtlDelay / sumTtlHop);
	double delayThisHop = (sumTtlNumRecvPkt == 0 ? 0 :
			       sumTtlDelayPerhop / sumTtlNumRecvPkt);
	double loss =
	    (sumTtlNumPkt ==
	     0 ? 1 : double (sumTtlNumPktLoss) / sumTtlNumPkt);
	double lossThisHop =
	    (sumTtlNumPktLastHop ==
	     0 ? 1 : double (sumTtlNumPktLossLastHop) /
	     sumTtlNumPktLastHop);

	fprintf(BtStat::logstat_, "|%f %f %f %f %f",
		rate, delay, delayThisHop, (1 - loss), (1 - lossThisHop));

	fprintf(BtStat::logstat_, "|%f %d %f %d %d %d %d %d",
		sumTtlDelay, sumTtlHop,
		sumTtlDelayPerhop, sumTtlNumRecvPkt,
		sumTtlNumPktLoss, sumTtlNumPkt, sumTtlNumPktLossLastHop,
		sumTtlNumPktLastHop);

	fprintf(BtStat::logstat_, "\n");
    }

    if (scatFormator_) {
	scatFormator_->dumpTopo();
    }
}

void BTNode::setall_scanwhenon(int v)
{
    BTNode *wk = this;
    do {
	wk->lmp_->scanWhenOn_ = v;
    } while ((wk = wk->next_) != this);
}

void BTNode::force_on()
{
    Scheduler & s = Scheduler::instance();
    if (!lmp_->_on) {
	lmp_->_on = 1;
	// checkLink();
	bb_->on();
	// bb_->clkn_ = (bb_->clkn_ & 0xFFFFFFFC) + 3;
	int ntick = (bb_->clkn_ & 0x03);
	double nextfr = bb_->clkn_ev_.time_;
	bb_->t_clkn_00_ = nextfr - bb_->Tick * (ntick + 1);
	while (bb_->t_clkn_00_ > s.clock()) {
	    bb_->t_clkn_00_ -= bb_->SlotTime * 2;
	}
/*
	bb_->clkn_ = (bb_->clkn_ & 0xFFFFFFFC) + 3;
	bb_->t_clkn_00_ = nextfr - bb_->SlotTime * 2;
*/
    }
}

void BTNode::forceSuspendCurPico()
{
    Scheduler & s = Scheduler::instance();
    LMPLink *wk = lmp_->curPico->activeLink;
    if (wk) {
	int numLink = lmp_->curPico->numActiveLink;
	for (int i = 0; i < numLink; i++) {
	    if (wk->_in_sniff) {
		if (wk->_sniff_ev_to->uid_ > 0) {
		    s.cancel(wk->_sniff_ev_to);
		}
		wk->handle_sniff_suspend();
	    } else {
		wk->force_oneway_hold(200);
	    }
	    wk = wk->next;
	}
    } else {
	lmp_->suspendCurPiconet();
    }
}

// Bypass any message tx/rx, page/pagescan, to have bnep Conn ready.
void BTNode::bnep_join(BTNode * slave, hdr_bt::packet_type pt,
		       hdr_bt::packet_type rpt)
{
    if (pt < hdr_bt::NotSpecified) {
	lmp_->defaultPktType_ = pt;
	slave->lmp_->defaultRecvPktType_ = pt;
    }
    if (rpt < hdr_bt::NotSpecified) {
	lmp_->defaultRecvPktType_ = rpt;
	slave->lmp_->defaultPktType_ = rpt;
    }

    Scheduler & s = Scheduler::instance();

    // Make sure the 2 nodes are on and active.
    force_on();
    slave->force_on();
    if (slave->bb_->inSleep_) {
	slave->bb_->wakeupClkn();
    }
    if (bb_->inSleep_) {
	bb_->wakeupClkn();
    }
    // suspend curPico for the slave, if necessary.
    if (slave->lmp_->curPico) {
	slave->forceSuspendCurPico();
    }
    // suspend curPico for the master, if necessary.
    if (lmp_->curPico && lmp_->curPico != lmp_->masterPico) {
	forceSuspendCurPico();
    }
    // compute slotoffset and clkoffset
    double t;
    int sclkn = slave->bb_->clkn_;
    double sclkn_00 = slave->bb_->t_clkn_00_;
/*
    if ((t =
	 (s.clock() - slave->bb_->t_clkn_00_)) >=
	slave->bb_->SlotTime * 2) {
	int nc = int (t / (bb_->SlotTime * 2));
	sclkn_00 += slave->bb_->SlotTime * 2 * nc;
	sclkn += (nc * 4);
    }
*/
    int slotoffset =
	(int) ((bb_->t_clkn_00_ + BT_CLKN_CLK_DIFF - sclkn_00) * 1E6);
    while (slotoffset < 0) {
	slotoffset += 1250;
    }
    while (slotoffset >= 1250) {
	slotoffset -= 1250;
    }

/*
    if (slotoffset <= -1250 || slotoffset >= 1250) {
	fprintf(stderr, "slotoffset is %d %f %f %f %f\n",
		slotoffset, s.clock(),
		bb_->t_clkn_00_, sclkn_00, slave->bb_->t_clkn_00_);
	abort();
    }
*/
    int clkoffset = ((int) (bb_->clkn_ & 0xFFFFFFFC)
		     - (int) (sclkn & 0xFFFFFFFC));

    Bd_info *slaveinfo =
	lmp_->_add_bd_info(new Bd_info(slave->bb_->bd_addr_, clkoffset));
    Bd_info *masterinfo =
	slave->lmp_->_add_bd_info(new Bd_info(bb_->bd_addr_, clkoffset));
    ConnectionHandle *connh = new ConnectionHandle(pt, rpt);
    ConnectionHandle *sconnh = new ConnectionHandle(rpt, pt);

    masterinfo->lt_addr_ = slaveinfo->lt_addr_ =
	lmp_->get_lt_addr(slave->bb_->bd_addr_);
    slave->lmp_->_my_info->lt_addr_ = slaveinfo->lt_addr_;

    Piconet *pico = lmp_->masterPico;
    LMPLink *mlink;
    if (pico) {
	mlink = pico->add_slave(slaveinfo, lmp_, connh);
    } else {
	mlink = lmp_->add_piconet(slaveinfo, 1, connh);
    }
    mlink->txBuffer->reset_seqn();
    mlink->piconet->slot_offset = 0;
    mlink->piconet->clk_offset = 0;

    LMPLink *slink = slave->lmp_->add_piconet(masterinfo, 0, sconnh);
    // slink->lt_addr_ = mlink->lt_addr_ = slaveinfo->lt_addr_;
    slink->txBuffer->reset_seqn();
    slink->piconet->slot_offset = slotoffset;
    slink->piconet->clk_offset = clkoffset;

    mlink->pt = pt;
    mlink->rpt = rpt;
    slink->rpt = pt;
    slink->pt = rpt;

    // start master's clk
    lmp_->curPico = lmp_->masterPico;
    bb_->setPiconetParam(lmp_->masterPico);
    if (bb_->clk_ev_.uid_ > 0) {
	s.cancel(&bb_->clk_ev_);
    }
    t = s.clock() - bb_->t_clkn_00_;
    if (t < BT_CLKN_CLK_DIFF) {
	bb_->clk_ = bb_->clkn_ - 2;
	t = BT_CLKN_CLK_DIFF - t;
    } else {
	t = bb_->SlotTime * 2 + BT_CLKN_CLK_DIFF - t;
	bb_->clk_ = (bb_->clkn_ & 0xfffffffc) + 2;
    }
    if (bb_->clk_ & 0x01) {
	fprintf(stderr, "clkn:%d clk:%d t:%f\n", bb_->clkn_, bb_->clk_, t);
	abort();
    }
    s.schedule(&bb_->clk_handler_, &bb_->clk_ev_, t);

    // start slave's clk
    slave->lmp_->curPico = slink->piconet;
    slave->bb_->setPiconetParam(slink->piconet);
    if (slave->bb_->clk_ev_.uid_ > 0) {
	s.cancel(&slave->bb_->clk_ev_);
    }
    slave->bb_->clk_ = bb_->clk_;
    s.schedule(&slave->bb_->clk_handler_, &slave->bb_->clk_ev_, t);

    // lmp_->link_setup(connh);
    connh->ready_ = 1;
    sconnh->ready_ = 1;
    slink->connected = 1;
    mlink->connected = 1;

    // set up L2CAPChannel
    L2CAPChannel *ch = setupL2CAPChannel(connh, slave->bb_->bd_addr_, 0);
    L2CAPChannel *sch =
	slave->setupL2CAPChannel(sconnh, bb_->bd_addr_, ch);
    ch->_rcid = sch;

    // set up BNEP Connection    
    BNEP::Connection * bc = bnep_->addConnection(ch);
    BNEP::Connection * sbc = slave->bnep_->addConnection(sch);
    bnep_->_br_table.add(bc->daddr, bc->port);
    slave->bnep_->_br_table.add(sbc->daddr, sbc->port);
    bc->ready_ = 1;
    sbc->ready_ = 1;

    // start SNIFF negotiation.
    if (slave->lmp_->rpScheduler && (slave->lmp_->lowDutyCycle_ ||
				     slave->lmp_->suspendPico)) {
	slave->lmp_->rpScheduler->start(slink);
#if 0
    } else if (lmp_->rpScheduler && (lmp_->lowDutyCycle_ ||
				     lmp_->suspendPico)) {
#endif
    } else if (lmp_->rpScheduler && lmp_->suspendPico) {
	lmp_->rpScheduler->start(mlink);
    }
}

L2CAPChannel *BTNode::setupL2CAPChannel(ConnectionHandle * connh,
					bd_addr_t rmt, L2CAPChannel * rcid)
{
    L2CAPChannel *ch = new L2CAPChannel(l2cap_, PSM_BNEP, connh, rcid);
    ch->_bd_addr = rmt;
    connh->recv_packet_type = lmp_->defaultRecvPktType_;
    l2cap_->addConnectionHandle(connh);
    // ch->_connhand->add_channel(ch);
    ch->_connhand->reqCid = ch;
    l2cap_->registerChannel(ch);
    // connh->ready_ = 1;
    ch->ready_ = 1;
    ch->_rcid = rcid;
    return ch;
}

void BTNode::flushPkt(bd_addr_t addr)
{
    L2CAPChannel *ch = l2cap_->lookupChannel(PSM_BNEP, addr);
    if (ch) {
	ch->flush();
    }
}

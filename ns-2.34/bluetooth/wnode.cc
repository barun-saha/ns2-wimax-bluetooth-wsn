/*
 * Copyright (c) 2005, University of Cincinnati, Ohio.
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
 *	wnode.cc
 */

#include "wnode.h"
#include "random.h"


void WNodeTimer::handle(Event * e)
{
    if (e == &node_->on_ev_) {
	node_->on();
    }
}

WNode *WNode::chain_ = NULL;
char *WNode::ifqType_ = WNODE_IFQ_TYPE;
int WNode::ifqLimit_ = WNODE_IFQ_LIMIT;

int WNode::numFileOpened_ = 0;
const char *WNode::tracefilename_[MAXTRACEFILES];
FILE *WNode::tracefile_[MAXTRACEFILES];
char WNode::filenameBuff_[4096];
int WNode::filenameBuffInd_ = 0;

// double WNode::collisionDist_ = WNODE_INTERFERE_DISTANCE;
// double WNode::radioRange_ = WNODE_EFF_DISTANCE;
// double WNode::toroidal_x_ = -1.0;
// double WNode::toroidal_y_ = -1.0;

WNode::WNode()
:  addr_(-1), ragent_(0), stat_(0), on_(0), timer_(this), next_(this),
wifi_(0)
{
    bind("initDelay_", &initDelay_);
    bind("X_", &X_);
    bind("Y_", &Y_);
    bind("Z_", &Z_);
    initDelay_ = 0;

    logfile_ = stdout;

    collisionDist_ = WNODE_INTERFERE_DISTANCE;
    radioRange_ = WNODE_EFF_DISTANCE;
    toroidal_x_ = -1.0;
    toroidal_y_ = -1.0;
    toroidal_z_ = -1.0;

    x1_range = 0;
    y1_range = 0;
    z1_range = 0;
    x2_range = WNODE_RANGE;
    y2_range = WNODE_RANGE;
    z2_range = WNODE_RANGE;

    X_ = Random::uniform() * (x2_range - x1_range) + x1_range;
    Y_ = Random::uniform() * (y2_range - y1_range) + y1_range;
    Z_ = 0;
    // speed_ = 0;
    dX_ = 0;			// speed = 0
    dY_ = 0;
    dZ_ = 0;

    if (chain_ == NULL) {
	chain_ = this;
	chain_->next_ = chain_;
    } else {
	next_ = chain_->next_;
	chain_->next_ = this;
	chain_ = this;
    }
}

WNode::~WNode()
{
}

void WNode::setPos(double x, double y, double z) 
{
    X_ = x;
    Y_ = y;
    Z_ = z; 
}

void WNode::setRange(double x1, double y1, double z1, 
		     double x2, double y2, double z2)
{
    if (x1 == 0 && y1 == 0  && z1 == 0 && x2 == 0 && y2 == 0 && z2 == 0) {
	return;
    }
    x1_range = x1;
    y1_range = y1;
    z1_range = z1;
    x2_range = x2;
    y2_range = y2;
    z2_range = z2;

    double x = (x1 == x2 ? x1 : (Random::uniform() * (x2 - x1) + x1));
    double y = (y1 == y2 ? y1 : (Random::uniform() * (y2 - y1) + y1));
    double z = (z1 == z2 ? z1 : (Random::uniform() * (z2 - z1) + z1));
    setPos(x, y, z); 

// #define BTDEBUG_00
#ifdef BTDEBUG_00
    fprintf(stderr,
	    "%d Set range[(%f,%f,%f), (%f,%f,%f)]. "
	    "Reposition to (%f, %f,%f)\n", 
	    getAddr(), x1, y1, z1, x2, y2, z2, X_, Y_, Z_);
#endif
}

int WNode::setTraceStream(FILE ** tfileref, const char *fname,
			  const char *cmd, int appnd)
{
    int i;

    for (i = 0; i < numFileOpened_; i++) {
	if (!strcmp(fname, tracefilename_[i])) {
	    *tfileref = tracefile_[i];
	    if (appnd) {
		fprintf(stderr, "file %s is open. Can't use 'a' flag.",
			fname);
		return 0;
	    }
	    return 1;
	}
    }

    if (numFileOpened_ >= MAXTRACEFILES) {
	fprintf(stderr,
		"Too many trace files opened.  Change MAXTRACEFILES"
		" in bt.h and recompile.\n");
	return 0;
    }

    const char *flag = (appnd ? "a" : "w");
    if ((tracefile_[numFileOpened_] = fopen(fname, flag))) {
	strcpy(filenameBuff_ + filenameBuffInd_, fname);
	tracefilename_[numFileOpened_] = filenameBuff_ + filenameBuffInd_;
	filenameBuffInd_ += (strlen(fname) + 1);
	*tfileref = tracefile_[numFileOpened_];
	numFileOpened_++;
	return 1;
    } else {
	fprintf(stderr, "Can't open trace file: %s for cmd: %s\n", fname,
		cmd);
	return 0;
    }
}

void WNode::on()
{
    Tcl & tcl = Tcl::instance();
    tcl.evalf("%s start ", (dynamic_cast < Agent * >(ragent_))->name());
    on_ = 0;
}

// numNode is the total number of nodes, including this node itself.
WNode **WNode::getNodes(int argc, const char *const *argv, int numNode,
			const char *const cmd)
{
    WNode **nd = new WNode *[numNode];
    nd[0] = this;
    int startPos = argc - numNode;
    for (int i = 1; i < numNode; i++) {
	if (argv[startPos + i][0] >= '0' && argv[startPos + i][0] <= '9') {
	    nd[i] = lookupNode(atoi(argv[startPos + i]));
	} else {
	    nd[i] = (WNode *) TclObject::lookup(argv[startPos + i]);
	}
	if (!nd[i]) {
	    fprintf(stderr, "%d %s: node lookup fails: %s\n",
		    addr_, cmd, argv[startPos + i]);
	    abort();
	}
    }
    return nd;
}

int WNode::command(int argc, const char *const *argv)
{
    // Tcl & tcl = Tcl::instance();
    if (argc > 2 && !strncmp(argv[1], "manu-rt-", 8)) {
	int numNode = argc - 1;
	WNode **nd = getNodes(argc, argv, numNode, argv[1]);

	int i;
	if (!strcmp(argv[1], "manu-rt-path")) {
	    for (i = 0; i < numNode - 1; i++) {
		nd[i]->ragent_->addRtEntry(nd[numNode - 1]->getAddr(),
					   nd[i + 1]->getAddr(), 0);
	    }

	    for (i = numNode - 1; i > 0; i--) {
		nd[i]->ragent_->addRtEntry(nd[0]->getAddr(),
					   nd[i - 1]->getAddr(), 0);
	    }
	} else if (!strcmp(argv[1], "manu-rt-linear")) {
	    for (int src = 0; src < numNode - 1; src++) {
		for (int dst = src + 1; dst < numNode; dst++) {
		    for (i = src; i < dst; i++) {
			nd[i]->ragent_->addRtEntry(nd[dst]->getAddr(),
						   nd[i +
						      1]->getAddr(), 0);
			nd[i +
			   1]->ragent_->addRtEntry(nd[src]->getAddr(),
						   nd[i]->getAddr(), 0);
		    }
		}
	    }
	} else if (!strcmp(argv[1], "manu-rt-star")) {
	    for (i = 1; i < numNode - 1; i++) {
		nd[0]->ragent_->addRtEntry(nd[i]->getAddr(),
					   nd[i]->getAddr(), 0);
		nd[i]->ragent_->addRtEntry(nd[0]->getAddr(),
					   nd[0]->getAddr(), 0);
	    }
	} else {
	    fprintf(stderr, "%s: command is not recognized.\n", argv[1]);
	    return TCL_ERROR;
	}

	delete[]nd;
	return TCL_OK;

    } else if (argc > 2 && !strcmp(argv[1], "pico-range")) {
	int numNode = argc - 5;
	WNode **nd = getNodes(argc, argv, numNode, argv[1]);
	double x1 = atof(argv[2]);
	double y1 = atof(argv[3]);
	double x2 = atof(argv[4]);
	double y2 = atof(argv[5]);

	for (int i = 0; i < numNode; i++) {
	    nd[i]->setRange(x1, y1, 0, x2, y2, 0);
	}

	delete[]nd;
	return (TCL_OK);

    } else if (argc > 2 && !strcmp(argv[1], "pico-range3")) {
	int numNode = argc - 7;
	WNode **nd = getNodes(argc, argv, numNode, argv[1]);
	double x1 = atof(argv[2]);
	double y1 = atof(argv[3]);
	double z1 = atof(argv[4]);
	double x2 = atof(argv[5]);
	double y2 = atof(argv[6]);
	double z2 = atof(argv[7]);

	for (int i = 0; i < numNode; i++) {
	    nd[i]->setRange(x1, y1, z1, x2, y2, z2);
	}

	delete[]nd;
	return (TCL_OK);

    } else if (argc > 2 && !strcmp(argv[1], "dstNodes")) {
	for (int i = 2; i < argc; i++) {
	    WNode *nd = lookupNode(atoi(argv[i]));
	    if (!nd) {
		nd = (WNode *) TclObject::lookup(argv[i]);
	    }
	    if (!nd || !nd->stat_) {
		fprintf(stderr, "dstNodes: node lookup fails: %s\n",
			argv[i]);
		return (TCL_ERROR);
	    }
	    nd->stat_->isDst_ = 1;
	}
	return (TCL_OK);

    } else if (!strncmp(argv[1], "trace-", 6)) {
	int append = 0;
	if (argc > 3 && (argv[3][0] == 'a' || argv[3][0] == 'A')) {
	    append = 1;
	}
	return (setTrace(argv[1], argv[2], append) ? TCL_OK : TCL_ERROR);

    } else if (!strcmp(argv[1], "set-statist")) {
	if (argc < 5) {
	    fprintf(stderr,
		    "$nd set-statist begin-T end-T step [adjustment]\n");
	    return TCL_ERROR;
	}
	double begin = atof(argv[2]);
	double end = atof(argv[3]);
	double step = atof(argv[4]);
/*
	if (stat_) {
	    delete stat_;
	}
	stat_ = new BtStat(begin, end, step, addr_);
*/
	stat_->reset(begin, end, step, addr_);

	if (argc >= 6) {
	    if (!strcmp(argv[5], "adjust-l2cap-hdr")) {
		stat_->proto_adj_ = 4;	// add 4 bytes of l2cap hdr
	    } else {
		stat_->proto_adj_ = atoi(argv[5]);
	    }
	}
	return (TCL_OK);

    } else if (argc == 2) {

	if (!strcmp(argv[1], "print-stat")) {
	    printStat();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "print-all-stat")) {
	    printAllStat();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "reset-energy")) {
	    energyReset();
	    return TCL_OK;

	} else if (!strcmp(argv[1], "reset-energy-allnodes")) {
	    energyResetAllNodes();
	    return TCL_OK;
	}

    } else if (argc == 3) {

	if (!strcasecmp(argv[1], "log-target")) {
	    log_target_ = (Trace *) TclObject::lookup(argv[2]);
	    if (log_target_ == 0) {
		return TCL_ERROR;
	    }
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "radioRange")) {
	    setRadioRange(atof(argv[2]));
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "CollisionDist")) {
	    setCollisionDist(atof(argv[2]));
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "ifqtype")) {
	    ifqType_ = new char[strlen(argv[2]) + 1];
	    strcpy(ifqType_, argv[2]);
	    return TCL_OK;

	} else if (!strcasecmp(argv[1], "ifqLimit")) {
	    ifqLimit_ = atoi(argv[2]);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "get-ifq")) {
	    WNode *dest = (WNode *) TclObject::lookup(argv[2]);
	    getIFQ(dest);
	    return TCL_OK;
	}

    } else if (argc == 4) {

	// cmd toroidal-distance x-max y-max
	// if x-max < 0, it is turned off, that is Euclidian distance is used.
	// Please make sure Baseband::toroidal_x_ is the width of the region
	// (x_max) and toroidal_y_ is the height (y_max).
	if (!strcasecmp(argv[1], "toroidal-distance")) {
	    setToroidalDist(atof(argv[2]), atof(argv[3]));
	    return TCL_OK;

	} else if (!strcmp(argv[1], "set-ifq")) {
	    WNode *dest = (WNode *) TclObject::lookup(argv[2]);
	    Queue *q = (Queue *) TclObject::lookup(argv[3]);
	    setIFQ(dest, q);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "pos")) {
	    double x = atof(argv[2]);
	    double y = atof(argv[3]);
	    setPos(x, y);
	    fprintf(stderr, "Node %d new pos (%f, %f)\n", address(), x, y);
	    return TCL_OK;

	} else if (!strcmp(argv[1], "add-rtenty")) {
	    ragent_->addRtEntry(atoi(argv[2]), atoi(argv[3]), 0);
	    return (TCL_OK);
	}

    } else if (argc == 5) {

	if (!strcmp(argv[1], "setdest")) {
	    double x = atof(argv[2]);
	    double y = atof(argv[3]);
	    double s = atof(argv[4]);
	    setdest(x, y, 0.0, s);
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "pos")) {
	    double x = atof(argv[2]);
	    double y = atof(argv[3]);
	    double z = atof(argv[4]);
	    setPos(x, y, z);
	    return (TCL_OK);
	}

    } else if (argc == 6) {
	if (!strcmp(argv[1], "range")) {
	    double x1 = atof(argv[2]);
	    double y1 = atof(argv[3]);
	    double x2 = atof(argv[4]);
	    double y2 = atof(argv[5]);
	    setRange(x1, y1, 0.0, x2, y2, 0.0);
	    return (TCL_OK);

	} else if (!strcmp(argv[1], "setdest")) {
	    double x = atof(argv[2]);
	    double y = atof(argv[3]);
	    double z = atof(argv[4]);
	    double s = atof(argv[5]);
	    setdest(x, y, z, s);
	    return (TCL_OK);
	}

    } else if (argc == 8) {
	if (!strcmp(argv[1], "range")) {
	    double x1 = atof(argv[2]);
	    double y1 = atof(argv[3]);
	    double z1 = atof(argv[4]);
	    double x2 = atof(argv[5]);
	    double y2 = atof(argv[6]);
	    double z2 = atof(argv[7]);
	    setRange(x1, y1, z1, x2, y2, z2);
	    return (TCL_OK);
	}
    }

    return Node::command(argc, argv);
}

void WNode::printStat()
{
}

void WNode::printAllStat()
{
    WNode *wk = this;
    do {
	wk->printStat();
    } while ((wk = wk->getNext()) != this);

    printAllStatExtra();
}

void WNode::energyResetAllNodes()
{
    WNode *wk = this;
    do {
	wk->energyReset();
    } while ((wk = wk->getNext()) != this);
}

WNode *WNode::lookupNode(int n)
{
    WNode *wk = this;

    do {
	if (wk->getAddr() == n) {
	    return wk;
	}
    } while ((wk = wk->getNext()) != this);

    return NULL;
}

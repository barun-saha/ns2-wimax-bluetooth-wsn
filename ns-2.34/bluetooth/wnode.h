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

#ifndef __ns_wnode_h__
#define __ns_wnode_h__

#include "mobilenode.h"
#include "statis.h"

#define WNODE_RANGE 7
#define WNODE_EFF_DISTANCE 11.2
#define WNODE_INTERFERE_DISTANCE 22

#define MAXTRACEFILES 16
#define WNODE_IFQ_TYPE "Queue/DropTail"
#define WNODE_IFQ_LIMIT -1

class WNode;
class RoutingIF {
  public:
    RoutingIF():node_(0) {}
    virtual ~RoutingIF() {}

    virtual void sendInBuffer(nsaddr_t dst) = 0;
    virtual nsaddr_t nextHop(nsaddr_t dst) = 0;
    // virtual void start() = 0;
    virtual void addRtEntry(nsaddr_t dst, nsaddr_t nexthop, int flag) = 0;
    virtual void delRtEntry(nsaddr_t nexthop) = 0;
    virtual void linkFailed(Packet * p) { Packet::free(p); }
    // virtual void flagLinkUp(nsaddr_t dst) = 0;

    inline void setNode(WNode * n) { node_ = n; }

  protected:
    WNode * node_;		// A pointer to the Node I attached.
};

class WNodeTimer:public Handler {
  public:
    WNodeTimer(class WNode * nd):node_(nd) {}
    void handle(Event *);

  private:
    class WNode * node_;
};

class WNode:public Node {
    friend class WNodeTimer;
    friend class Statistic;

  public:
    WNode();
    virtual ~WNode() = 0;
    virtual WNode *getNext() { return next_; }
    virtual void setNext(WNode * n) { next_ = n; }

    static char *ifqType() { return ifqType_; }
    static int ifqLimit() { return ifqLimit_; }

    void setAddr(int ad) { addr_ = ad; }
    int getAddr() { return addr_; }
    MobileNode *mobilenode() { return wifi_; }
    void set_mobilenode(MobileNode * m) { wifi_ = m; }
    void setRagent(RoutingIF * r) { ragent_ = r; }
    RoutingIF *getRagent() { return ragent_; }
    virtual Statistic *getStat() { return stat_; }

    int command(int, const char *const *);
    virtual void on();
    WNode *lookupNode(int n);

    void recordRecv(int nbytes, int dst, int dport, int hopcnt,
		    double flow_ts, int flow_seq, double flow_ts_lasthop,
		    int flow_seq_lasthop) {
	stat_->recv(nbytes, dst, dport, hopcnt, flow_ts, flow_seq,
		    flow_ts_lasthop, flow_seq_lasthop);
    }

    void recordSend(int nbytes, int dst, int port, int *hopcnt, double *flow_ts,
		    int *flow_seq, double *flow_ts_lasthop,
		    int *flow_seq_lasthop, int isSrc) {
	stat_->send(nbytes, dst, port, hopcnt, flow_ts, flow_seq, 
		    flow_ts_lasthop, flow_seq_lasthop, isSrc);
    }

    WNode **getNodes(int argc, const char *const *argv, int numNode, 
		const char* const cmd); 

    void setToroidalDist(double x, double y, double z = 0) {
	toroidal_x_ = x; toroidal_y_ = y; toroidal_z_ = z;
    }
    void setCollisionDist(double d) {collisionDist_ = d;}
    void setRadioRange(double d) {radioRange_ = d;}

    virtual void flushPkt(int addr) {}
    virtual void printStat();
    void printAllStat();
    virtual void printAllStatExtra() {}
    void energyResetAllNodes();
    virtual void energyReset() {}
    void setRange(double x1, double y1, double z1, 
		  double x2, double y2, double z2);
    virtual void setIFQ(WNode *rmt, Queue *q) {}
    virtual void getIFQ(WNode *rmt) {}

    virtual int setTrace(const char *cmdname, const char *arg, int) {return 0;}
    int setTraceStream(FILE ** tfileref, const char *fname,
		       const char *cmd, int);

    virtual void setdest(double destx, double desty, double destz, 
		         double speed) {}

    double X() { return X_; }
    double Y() { return Y_; }
    double Z() { return Z_; }
    void setPos(double x, double y, double z = 0);

    inline double distance(double x1, double y1, double z1, 
			   double x2, double y2, double z2) {
        if (toroidal_x_ < 0) {
            return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)
			 + (z1 - z2) * (z1 - z2));
        }
        double xx = (x1 < x2 ? x2 - x1 : x1 - x2);
        double yy = (y1 < y2 ? y2 - y1 : y1 - y2);
        double zz = (z1 < z2 ? z2 - z1 : z1 - z2);
        if (toroidal_x_ - xx < xx && toroidal_x_ - xx >= 0) {
                xx = toroidal_x_ - xx;
        }
        if (toroidal_y_ - yy < yy && toroidal_y_ - yy >= 0) {
                yy = toroidal_y_ - yy;
        }
        if (toroidal_z_ - zz < zz && toroidal_z_ - zz >= 0) {
                zz = toroidal_z_ - zz;
        }
        return sqrt(xx * xx + yy * yy + zz * zz);
    }
    inline double distance(double x, double y, double z) {
        return distance(x, y, z, X_, Y_, Z_);
    }
    inline double distance(WNode *bb) {
        return distance(bb->X_, bb->Y_, bb->Z_, X_, Y_, Z_);
    }

    void dump_str(const char *s) {
	unsigned int len = strlen(s);
	for (unsigned int i = 0; i < len; i++) {
	    putchar(s[i]);
	    printf("[%d] ", s[i]);
	}
	printf("(len:%u)\n", len);
    }

    // class ScatFormator * scatFormator_;
  public:
    double radioRange_;
    double collisionDist_;

    // toroidal distance is an artificial distance which removes
    // border/corner effect.
    //
    // if they are set to a value > 0, then toroidal distance instead of
    //   Euclidian distance is used.  See,
    //   Christian Bettstetter, On the Minimum Node Degree and Connectivity
    //   of a Wireless Multihop Network.  MobiHoc'02 June 9-11, 2002,
    //   EPF Lausanne, Switzerland.
    double toroidal_x_;  // width of the area, or x_max_.
    double toroidal_y_;  // heighth of the area, or y_max_.
    double toroidal_z_;  

    double x1_range;
    double y1_range;
    double z1_range;

    double x2_range;
    double y2_range;
    double z2_range;

    double X_;
    double Y_;
    double Z_;
    // double speed_;
    double dX_;
    double dY_;
    double dZ_;

    int addr_;
    RoutingIF *ragent_;
    Statistic * stat_;

    static char *ifqType_;
    static int ifqLimit_;

  protected:
    double initDelay_;
    int on_;

    static int numFileOpened_;
    static const char *tracefilename_[MAXTRACEFILES];
    static FILE *tracefile_[MAXTRACEFILES];
    static char filenameBuff_[4096];
    static int filenameBuffInd_;

    FILE *logfile_;
    WNodeTimer timer_;
    Event on_ev_;

    Trace *log_target_;

  private:
    static WNode *chain_;
    WNode * next_;
    MobileNode *wifi_;
};

#endif

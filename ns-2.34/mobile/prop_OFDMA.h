#ifndef __prop_OFDMA_h__
#define __prop_OFDMA_h__


#include "trace.h"
#include "packet-stamp.h"
//#include "wireless-phy.h"
#include "propagation.h"
//#include "tworayground.h"
#include "cost231.h"
#include "timer-handler.h"
#include <string>

#include "mac802_16.h"

class Propagation;
class PropOFDMA;
struct PacketRecord;

#define NUM_REALIZATIONS 1000
#define NUM_SUBCARRIERS 1024     /* Maximum supported number of subcarriers */ 
#define NUM_DATA_SUBCARRIERS 841
#define MAX_OFDM_SYMBOLS 100

/* 2-d array of ofdmsymbol * subcarriers tht keep track of the powers on subcarriers pertaining to ofdm symbols over which the packet was received.
 * Used for collision detection and interference. 
 */
static double channel_gain[NUM_REALIZATIONS][NUM_SUBCARRIERS+1] = {{0}};
static double total_power_[MAX_OFDM_SYMBOLS][NUM_SUBCARRIERS] = {{0}} ; 

string SelectedPDP="";

typedef double intpower[NUM_SUBCARRIERS];

// This class defines timers for updating the total received power at the receiver
class PacketTimer : public TimerHandler {
 public:
  PacketTimer( PropOFDMA *prop, PacketRecord *ptr ) : TimerHandler() {
    prop_=prop; record_=ptr; 
  }
 protected:
  virtual void expire(Event *e);
  PropOFDMA *prop_;
  PacketRecord *record_;
};

// This structure stores information about a packet currently being received.  
// These structures are dynamically stored in a doubly linked list during simulation 
struct PacketRecord {
  PacketRecord( PropOFDMA *prop ) : timer( prop, this ) {
    return;
  }
  PacketTimer timer;
  PacketRecord *prev;
  PacketRecord *next;
  double power [MAX_OFDM_SYMBOLS][NUM_SUBCARRIERS];
};

/*
 * Class defining Propagation for OFDMA
 */
class PropOFDMA : public Cost231 {
 public:
  PropOFDMA();
  virtual double Pr(PacketStamp *tx, PacketStamp *rx, WirelessPhy *ifp, Packet *p);
  //virtual double Pr(PacketStamp *tx, PacketStamp *rx, WirelessPhy *ifp);

  //       double envelope_factor(PacketStamp *tx, PacketStamp *rx, WirelessPhy *ifp);
  virtual int command(int argc, const char*const* argv);
  ~PropOFDMA();
  // double * GetChannel(int channel_Num_);  
  //double  GetInitialChannel()         ;
  int MyTest_; //RPI
  void removePower(PacketRecord*);

 protected:
  //int LoadDataFile(const char *filename);
  void LoadDataFile();
  void Init_Channels();
  //double channel_gain[NUM_REALIZATIONS][NUM_SUBCARRIERS + 1];
  /*
   * Configured via TCL
   */
  double  max_velocity;       /* Maximum velocity of vehicle/objects in 
				 environment.  Used for computing doppler */

  /* Internal values */
  int N;                  /* Num points in table */
  float fm0;              /* Max doppler freq in table */
  float fm;               /* Max doppler freq in scenario */
  float fs;               /* Sampling rate */
  float dt;               /* Sampling period = 1/fs */
	
	
  float *data1;           /* Data values for inphase and quad phase */
  float *data2;
  bool initialized_;

  Trace  *trtarget_;

  void trace(char *fmt, ...);

  int TestVar;

  /* Variables and functions for keeping track of received packets */
  PacketRecord *head_pkt_;
  PacketRecord *tail_pkt_;
  PacketRecord *trash_pkt_;
  //   static intpower *total_power_;
  void addPower( intpower *pwr, double duration );
  double ReceiveCombining(int , int , int , int );

  int GetRandChannel( int );

  /* 2-d array of ofdmsymbol * subcarriers tht keep track of the powers on subcarriers pertaining to ofdm symbols over which the packet was received.
   * Used for collision detection and interefernce. 
   */ 
   
  intpower *intpower_;
  double *Pr_subcarrier; // calcualting average recv power  
};

#endif 


/* This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and
 * is in the public domain.
 * NIST assumes no responsibility whatsoever for its use by other parties,
 * and makes no guarantees, expressed or implied, about its quality,
 * reliability, or any other characteristic.
 * <BR>
 * We would appreciate acknowledgement if the software is used.
 * <BR>
 * NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
 * DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE USE OF THIS SOFTWARE.
 * </PRE></P>
 * @author  rouil
 */

#ifndef MAC802_16_H
#define MAC802_16_H
//Should not hard code those define. Just edit the Makefile and add -DDEBUG_WIMAX and/or -DDEBUG_WIMAX_EXT
#define DEBUG_WIMAX
#define DEBUG_WIMAX_EXT

#include "sduclassifier.h"
#include "connectionmanager.h"
#include "serviceflowhandler.h"
#include "serviceflowqos.h"
#include "peernode.h"
#include "mac.h"
#include "mac802_16pkt.h"
#include "mac802_16timer.h"
#include "scheduling/framemap.h"
#include "scheduling/contentionslot.h"
#include "scheduling/dlsubframetimer.h"
#include "scheduling/ulsubframetimer.h"
#include "neighbordb.h"
#include "wimaxneighborentry.h"


//Define new debug function for cleaner code
#ifdef DEBUG_WIMAX
#define debug2 printf 
#else
#define debug2(arg1,...) 
#endif

//Define additional debug; level 10 (scheduling and ranging details)
#ifdef DEBUG_WIMAX_EXT
#define debug10 printf
#else
#define debug10(arg1,...) 
#endif

#define BS_NOT_CONNECTED -1 //bs_id when MN is not connected

#define DL_PREAMBLE 3  			//preamble+fch
#define INIT_RNG_PREAMBLE 0         	// no preamble for ranging
#define BW_REQ_PREAMBLE 0           	// no preamble for bw req

#define BASE_POWER  1e-18  //rpi
#define DOUBLE_INT_ZERO 1e-30

/** Defines different types of nodes */
enum station_type_t {
  STA_UNKNOWN,
  STA_MN,
  STA_BS
};

//rpi 

#define HORIZONTAL_STRIPPING 0
#define VERTICAL_STRIPPING 1

 typedef double power[1024];
//rpi


/** MAC MIB */
class Mac802_16MIB {
 public: 
  Mac802_16MIB (Mac802_16 *parent);
 
  int queue_length;
  double frame_duration;

  double dcd_interval;
  double ucd_interval;
  double init_rng_interval;
  double lost_dlmap_interval;
  double lost_ulmap_interval;
  
  double t1_timeout;
  double t2_timeout;
  double t3_timeout;
  double t6_timeout;
  double t12_timeout;
  double t16_timeout;
  double t17_timeout;
  double t21_timeout;
  double t44_timeout;

  u_int32_t contention_rng_retry;
  u_int32_t invited_rng_retry;
  u_int32_t request_retry;
  u_int32_t reg_req_retry;
  double    tproc;
  u_int32_t dsx_req_retry;
  u_int32_t dsx_rsp_retry;

  u_int32_t rng_backoff_start;
  u_int32_t rng_backoff_stop;
  u_int32_t bw_backoff_start;
  u_int32_t bw_backoff_stop;

  u_int32_t init_contention_size;
  u_int32_t bw_req_contention_size;

  u_int32_t cdma_code_bw_start;
  u_int32_t cdma_code_bw_stop;
  u_int32_t cdma_code_init_start;
  u_int32_t cdma_code_init_stop;
  u_int32_t cdma_code_cqich_start;
  u_int32_t cdma_code_cqich_stop;
  u_int32_t cdma_code_handover_start;
  u_int32_t cdma_code_handover_stop;

  //mobility extension
  u_int32_t scan_duration;
  u_int32_t interleaving;
  u_int32_t scan_iteration;
  u_int32_t max_dir_scan_time;
  double    nbr_adv_interval;
  u_int32_t scan_req_retry;

  //miscalleous
  double rxp_avg_alpha;  //for measurements
  double lgd_factor_; 
  double RXThreshold_;
  double client_timeout; //used to clear information on BS side
  int ITU_PDP_; // To get value of ITU-PDP model being used

};

/** PHY MIB */
class Phy802_16MIB {
 public: 
  Phy802_16MIB (Mac802_16 *parent);

//RPI MIMO 
  unsigned char MIMO_enable:1;
  unsigned char num_antenna;
//RPI MIMO
  int channel; //current channel
  double fbandwidth;
  u_int32_t ttg; 
  u_int32_t rtg;
  u_int32_t dl_perm;
  u_int32_t ul_perm;
  int disableInterference;
};


// This struct stores info about a packt currently being recvd. These structs are dynamically stored in a doubly linked list during simulation.  
struct PacketTimerRecord {
        PacketTimerRecord( Mac802_16 *m ) : timer( m, this ) {
                return;
        }
        WimaxRxTimer timer;
        PacketTimerRecord *prev;
        PacketTimerRecord *next;
        Packet *p;
};


class WimaxScheduler;
class FrameMap;
class StatTimer;
class DlTimer;
class UlTimer;

/**
 * Class implementing IEEE 802_16
 */ 
class Mac802_16 : public Mac {

  friend class PeerNode;
  friend class SDUClassifier;
  friend class WimaxFrameTimer;
  friend class FrameMap;
  friend class WimaxScheduler;
  friend class BSScheduler;
  friend class SSscheduler;
  friend class ServiceFlowHandler;
  friend class Connection;
  friend class StatTimer;
  friend class InitTimer;
 public:

  Mac802_16();

  /**
   * Return the connection manager
   * @return the connection manager
   */
  inline ConnectionManager *  getCManager () { return connectionManager_; }
  
  /**
   * Return The Service Flow handler
   * @return The Service Flow handler
   */
  inline ServiceFlowHandler *  getServiceHandler () { return serviceFlowHandler_; }
  
  /**
   * Return the Scheduler
   * @return the Scheduler
   */
  inline WimaxScheduler * getScheduler () { return scheduler_; }

  /**
   * Return the frame duration (in s)
   * @return the frame duration (in s)
   */
  double  getFrameDuration () { return macmib_.frame_duration; }
  
  /**
   * Set the frame duration
   * @param duration The frame duration (in s)
   */
  void  setFrameDuration (double duration) { macmib_.frame_duration = duration; }
  
  /**
   * Return the current frame number
   * @return the current frame number
   */
  int getFrameNumber ();

  /**
   * Return the type of MAC
   * @return the type of node
   */
  station_type_t getNodeType();

  /**
   * Interface with the TCL script
   * @param argc The number of parameter
   * @param argv The list of parameters
   */
  virtual int command(int argc, const char*const* argv);

  /**
   * Change the channel
   * @param channel The new channel
   */
  void setChannel (int channel);

  /**
   * Return the channel index
   * @return The channel
   */
  int getChannel ();

  /**
   * Return the channel number for the given frequency
   * @param freq The frequency
   * @return The channel number of -1 if the frequency does not match
   */
  int getChannel (double freq);

  /**
   * Set the channel to the next from the list
   * Used at initialisation and when loosing signal
   */
  void nextChannel ();

  /**
   * Process packets going out
   * @param p The packet to transmit
   */
  virtual void sendDown(Packet *p);

  /**
   * Process packets going out
   * @param p The packet to transmit
   */
  virtual void transmit(Packet *p);

  virtual u_char get_diuc();
        
  /**
   * Process incoming packets 
   * @param p The received packet
   */
  virtual void sendUp(Packet *p);

  /**
   * Process the packet after receiving last bit
   */
  //virtual void receive();

  /**
   * Process the packet after receiving last bit
   * @param p the packet to be received
   */
  virtual void receive(Packet *p);

  /**
   * Set the variable used to find out if upper layers
   * must be notified to send packets. During scanning we
   * do not want upper layers to send packet to the mac.
   * @param notify Value indicating if we want to receive packets 
   * from upper layers
   */
  void setNotify_upper (bool notify);

  /**
   * Return the head of the peer nodes list
   * @return the head of the peer nodes list
   */
  PeerNode * getPeerNode_head () { return peer_list_->lh_first; }

  /**
   * Return the peer node that has the given address
   * @param index The address of the peer
   * @return The peer node that has the given address
   */
  PeerNode *getPeerNode (int index);

  /**
   * Add the peer node
   * @param The peer node to add
   */
  void addPeerNode (PeerNode *node);

  /**
   * Remove a peer node
   * @param The peer node to remove
   */
  void removePeerNode (PeerNode *node);

  /**
   * Return the number of peer nodes
   */
  int getNbPeerNodes ();

  /**
  * rpi- record a packet 
  */
  void addPacket( Packet *p); 

  /**
   * rpi - remove a packet from the record. 
   */
//  void removePacket(Packet *p); 
  void removePacket(PacketTimerRecord*);


  /**
   * Start a new DL subframe
   */
  virtual void start_dlsubframe ();
  
  /**
   * Start a new UL subframe
   */
  virtual void start_ulsubframe ();

  /**
   * Called when a timer expires
   * @param The timer ID
   */
  virtual void expire (timer_id id);

  /**
   * Return the MAP of the current frame
   * @return the MAP of the current frame
   */
  FrameMap *getMap () { return map_;}

  /**
   * Return the PHY layer
   * @return The physical layer
   */
  OFDMAPhy* getPhy ();

  /**
   * The MAC MIB
   */
   Mac802_16MIB macmib_;

   /**
    * The Physical layer MIB
    */
   Phy802_16MIB phymib_;

//rpi

  /**
   *  Set the total dl duration left after preamble for dl timer 
   */
   inline void setMaxDlduration (int maxdlduration){ MaxDlduration_ = maxdlduration; }

  /*
   *  Get the dl duration 
   */
   inline int getMaxDlduration() { return MaxDlduration_;}

  /**
   *  Set the total dl duration left after preamble for dl timer 
   */
   inline void setMaxUlduration (int maxulduration){ MaxUlduration_ = maxulduration; }

  /*
   *  Get the dl duration 
   */
    inline int getMaxUlduration() { return MaxUlduration_;} 

  /**
   *  Set the total dl duration left after preamble for dl timer 
   */
    inline void setStartUlduration (int ulduration){ Ulduration_ = ulduration; }

  /*
   *  Get the dl duration 
   */
    inline int getStartUlduration() { return Ulduration_;}

 /* 
  *   get the ITU_PDP model 
  */ 
    inline int getITU_PDP () { return macmib_.ITU_PDP_; } // 1 - PEDA, 2 - PEDB, 3 -VEHIC


//rpi

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
   /* 
    * Configure/Request configuration
    * The upper layer sends a config object with the required 
    * new values for the parameters (or PARAMETER_UNKNOWN_VALUE).
    * The MAC tries to set the values and return the new setting.
    * For examples if a MAC does not support a parameter it will
    * return  PARAMETER_UNKNOWN_VALUE
    * @param config The configuration object
    */ 
   void link_configure (link_parameter_config_t* config);

   /* 
    * Configure the threshold values for the given parameters
    * @param numLinkParameter number of parameter configured
    * @param linkThresholds list of parameters and thresholds
    */
   struct link_param_th_status * link_configure_thresholds (int numLinkParameter, struct link_param_th *linkThresholds); //configure threshold
        
   /*
    * Disconnect from the PoA
    */
   virtual void link_disconnect ();

   /*
    * Connect to the PoA
    * @param poa The address of PoA
    */
   virtual void link_connect (int poa);
   
   /*
    * Scan channel
    * @param req the scan request information
    */
   virtual void link_scan (void *req);

   /*
    * Set the operation mode
    * @param mode The new operation mode
    * @return true if transaction succeded
    */
   bool set_mode (mih_operation_mode_t mode); 
#endif
   
 protected:
   /**
    * Init the MAC
    */
   virtual void init ();

   /**
    * The packet scheduler
    */
   WimaxScheduler * scheduler_;
   
   /**
    * Return a new allocated packet
    * @return A newly allocated packet 
    */
   Packet * getPacket();

   /*
    * Return the code for the frame duration
    * @return the code for the frame duration
    */
   int getFrameDurationCode ();
   
   /**
    * Set the frame duration using code
    * @param code The frame duration code
    */
   void setFrameDurationCode (int code);
   
   /**
    * Add a classifier
    */
   void addClassifier (SDUClassifier *);

   /**
    * Run the packet through the classifiers
    * to find the proper connection
    * @param p the packet to classify
    */
   int classify (Packet *p);
  
  /**
   * Timer to init the MAC
   */
  InitTimer *initTimer_;

   /** 
    * The map of the frame
    */
   FrameMap *map_;

   /**
    * Current frame number
    */
   int frame_number_;
   
   /**
    * Timer used to mark the begining of downlink subframe (i.e new frame)
    */
   DlTimer *dl_timer_;
   
   /**
    * Timer used to mark the begining of uplink subframe
    */
   UlTimer *ul_timer_;

   /**
    * Statistics for queueing delay
    */
   StatWatch delay_watch_; 
   
   /**
    * Delay for last packet
    */
   double last_tx_delay_;

   /**
    * Statistics for delay jitter 
    */
   StatWatch jitter_watch_;
   
   /**
    * Stats for packet loss
    */
   StatWatch loss_watch_;

   /**
    * Stats for incoming data throughput
    */
   ThroughputWatch rx_data_watch_;

   /**
    * Stats for incoming traffic throughput (data+management)
    */
   ThroughputWatch rx_traffic_watch_;


   /**
    * Stats for outgoing data throughput
    */
   ThroughputWatch tx_data_watch_;

   /**
    * Stats for outgoing traffic throughput (data+management)
    */
   ThroughputWatch tx_traffic_watch_;

   /**
    * Timers to continuously poll stats in case it is not updated by
    * sending or receiving packets
    */
   StatTimer *rx_data_timer_;
   StatTimer *rx_traffic_timer_;
   StatTimer *tx_data_timer_;
   StatTimer *tx_traffic_timer_;

   /**
    * Indicates if the stats must be printed
    */
   int print_stats_;


   /**
     * Record the number of SS.
     */
     int reg_SS_number;


   /**
     * Rec delta time
    */
     double recv_delta;
     double r_delta_bs;
     double r_delta_ss;
   
   /**
    * Update the given timer and check if thresholds are crossed
    * @param watch the stat watch to update
    * @param value the stat value
    */
   virtual void update_watch (StatWatch *watch, double value);

   /**
    * Update the given timer and check if thresholds are crossed
    * @param watch the stat watch to update
    * @param size the size of packet received
    */
   virtual void update_throughput (ThroughputWatch *watch, double size);

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
   /**
    * Poll the given stat variable to check status
    * @param type The link parameter type
    */
   void poll_stat (link_parameter_type_s type);
#endif

   /** 
    * Log the packet. Private in Mac so we need to redefine it
    * @param p The received packet
    */
   inline void mac_log(Packet *p) {
     logtarget_->recv(p, (Handler*) 0);
   }
   

/**
  * rpi packet record to keep a record of packets and thr timers when multipl packets r recvd in OFDMA
  */
   PacketTimerRecord *head_pkt_;
   PacketTimerRecord *tail_pkt_;
   PacketTimerRecord *trash_pkt_;


   /** 
    * Object to log received packets. Private in Mac so we need to redefine it
    */
   NsObject*	logtarget_;

   /**
    * Packet being received
    */
   Packet *pktRx_;   // since, now multiple packets can be received at the same time, this pointer object is used.  

   /**
    * A packet buffer used to temporary store a packet 
    * received by upper layer. Used during scanning
    */
   Packet *pktBuf_;

   /**
    * Set the node type
    * @param type The station type
    */
   void setStationType (station_type_t type);

   /*
    * The type of station (MN or BS) 
    */
   station_type_t type_;

   /**
    * Receiving timer
    */
   //WimaxRxTimer rxTimer_;

   /**
    * Indicates if a collision occured
    */
   bool collision_;

   /**
    * Indicate if upper layer must be notified to send more packets
    */
   bool notify_upper_;

   /**
    * Last time a packet was sent
    */
   double last_tx_time_;

   /**
    * Last transmission duration
    */
   double last_tx_duration_;

   /**
    * The class to handle connections
    */
   ConnectionManager * connectionManager_;
   
   /**
    * The module that handles flow requests
    */
   ServiceFlowHandler * serviceFlowHandler_;

   /**
    * List of connected peer nodes. Only one for SSs.
    */
   struct peerNode *peer_list_;

   /**
    * Number of peer in the list
    */
   int nb_peer_;

   /**
    * Database of neighboring BS
    */
   NeighborDB *nbr_db_;

   /**
    * Loss rate for data connections
    * Allows to test loss on data and not management
    */
   double data_loss_;

   /**
    * Allows sending ARQFB in data connection  
    */
   int arqfb_in_dl_data_;
 
   /**
    * Allows sending ARQFB in data connection  
    */
   int arqfb_in_ul_data_;
//Begin RPI
   /**
    * Allows ARQ_BLOCK_SIZE in data connection  
    */
   int arq_block_size_ ;
//End RPI

   /**rpi
    * dlduration left after preamble in num OFDM  symbols
    */
   int MaxDlduration_; 

  
   /**rpi
    * dlduration left after preamble in num OFDM  symbols
    */
   int MaxUlduration_; 

  
   /**rpi
    * Ulduration left after intial ranging and all in num OFDM  symbols
    */
   int Ulduration_;

   /**rpi 
   * dlduration left after preamble in num OFDM symbols
   */ 
   int Dlduration_; 
 
   /**
    * Array to store power per subcarrier, to compute interference
    */
   double **intpower_;
   double **basepower_;

   /*
    * For collision  detection of contention based packet
    * as they are treated differently here. (OFDM based cont slots) 
    */
   bool contPktRxing_;    

 private:
   /**
    * The list of classifier
    */
   struct sduClassifier classifier_list_;
     
};

/** Class to poll stats */
class StatTimer : public TimerHandler {
 public:
  StatTimer (Mac802_16 *mac, ThroughputWatch *watch) : TimerHandler() {
    mac_ = mac;
    watch_ = watch;
    timer_interval_ = 0.100000000001; //default 100ms+a little off to avoid synch
    resched (timer_interval_);
  }
    void expire (Event *) {
      mac_->update_throughput (watch_, 0);
      //double tmp = watch_->get_timer_interval();
      //resched(tmp > 0? tmp: timer_interval_);
    }
    inline void set_timer_interval(double ti) { timer_interval_ = ti; }
 private:
    Mac802_16 *mac_;
    ThroughputWatch *watch_;
    double timer_interval_;
};

#endif //MAC802_16_H




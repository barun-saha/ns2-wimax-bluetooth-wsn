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

#include "mac802_16.h"
#include "scheduling/wimaxscheduler.h"
//we use mac 802_11 for trace
#include "mac-802_11.h"

/* Defines frequencies for 3.5 GHz band and 7 Mhz freqency bandwidth */
/* Will be removed when a dynamic way is added */
static const int nbFreq = 5;
static const double frequencies[] = { 3.486e+9, 3.493e+9, 3.5e+9, 3.507e+9, 3.514e+9};


int hdr_mac802_16::offset_;
/**
 * TCL Hooks for the simulator for wimax packets
 */
static class Mac802_16HeaderClass : public PacketHeaderClass
{
public:
  Mac802_16HeaderClass() : PacketHeaderClass("PacketHeader/802_16",
					     sizeof(hdr_mac802_16))
  {
    bind_offset(&hdr_mac802_16::offset_);
  }
} class_hdr_mac802_16;

/**
 * TCL Hooks for the simulator for wimax mac
 */
static class Mac802_16Class : public TclClass {
public:
  Mac802_16Class() : TclClass("Mac/802_16") {}
  TclObject* create(int, const char*const*) {
    return (new Mac802_16());
    
  }
} class_mac802_16;

Phy802_16MIB::Phy802_16MIB(Mac802_16 *parent)
{
  parent->bind ("channel_", &channel );
  parent->bind ("fbandwidth_", &fbandwidth );
  parent->bind ("ttg_", &ttg );
  parent->bind ("rtg_", &rtg );
  parent->bind ("dl_permutation_", &dl_perm);
  parent->bind ("ul_permutation_", &ul_perm);
  parent->bind ("disable_interference_", &disableInterference);
}

Mac802_16MIB::Mac802_16MIB(Mac802_16 *parent)
{
  parent->bind ("queue_length_", &queue_length );
  parent->bind ("frame_duration_", &frame_duration );
  parent->bind ("dcd_interval_", &dcd_interval );
  parent->bind ("ucd_interval_", &ucd_interval );
  parent->bind ("init_rng_interval_", &init_rng_interval );
  parent->bind ("lost_dlmap_interval_", &lost_dlmap_interval );
  parent->bind ("lost_ulmap_interval_", &lost_ulmap_interval );
  parent->bind ("t1_timeout_", &t1_timeout );
  parent->bind ("t2_timeout_", &t2_timeout );
  parent->bind ("t3_timeout_", &t3_timeout );
  parent->bind ("t6_timeout_", &t6_timeout );
  parent->bind ("t12_timeout_", &t12_timeout );
  parent->bind ("t16_timeout_", &t16_timeout );
  parent->bind ("t17_timeout_", &t17_timeout );
  parent->bind ("t21_timeout_", &t21_timeout );
  parent->bind ("contention_rng_retry_", &contention_rng_retry );
  parent->bind ("invited_rng_retry_", &invited_rng_retry );
  parent->bind ("request_retry_", &request_retry );
  parent->bind ("reg_req_retry_", &reg_req_retry );
  parent->bind ("tproc_", &tproc );
  parent->bind ("dsx_req_retry_", &dsx_req_retry );
  parent->bind ("dsx_rsp_retry_", &dsx_rsp_retry );

  parent->bind ("rng_backoff_start_", &rng_backoff_start);
  parent->bind ("rng_backoff_stop_", &rng_backoff_stop);
  parent->bind ("bw_backoff_start_", &bw_backoff_start);
  parent->bind ("bw_backoff_stop_", &bw_backoff_stop);
  parent->bind ("bw_req_contention_size_", &bw_req_contention_size);
  parent->bind ("init_contention_size_", &init_contention_size);
  parent->bind ("cdma_code_bw_start_", &cdma_code_bw_start);
  parent->bind ("cdma_code_bw_stop_", &cdma_code_bw_stop);
  parent->bind ("cdma_code_init_start_", &cdma_code_init_start);
  parent->bind ("cdma_code_init_stop_", &cdma_code_init_stop);
  parent->bind ("cdma_code_cqich_start_", &cdma_code_cqich_start);
  parent->bind ("cdma_code_cqich_stop_", &cdma_code_cqich_stop);
  parent->bind ("cdma_code_handover_start_", &cdma_code_handover_start);
  parent->bind ("cdma_code_handover_stop_", &cdma_code_handover_stop);

 
  //mobility extension
  parent->bind ("scan_duration_", &scan_duration );
  parent->bind ("interleaving_interval_", &interleaving );
  parent->bind ("scan_iteration_", &scan_iteration );
  parent->bind ("t44_timeout_", &t44_timeout );
  parent->bind ("max_dir_scan_time_", &max_dir_scan_time );
  parent->bind ("nbr_adv_interval_", &nbr_adv_interval );
  parent->bind ("scan_req_retry_", &scan_req_retry );

  parent->bind ("client_timeout_", &client_timeout );
  parent->bind ("rxp_avg_alpha_", &rxp_avg_alpha);
  parent->bind ("lgd_factor_", &lgd_factor_);
  parent->bind ("ITU_PDP_", &ITU_PDP_ );
  
}

/**
 * Creates a Mac 802.16
 * The MAC 802.16 is a superclass for the BS and SS MAC
 * It should not be instanciated
 */
Mac802_16::Mac802_16() : Mac (), macmib_(this), phymib_(this)//, rxTimer_(this)
{
  //init variable
  LIST_INIT(&classifier_list_);
  peer_list_ = (struct peerNode *) malloc (sizeof(struct peerNode));
  LIST_INIT(peer_list_);
  nb_peer_=0;

  collision_ = false;
  pktRx_ = NULL;
  pktBuf_ = NULL;

  //rpi  packet record for receiving 
 
  head_pkt_ = NULL;
  tail_pkt_ = NULL;
  trash_pkt_ = NULL;

  //rpi end

  /* Subclass will create them */
  intpower_ = NULL;  
  basepower_ = NULL;

  contPktRxing_ = false;

  //rpi end
  connectionManager_ = new ConnectionManager (this);
  scheduler_ = NULL;
  /* the following will be replaced by dynamic adding of service flow */
  serviceFlowHandler_ = new ServiceFlowHandler ();
  serviceFlowHandler_->setMac (this);
  frame_number_ = 0;
  notify_upper_ = true;

  last_tx_time_ = 0;
  last_tx_duration_ = 0;

  Tcl& tcl = Tcl::instance();
  tcl.evalf ("Phy/WirelessPhy set RXThresh_");
  macmib_.RXThreshold_ = atof (tcl.result());

  nbr_db_ = new NeighborDB ();
  


#ifdef USE_802_21
  linkType_ = LINK_802_16;
  eventList_ = 0x1BF;
  commandList_ = 0xF;
#endif  

  /* Initialize Stats variables */
  bind_bool ("print_stats_", &print_stats_);
  last_tx_delay_ = 0;
  double tmp;
  bind ("delay_avg_alpha_", &tmp);
  delay_watch_.set_alpha(tmp);
  bind ("jitter_avg_alpha_", &tmp);
  jitter_watch_.set_alpha(tmp);
  bind ("loss_avg_alpha_", &tmp);
  loss_watch_.set_alpha(tmp);
  bind ("throughput_avg_alpha_", &tmp);
  rx_data_watch_.set_alpha(tmp);
  rx_data_watch_.set_pos_gradient (false);
  rx_traffic_watch_.set_alpha(tmp);
  rx_traffic_watch_.set_pos_gradient (false);
  tx_data_watch_.set_alpha(tmp);
  tx_data_watch_.set_pos_gradient (false);
  tx_traffic_watch_.set_alpha(tmp);
  tx_traffic_watch_.set_pos_gradient (false);
  bind ("throughput_delay_", &tmp);
  rx_data_watch_.set_delay (tmp);
  rx_traffic_watch_.set_delay (tmp);
  tx_data_watch_.set_delay (tmp);
  tx_traffic_watch_.set_delay (tmp);
  //timers for stats
  rx_data_timer_ = new StatTimer (this, &rx_data_watch_);
  rx_traffic_timer_ = new StatTimer (this, &rx_traffic_watch_);
  tx_data_timer_ = new StatTimer (this, &tx_data_watch_);
  tx_traffic_timer_ = new StatTimer (this, &tx_traffic_watch_);

  //variable to test loss on data connection
  bind ("data_loss_rate_", &data_loss_);
  bind ("arqfb_in_dl_data_", &arqfb_in_dl_data_);
  bind ("arqfb_in_ul_data_", &arqfb_in_ul_data_);
  bind ("arq_block_size_", &arq_block_size_); /*RPI*/

  recv_delta = 0;
  r_delta_bs = 0;
  r_delta_ss = 0;

  initTimer_ = new InitTimer (this);
  initTimer_->sched (0.001);

}

/*
 * Interface with the TCL script
 * @param argc The number of parameter
 * @param argv The list of parameters
 */
int Mac802_16::command(int argc, const char*const* argv)
{
  int result;
  if (argc == 2) {
    if (strcmp(argv[1], "dump-classifiers") == 0) {
      for (SDUClassifier *n=classifier_list_.lh_first;n;n=n->next_entry()) {
	//debug ("Classifier %x priority=%d\n", (int)n, n->getPriority());
      }
      return TCL_OK;
    }
  }
  else if (argc == 3) {
    if (strcmp(argv[1], "add-classifier") == 0) {
      SDUClassifier *clas = (SDUClassifier*) TclObject::lookup(argv[2]);
      if (clas == 0)
	return TCL_ERROR;
      //add classifier to the list
      addClassifier (clas);
      return TCL_OK;
    } else if (strcmp(argv[1], "set-scheduler") == 0) {
      scheduler_ = (WimaxScheduler*) TclObject::lookup(argv[2]);
      if (scheduler_ == 0)
	return TCL_ERROR;
      scheduler_->setMac (this); //register the mac
      return TCL_OK;
    } else if (strcmp(argv[1], "set-servicehandler") == 0) {
      serviceFlowHandler_ = (ServiceFlowHandler*) TclObject::lookup(argv[2]);
      if (serviceFlowHandler_ == 0)
	return TCL_ERROR;
      serviceFlowHandler_->setMac (this);
      return TCL_OK;
    } else if (strcmp(argv[1], "set-channel") == 0) {
      assert (netif_); //to make sure we can update the phy
      phymib_.channel = atoi (argv[2]);
      double tmp = frequencies[phymib_.channel];
      getPhy ()->setFrequency (tmp);
      return TCL_OK;
    } else if (strcmp(argv[1], "log-target") == 0) { 
      logtarget_ = (NsObject*) TclObject::lookup(argv[2]);
      if(logtarget_ == 0)
	return TCL_ERROR;
      return TCL_OK;
    }
  }
  else if (argc == 21) {
    if (strcmp(argv[1], "setflow") == 0) {
      //debug (" ***********************************argc is 21 is taken \n" );
      result = serviceFlowHandler_->addStaticFlow(argc, argv);
      return result; 
    }
    else {
      return TCL_ERROR;
    } 
  }
  return Mac::command(argc, argv);
}

/**
 * Init the MAC
 */
void Mac802_16::init ()
{
  //define by subclass
}

/**
 * Return the type of MAC
 * @return the type of node
 */
station_type_t Mac802_16::getNodeType()
{
  return type_;
}

/**
 * Return the PHY layer
 * @return The PHY layer
 */
OFDMAPhy* Mac802_16::getPhy () { 
  return (OFDMAPhy*) netif_;
}

/**
 * Change the channel
 * @param channel The new channel
 */
void Mac802_16::setChannel (int channel)
{
  assert (channel < nbFreq);
  phymib_.channel = channel;
  double tmp = frequencies[phymib_.channel];
  getPhy ()->setFrequency (tmp);
}

/**
 * Return the channel number for the given frequency
 * @param freq The frequency
 * @return The channel number of -1 if the frequency does not match
 */
int Mac802_16::getChannel (double freq)
{
  for (int i = 0 ; i < nbFreq ; i++) {
    if (frequencies[i]==freq)
      return i;
  }
  return -1;
}

/**
 * Return the channel index
 * @return The channel
 */
int Mac802_16::getChannel ()
{
  return phymib_.channel;
}

/**
 * Set the channel to the next from the list
 * Used at initialisation and when loosing signal
 */
void Mac802_16::nextChannel ()
{
  debug ("At %f in Mac %d Going to channel %d\n", NOW, index_, (phymib_.channel+1)%nbFreq);
  setChannel ((phymib_.channel+1)%nbFreq);
}
  
/**
 * Set the variable used to find out if upper layers
 * must be notified to send packets. During scanning we
 * do not want upper layers to send packet to the mac.
 */
void Mac802_16::setNotify_upper (bool notify) { 
  debug ("At %f in Mac %d notify %d pktBuf_=%p\n", NOW, index_, notify, pktBuf_);
  notify_upper_ = notify; 
  if (notify_upper_ && pktBuf_) {
    sendDown (pktBuf_);
    pktBuf_ = NULL;
  }
}

/**
 * Return the peer node that has the given address
 * @param index The address of the peer
 * @return The peer node that has the given address
 */
PeerNode* Mac802_16::getPeerNode (int index)
{
  for (PeerNode *p=peer_list_->lh_first;p;p=p->next_entry()) {
    if (p->getAddr ()==index)
      return p;
  }
  return NULL;
}

/**
 * Add the peer node
 * @param The peer node to add
 */
void Mac802_16::addPeerNode (PeerNode *node)
{
  node->insert_entry (peer_list_);
  nb_peer_++;
  //update Rx time so for default value
  node->setRxTime(NOW);
  node->getStatWatch()->set_alpha(macmib_.rxp_avg_alpha);
}

/**
 * Remove the peer node
 * @param The peer node to remove
 */
void Mac802_16::removePeerNode (PeerNode *peer)
{
  //when removing, we give the CID and it removes IN and OUT connections
  if (peer->getBasic(IN_CONNECTION)) {
    getCManager()->remove_connection (peer->getBasic(IN_CONNECTION)->get_cid());
    delete (peer->getBasic(IN_CONNECTION));
    delete (peer->getBasic(OUT_CONNECTION));
  }
  if (peer->getPrimary(IN_CONNECTION)) {
    getCManager()->remove_connection (peer->getPrimary(IN_CONNECTION)->get_cid());
    delete (peer->getPrimary(IN_CONNECTION));
    delete (peer->getPrimary(OUT_CONNECTION));
  }
  if (peer->getSecondary(IN_CONNECTION)) {
    getCManager()->remove_connection (peer->getSecondary(IN_CONNECTION)->get_cid());
    delete (peer->getSecondary(IN_CONNECTION));
    delete (peer->getSecondary(OUT_CONNECTION));
  }
  if (peer->getInData()) {
    getCManager()->remove_connection (peer->getInData()->get_cid());
    delete (peer->getInData());
  }
  if (peer->getOutData()) {
    getCManager()->remove_connection (peer->getOutData()->get_cid());
    delete (peer->getOutData());
  }
  peer->remove_entry ();
  nb_peer_--;
  delete (peer);
}

/**
 * Return the number of peer nodes
 */
int Mac802_16::getNbPeerNodes ()
{
  return nb_peer_;
}


/**** Packet processing methods ****/

/*
 * Process packets going out
 * @param p The packet to send out
 */
void Mac802_16::sendDown(Packet *p)
{
  //handle by subclass
}

/*
 * Transmit a packet to the physical layer
 * @param p The packet to send out
 */
void Mac802_16::transmit(Packet *p)
{
  //handle by subclass
}

u_char Mac802_16::get_diuc() 
{
  //handle by subclass
}
/*
 * Process incoming packets
 * @param p The incoming packet
 */
void Mac802_16::sendUp (Packet *p)
{
  //handle by subclass
}

/**
 * Process the fully received packet
 */
/*void Mac802_16::receive ()
  {
  //handle by subclass
  }
*/
/**
 * Process the fully received packet - changed for OFDMA 
 * @param p The packet to receive  -rpi
 */
void Mac802_16::receive (Packet *p)
{
  //handle by subclass
}


/**** Helper methods ****/

/**
 * Return the frame number
 * @return the frame number
 */
int Mac802_16::getFrameNumber () {
  return frame_number_;
}

/*
 * Return the code for the frame duration
 * @return the code for the frame duration
 */
int Mac802_16::getFrameDurationCode () {
  if (macmib_.frame_duration == 0.0025)
    return 0;
  else if (macmib_.frame_duration == 0.004)
    return 1;
  else if (macmib_.frame_duration == 0.005)
    return 2;
  else if (macmib_.frame_duration == 0.008)
    return 3;
  else if (macmib_.frame_duration == 0.01)
    return 4;
  else if (macmib_.frame_duration == 0.0125)
    return 5;
  else if (macmib_.frame_duration == 0.02)
    return 6;
  else {
    fprintf (stderr, "Invalid frame duration %f\n", macmib_.frame_duration);
    exit (1);
  }
}

/*
 * Set the frame duration using code
 * @param code The frame duration code
 */
void Mac802_16::setFrameDurationCode (int code) 
{
  switch (code) {
  case 0:
    macmib_.frame_duration = 0.0025;
    break;
  case 1:
    macmib_.frame_duration = 0.004;
    break;
  case 2:
    macmib_.frame_duration = 0.005;
    break;
  case 3:
    macmib_.frame_duration = 0.008;
    break;
  case 4:
    macmib_.frame_duration = 0.01;
    break;
  case 5:
    macmib_.frame_duration = 0.0125;
    break;
  case 6:
    macmib_.frame_duration = 0.02;
    break;
  default:
    fprintf (stderr, "Invalid frame duration code %d\n", code);
    exit (1);
  }
}


/**
 * Return a packet 
 * @return a new packet
 */
Packet *Mac802_16::getPacket ()
{
  Packet *p = Packet::alloc ();
  
  hdr_mac802_16 *wimaxHdr= HDR_MAC802_16(p);

  //set header information
  wimaxHdr->cdma = 0;
  wimaxHdr->header.ht = 0;
  wimaxHdr->header.ec = 1;
  wimaxHdr->header.type_mesh = 0;
  wimaxHdr->header.type_arqfb = 0;
  wimaxHdr->header.type_ext = 0;
  wimaxHdr->header.type_frag = 0;
  wimaxHdr->header.type_pck = 0;
  wimaxHdr->header.type_fbgm = 0;
  wimaxHdr->header.ci = 0;
  wimaxHdr->header.eks = 0;
  wimaxHdr->header.cid = BROADCAST_CID; //default
  wimaxHdr->header.hcs = 0;
  HDR_CMN(p)->ptype() = PT_MAC;

  HDR_CMN(p)->size() = HDR_MAC802_16_SIZE;

  return p;
}

/**** Internal methods ****/


/*
 * Add a classifier
 * @param clas The classifier to add
 */
void Mac802_16::addClassifier (SDUClassifier *clas) 
{
  SDUClassifier *n=classifier_list_.lh_first;
  SDUClassifier *prev=NULL;
  int i = 0;
  if (!n || (n->getPriority () >= clas->getPriority ())) {
    //the first element
    //debug ("Add first classifier\n");
    clas->insert_entry_head (&classifier_list_);
  } else {
    while ( n && (n->getPriority () < clas->getPriority ()) ) {
      prev=n;
      n=n->next_entry();
      i++;
    }
    //debug ("insert entry at position %d\n", i);
    clas->insert_entry (prev);
  }
  //Register this mac with the classifier
  clas->setMac (this);
}

/**
 * Run the packet through the classifiers
 * to find the proper connection
 * @param p the packet to classify
 */
int Mac802_16::classify (Packet *p)
{
  int cid = -1;
  for (SDUClassifier *n=classifier_list_.lh_first; n && cid==-1; n=n->next_entry()) {
    cid = n->classify (p);
  }
  return cid;
}

#ifdef USE_802_21

/* 
 * Configure/Request configuration
 * The upper layer sends a config object with the required 
 * new values for the parameters (or PARAMETER_UNKNOWN_VALUE).
 * The MAC tries to set the values and return the new setting.
 * For examples if a MAC does not support a parameter it will
 * return  PARAMETER_UNKNOWN_VALUE
 * @param config The configuration object
 */ 
void Mac802_16::link_configure (link_parameter_config_t* config)
{
  assert (config);
  config->bandwidth = 15000000; //TBD use phy (but depend on modulation)
  config->type = LINK_802_16;
  //we set the rest to PARAMETER_UNKNOWN_VALUE
  config->ber = PARAMETER_UNKNOWN_VALUE;
  config->delay = PARAMETER_UNKNOWN_VALUE;
  config->macPoA = PARAMETER_UNKNOWN_VALUE;
}

/*
 * Disconnect from the PoA
 */
void Mac802_16::link_disconnect ()
{
  //handle by subclass
}

/*
 * Connect to the PoA
 */
void Mac802_16::link_connect (int poa)
{
  //handle by subclass
}

/*
 * Set the operation mode
 * @param mode The new operation mode
 * @return true if transaction succeded
 */
bool Mac802_16::set_mode (mih_operation_mode_t mode)
{
  switch (mode) {
  case NORMAL_MODE:
    if (op_mode_ != NORMAL_MODE) {
      getPhy()->node_on(); //turn on phy
      debug ("Turning on mac\n");
    }
    op_mode_ = mode;
    return true;
    break;
  case POWER_SAVING:
    //not yet supported 
    return false;
    break;
  case POWER_DOWN:
    if (op_mode_ != POWER_DOWN) {
      getPhy()->node_off(); //turn off phy
      debug ("Turning off mac\n");
    }
    op_mode_ = mode;
    return true;
    break;
  default:
    return false;
  }
}



/*
 * Scan channels
 */
void Mac802_16::link_scan (void *req)
{
  //handle by subclass
}


/* 
 * Configure the threshold values for the given parameters
 * @param numLinkParameter number of parameter configured
 * @param linkThresholds list of parameters and thresholds
 */
struct link_param_th_status * Mac802_16::link_configure_thresholds (int numLinkParameter, struct link_param_th *linkThresholds)
{
  struct link_param_th_status *result = (struct link_param_th_status *) malloc(numLinkParameter * sizeof (struct link_param_th_status));
  StatWatch *watch=NULL;
  for (int i=0 ; i < numLinkParameter ; i++) {
    result[i].parameter = linkThresholds[i].parameter;
    result[i].status = 1; //accepted..default
    switch (linkThresholds[i].parameter.parameter_type){
    case LINK_GEN_FRAME_LOSS: 
      watch = &loss_watch_;
      break;
    case LINK_GEN_PACKET_DELAY:
      watch = &delay_watch_;
      break;
    case LINK_GEN_PACKET_JITTER:
      watch = &jitter_watch_;
      break;
    case LINK_GEN_RX_DATA_THROUGHPUT:
      watch = &rx_data_watch_;
      break;
    case LINK_GEN_RX_TRAFFIC_THROUGHPUT:
      watch = &rx_traffic_watch_;
      break;
    case LINK_GEN_TX_DATA_THROUGHPUT:
      watch = &tx_data_watch_;
      break;
    case LINK_GEN_TX_TRAFFIC_THROUGHPUT:
      watch = &tx_traffic_watch_;
      break;
    default:
      debug (stderr, "Parameter type not supported %d/%d\n", 
	     linkThresholds[i].parameter.link_type, 
	     linkThresholds[i].parameter.parameter_type);
      result[i].status = 0; //rejected
    }
    watch->set_thresholds (linkThresholds[i].initActionTh.data_d, 
			   linkThresholds[i].rollbackActionTh.data_d ,
			   linkThresholds[i].exectActionTh.data_d);
  }
  return result;
}
 
#endif

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_16::update_watch (StatWatch *watch, double value)
{
  //handle by subclass
}

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_16::update_throughput (ThroughputWatch *watch, double size)
{
  //handle by subclass
}

/**
 * Start a new DL subframe
 */
void Mac802_16::start_dlsubframe ()
{
  //handle by subclass
}

/**
 * Start a new UL subframe
 */
void Mac802_16::start_ulsubframe ()
{
  //handle by subclass
}

/**
 * Called when a timer expires
 * @param The timer ID
 */
void Mac802_16::expire (timer_id id)
{
  //handle by subclass
}


//rpi linked list of packets to be received 

void
Mac802_16::addPacket( Packet *p ) {
  PacketTimerRecord *ptr;
  //HDR_CMN *ch = HDR_CMN(p);
  // Create a new packet record and set the timer and power for it
  ptr = new PacketTimerRecord(this);
  ptr->p = p;

  double del_inc = 0.000000001;
  double txtime_delta = HDR_CMN(p)->txtime();;
  // Commented by Barun : 22-Sep-2011
  //debug10 ("\t before scheduled event type :%d; at time %lf, r_delta_bs :%lf, r_delta_ss :%lf, recv_delta :%lf\n", type_, HDR_CMN(p)->txtime(), r_delta_bs, r_delta_ss, recv_delta);
  if (type_ == STA_BS) {
        txtime_delta = txtime_delta + r_delta_bs;
        r_delta_bs = r_delta_bs + del_inc;
  } else {
        txtime_delta = txtime_delta + r_delta_ss;
        r_delta_ss = r_delta_ss + del_inc;
  }
  recv_delta = recv_delta + del_inc;
  // Commented by Barun : 22-Sep-2011
  //debug10 ("\t after scheduled event type :%d; at time %lf, sum :%lf, r_delta_bs :%lf, r_delta_ss :%lf, recv_delta :%lf\n", type_, HDR_CMN(p)->txtime(), txtime_delta, r_delta_bs, r_delta_ss, recv_delta);

  ptr->timer.start(txtime_delta);

//  ptr->timer.sched( duration );
//  ptr->timer.start(HDR_CMN(p)->txtime()-0.000000001);

  //debug("\t scheduled interference event\n");
  // Add the new object to the tail of the list (ensures that packet powers
  // will always be added in the same order (probably doesn't matter but...))
  ptr->prev = tail_pkt_;
  ptr->next = NULL;
  if (head_pkt_ == NULL) {
    head_pkt_ = ptr;
  }
  if (tail_pkt_ != NULL) {
    tail_pkt_->next = ptr;
  }
  tail_pkt_ = ptr;

}



void
Mac802_16::removePacket( PacketTimerRecord *expired ) {

  assert( head_pkt_ != NULL );
  assert( tail_pkt_ != NULL );
  // Remove the expired packet 
  if (expired->prev == NULL) {
    head_pkt_ = expired->next;
  } else {
    expired->prev->next = expired->next;
  }
  if (expired->next == NULL){
    tail_pkt_ = expired->prev;
  } else {
    expired->next->prev = expired->prev;
  }

  // Cannot delete the packet record right away because the TimerHandler class
  // has to perform some more housekeeping.  So we move the packet to the
  // "trash" so it can be deleted the next time a packet expires.

  // If there is a packet in the trash, delete it.
  if (trash_pkt_ != NULL) {
    delete trash_pkt_;
  }
  // Move the expired packet to the trash.
  trash_pkt_ = expired;

}



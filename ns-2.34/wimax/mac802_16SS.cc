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
#include <cstdlib> 
#include "mac802_16SS.h"
#include "scheduling/wimaxscheduler.h"
#include "scheduling/ssscheduler.h" 
#include "destclassifier.h"
#include "cmu-trace.h"
#include "random.h"
#include "globalparams_wimax.h" 

/**
 * TCL Hooks for the simulator for wimax mac
 */
static class Mac802_16SSClass : public TclClass {
public:
  Mac802_16SSClass() : TclClass("Mac/802_16/SS") {}
  TclObject* create(int, const char*const*) {
    return (new Mac802_16SS());
    
  }
} class_mac802_16SS;

/**
 * Creates a Mac 802.16
 */
Mac802_16SS::Mac802_16SS() : Mac802_16 ()
{
  type_ = STA_MN; //type of MAC. In this case it is for SS
  
  //Create default configuration
  addClassifier (new DestClassifier ());
  scheduler_ = new SSscheduler();
  scheduler_->setMac (this); //register the mac
  
  init_default_connections ();

  //We can configure each SS with a different modulation
  //default transmission profile (64 QAM 3_4. Can be changed by TCL)
  //The DIUC to use will be determined via an algorithm considering 
  //the channel performance
  default_diuc_ = DIUC_PROFILE_7;

  //initialize state
  state_ = MAC802_16_DISCONNECTED; //At start up, we are not connected.
  //scanning status
  scan_info_ = (struct scanning_structure *) malloc (sizeof (struct scanning_structure));
  memset (scan_info_, 0, sizeof (struct scanning_structure));
  scan_info_->nbr = NULL;
  scan_info_->substate = NORMAL; //no scanning
  scan_flag_ = false;

  map_ = new FrameMap (this);

  //create timers for state machine
  t1timer_ = new WimaxT1Timer (this);
  t2timer_ = new WimaxT2Timer (this);
  t6timer_ = NULL;
  t12timer_ = new WimaxT12Timer (this);
  t21timer_ = new WimaxT21Timer (this);
  lostDLMAPtimer_ = new WimaxLostDLMAPTimer (this);
  lostULMAPtimer_ = new WimaxLostULMAPTimer (this);
  t44timer_ = NULL;

  //create timers for DL/UL boundaries
  dl_timer_ = new DlTimer (this);
  ul_timer_ = new UlTimer (this);

  //initialize some other variables
  nb_reg_retry_ = 0;
  nb_scan_req_ = 0;
}

/*
 * Interface with the TCL script
 * @param argc The number of parameter
 * @param argv The list of parameters
 * @return command status
 */
int Mac802_16SS::command(int argc, const char*const* argv)
{
  if (argc == 3) {
    if (strcmp(argv[1], "set-diuc") == 0) {
      int diuc = atoi (argv[2]);
      if (diuc < DIUC_PROFILE_1 || diuc > DIUC_PROFILE_11)
	return TCL_ERROR;
      default_diuc_ = diuc;
      return TCL_OK;
    } 
  }

  return Mac802_16::command(argc, argv);
}

/**
 * Initialize default connections
 * These connections are not linked to a peer node
 */
void Mac802_16SS::init_default_connections ()
{
  Connection * con;

  //create initial ranging and padding connection
  con = new Connection (CONN_INIT_RANGING);
  connectionManager_->add_connection (con, OUT_CONNECTION); 
  con = new Connection (CONN_INIT_RANGING);
  connectionManager_->add_connection (con, IN_CONNECTION); 
  con = new Connection (CONN_PADDING);
  connectionManager_->add_connection (con, OUT_CONNECTION);
  con = new Connection (CONN_PADDING);
  connectionManager_->add_connection (con, IN_CONNECTION);

  //create connection to receive broadcast packets from BS
  con = new Connection (CONN_BROADCAST);
  connectionManager_->add_connection (con, IN_CONNECTION);
}

/**
 * Initialize the MAC
 */
void Mac802_16SS::init ()
{
  //Set physical layer to receiving mode
  getPhy()->setMode (OFDM_RECV);

  //SS is looking for synchronization
  state_ = MAC802_16_WAIT_DL_SYNCH;  

  //start timer for expiration
  t21timer_->start (macmib_.t21_timeout);

  int nbPS = (int) floor((getFrameDuration()/getPhy()->getPS()));
  int nbPS_left = nbPS - phymib_.rtg - phymib_.ttg;
  int nbSymbols = (int) floor((getPhy()->getPS()*nbPS_left)/getPhy()->getSymbolTime());  // max num of OFDM symbols available per frame. 
  int nbSubcarrier = getPhy()->getFFT();
  int nbSubchannel = getPhy()->getNumsubchannels (DL_); //we receive downlink

  intpower_ = (double**) malloc (nbSymbols*sizeof (double*));
  for (int i = 0 ; i<nbSymbols ; i++) {
    intpower_[i] = (double*) malloc (nbSubcarrier*sizeof (double));
    for (int j = 0 ; j< nbSubcarrier ; j++)
      intpower_[i][j] = 0.0 ; 
  }
  basepower_ = (double**) malloc (nbSymbols*sizeof (double*));
  for (int i = 0 ; i<nbSymbols; i++) {
    basepower_[i] = (double*) malloc (nbSubchannel*sizeof (double));
    for (int j = 0 ; j<nbSubchannel ; j++)
      basepower_[i][j] = 0.0 ; 
  }
     
  //init the scheduler
  scheduler_->init();
}


/**
 * Set the mac state
 * @param state The new mac state
 */  
void Mac802_16SS::setMacState (Mac802_16State state)
{
  state_ = state;
}

/**
 * Return the mac state
 * @return The new mac state
 */  
Mac802_16State Mac802_16SS::getMacState ()
{
  return state_;
}

/**
 * Backup the state of the Mac
 * @return A structure containing a copy of current MAC state
 */
state_info* Mac802_16SS::backup_state ()
{
  state_info *backup_state = (state_info*) malloc (sizeof (state_info));
  backup_state->state = state_;
  backup_state->frameduration = getFrameDuration();
  backup_state->frame_number = frame_number_;
  backup_state->channel = getChannel();
  backup_state->connectionManager = connectionManager_;
  connectionManager_ = new ConnectionManager (this);
  init_default_connections ();
  backup_state->serviceFlowHandler = serviceFlowHandler_;
  serviceFlowHandler_ = new ServiceFlowHandler();
  backup_state->peer_list = peer_list_;
  backup_state->nb_peer = nb_peer_;
  peer_list_ = (struct peerNode *) malloc (sizeof(struct peerNode));
  LIST_INIT(peer_list_);
  return backup_state;
}

/**
 * Restore the state of the MAC
 * @param backup_state The MAC state to restore
 */
void Mac802_16SS::restore_state (state_info *backup_state)
{
  state_ = backup_state->state;
  setFrameDuration(backup_state->frameduration);
  frame_number_ = backup_state->frame_number;
  setChannel (backup_state->channel);
  delete (connectionManager_);
  connectionManager_ = backup_state->connectionManager;
  delete (serviceFlowHandler_);
  serviceFlowHandler_ = backup_state->serviceFlowHandler;
  while (getPeerNode_head()!=NULL) {
    removePeerNode (getPeerNode_head());
  }
  peer_list_ = backup_state->peer_list;
  nb_peer_ = backup_state->nb_peer;
}

/**
 * Called when a timer expires
 * @param The timer ID
 */
void Mac802_16SS::expire (timer_id id)
{
  switch (id) {
  case WimaxT21TimerID:
    //*debug ("At %f in Mac %d, synchronization failed\n", NOW, addr());
    //go to next channel
    nextChannel();
    t21timer_->start (macmib_.t21_timeout);
    break;
  case WimaxLostDLMAPTimerID:
    //*debug ("At %f in Mac %d, lost synchronization (DL_MAP)\n", NOW, addr());
    lost_synch ();
    break;
  case WimaxT1TimerID:
    //*debug ("At %f in Mac %d, lost synchronization (DCD)\n", NOW, addr());
    lost_synch ();
    break;
  case WimaxLostULMAPTimerID:
    //*debug ("At %f in Mac %d, lost synchronization (UL_MAP)\n", NOW, addr());
    lost_synch ();
    break;
  case WimaxT12TimerID:
    //*debug ("At %f in Mac %d, lost uplink param (UCD)\n", NOW, addr());
    lost_synch ();
    break;
  case WimaxT2TimerID:
    //*debug ("At %f in Mac %d, lost synchronization (RNG) so remove cdma initial ranging request\n", NOW, addr());
    //getMap()->getUlSubframe()->getRanging()->removeRequest ();
    getMap()->getUlSubframe()->getRanging()->removeRequest_mac (addr());
    Connection *c_tmp;
    c_tmp  = getCManager ()->get_connection (0, true);
    c_tmp->setINIT_REQ_QUEUE(addr(), 0);

    lost_synch ();
    break;
  case WimaxT3TimerID:
    //*debug ("At %f in Mac %d, no response from BS so remove cdma initial ranging request\n", NOW, addr());
    //we reach the maximum number of retries
    //mark DL channel usuable (i.e we go to next)
    //getMap()->getUlSubframe()->getRanging()->removeRequest ();
    getMap()->getUlSubframe()->getRanging()->removeRequest_mac (addr());
    nextChannel();
    lost_synch ();
    break;
  case WimaxT6TimerID:
    //*debug ("At %f in Mac %d, registration timeout (nbretry=%d)\n", NOW, addr(),
	   //*nb_reg_retry_);
    if (nb_reg_retry_ == macmib_.reg_req_retry) {
      //*debug ("\tmax retry excedeed\n");
      lost_synch ();
      //exit(1); // Xingting added here. Otherwise the segmentation fault will happen later.
    } else {
      send_registration();
    }
    break;
  case WimaxT44TimerID:
    //*debug ("At %f in Mac %d, did not receive MOB_SCN-RSP (nb_retry=%d/%d)\n", NOW, addr(), nb_scan_req_, macmib_.scan_req_retry);
    if (nb_scan_req_ <= macmib_.scan_req_retry) {
      send_scan_request ();
    } else { //reset for next time
      nb_scan_req_ = 0;
    }
    break;
  case WimaxScanIntervalTimerID:
    pause_scanning ();
    break;    
  case WimaxRdvTimerID:
    //we need to meet at another station. We cancel the current scanning
    //lost_synch ();
    //*debug ("At %f in Mac %d Rdv timer expired\n", NOW, addr());
    break;
  default:
    ;//*debug ("Trigger unkown\n");
  }
}


/**** Packet processing methods ****/

/*
 * Process packets going out
 * @param p The packet to send out
 */
void Mac802_16SS::sendDown(Packet *p)
{
  //We first send it through the CS
  int cid = -1;

  if (!notify_upper_) {
    assert (!pktBuf_);
    pktBuf_ = p;
    return;
  } 

  cid = classify (p);

  if (cid == -1) {
    //*debug ("At %f in Mac %d drop packet because no classification were found\n", \
	   NOW, index_);
    drop(p, "CID");
    //Packet::free (p);
  } else {
    //enqueue the packet 
    Connection *connection = connectionManager_->get_connection (cid, OUT_CONNECTION);
    if (connection == NULL) {
      //*debug2 ("Warning: At %f in Mac %d connection with cid = %d does not exist. Please check classifiers\n",\
	     NOW, index_, cid);
      //Packet::free (p);
      update_watch (&loss_watch_, 1);
      drop(p, "CID");
    }
    else 
    {
    		bool need_drop = false;
 			if(!connection->getArqStatus())
			{
				if(connection->queueLength() >= macmib_.queue_length)
				{
					update_watch (&loss_watch_, 1);
					need_drop = true;
					drop (p, "QWI");
				}
			}
			else if(connection->getArqStatus() && (connection->getArqStatus()->isArqEnabled() == 1))
			{
				int data_packet_size = HDR_CMN(p)->size();
				int num_arq_blocks = (int) ceil ((double)data_packet_size/arq_block_size_);

				//debug2("data_packet_size=%d\t num_arq_blocks=%d\t qlength=%d max_qlength=%d\n",
							//data_packet_size,num_arq_blocks,connection->queueLength(),macmib_.queue_length);
				if((connection->queueLength() + num_arq_blocks) >= macmib_.queue_length)
				{
					update_watch (&loss_watch_, 1);
					need_drop = true;
					drop (p, "QWI");
				}
			}
			
			if( !need_drop)
			{
				//Begin RPI
				if (connection->getArqStatus () == NULL) 
				{
					//End RPI
					//update mac header information
					//set header information
					hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
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
					wimaxHdr->header.cid = cid; //default
					wimaxHdr->header.hcs = 0;
					HDR_CMN(p)->size() += HDR_MAC802_16_SIZE;
					connection ->enqueue (p);
					//Begin RPI
					debug2 ("At %f (SS) in Mac %d, SENDDOWN, Enqueue packet to cid :%d queue size :%d (max :%d)\n", NOW, index_, cid,connection->queueLength (), macmib_.queue_length);
				}
				else if((connection->getArqStatus () != NULL) && (connection->getArqStatus ()->isArqEnabled () == 1)) 
				{
					//We will have to divide the packet to ARQ Blocks
					//In NS2 it will be implemented by duplicating the packet but changing the size of the packet

					int packet_size = HDR_CMN(p)->size();
					int number_of_packs= (packet_size/arq_block_size_); 
					hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
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
					wimaxHdr->header.cid = cid; //default
					wimaxHdr->header.hcs = 0;
					HDR_CMN(p)->size() = arq_block_size_;

					for(int i=1; i<= number_of_packs; i++)
					{
						if(i == 1)
							connection->getArqStatus ()->arqSend(p, connection, FRAG_FIRST);
						else
							connection->getArqStatus ()->arqSend(p, connection, FRAG_CONT);	
						connection ->enqueue (p->copy());  //?????
					}
					HDR_CMN(p)->size() = (packet_size - (number_of_packs * arq_block_size_));

					if(number_of_packs)
					{
						connection->getArqStatus ()->arqSend(p, connection, FRAG_LAST);
					}
					else
					{
						connection->getArqStatus ()->arqSend(p, connection, FRAG_NOFRAG);
					}
					connection ->enqueue (p);  //?????
					//debug2 ("ARQ SS Dividing Packet %d into Blocks Packet Size=%d, Number of packets = %d, ARQ_BLOCK_SIZE = %d,Last Packet Size= %d\n",HDR_CMN(p)->uid(),packet_size,number_of_packs+1,arq_block_size_, HDR_CMN(p)->size()) ;
				}
				//End RPI
			}
		}
	}

	//inform upper layer that it can send another packet
	resume (NULL);  
}

/*
 * Transmit a packet to the physical layer
 * @param p The packet to send out
 */
void Mac802_16SS::transmit(Packet *p)
{
  //The following condition is valid for OFDM but in OFDMA
  //we can send multiple packets using different subcarriers
  //so we will have to update this.
  /* if (NOW < last_tx_time_+last_tx_duration_) {
  //still sending
  //debug2 ("MAC is already transmitting. Drop packet.\n");
  Packet::free (p);
  return;
  }
  */ // rpi commented for OFDMA to allow multiple packets to be transmitted at the same time. 
  struct hdr_cmn *ch = HDR_CMN(p);
  int flag_cdma_packet = 0;

  //*debug10 ("At %f (SS) in Mac %d, TRANSMIT, sending packet (type=%s, size=%d, txtime=%f)\n", NOW, index_, packet_info.name(ch->ptype()), ch->size(), ch->txtime());
  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(p)->header.ht == 0)
      {
        //if((mac802_16_dl_map_frame*) p->accessdata() != NULL)
          //debug10 ("\t generic mngt :%d\n", ((mac802_16_dl_map_frame*) p->accessdata())->type);
      }
    else {
        hdr_mac802_16 *wimaxHdr_tmp = HDR_MAC802_16(p);
        gen_mac_header_t header_tmp = wimaxHdr_tmp->header;

        cdma_req_header_t *req_tmp;
        req_tmp = (cdma_req_header_t *)&header_tmp;

        if (req_tmp->type == 0x3) {
          //*debug10 ("\t cdma_bw_req code :%d, top :%d\n", req_tmp->code, req_tmp->top);
  	  flag_cdma_packet = 1;
        } else if (req_tmp->type == 0x2) {
          //*debug10 ("\t cdma_init_req code :%d, top :%d\n", req_tmp->code, req_tmp->top);
  	  flag_cdma_packet = 1;
        } else {
          //*debug10 ("\t bwreq\n");
        }

    }
  } else {
    //*debug10 ("\t unknown => %s\n",  packet_info.name(ch->ptype()));
  }
  
  //update stats for delay and jitter
  double delay = NOW-ch->timestamp();
  if (flag_cdma_packet == 0) update_watch (&delay_watch_, delay);
  double jitter = fabs (delay - last_tx_delay_);
  if (flag_cdma_packet == 0) update_watch (&jitter_watch_, jitter);
  last_tx_delay_ = delay;
  if (ch->ptype()!=PT_MAC) {
    update_throughput (&tx_data_watch_, 8*ch->size());
  } 
  if (flag_cdma_packet == 0) update_throughput (&tx_traffic_watch_, 8*ch->size());
  
  last_tx_time_ = NOW;
  last_tx_duration_ = ch->txtime();
  //pass it down
  downtarget_->recv (p, (Handler*)NULL);
}

/*
 * Process incoming packets
 * @param p The incoming packet
 */
void Mac802_16SS::sendUp (Packet *p)
{
  struct hdr_cmn *ch = HDR_CMN(p);
  hdr_mac802_16 *wimaxHdr;
  wimaxHdr = HDR_MAC802_16(p);
  int flag_cdma_packet = 0;

/*
//Chakchai; if the direction is not for SS; return => probably fixed by Richard
  hdr_cmn* ch_tmp = HDR_CMN(p);
  hdr_mac802_16 *wimaxHdr_tmp;
  wimaxHdr_tmp = HDR_MAC802_16(p);
//  debug10 (" SS direction :%d\n", wimaxHdr_tmp->phy_info.direction);
  if (wimaxHdr_tmp->phy_info.direction != 0) {
     Packet::free(p);
     return;
  }
*/
	  
#ifdef DEBUG_WIMAX
//  debug ("At %f in Mac %d receive first bit..over at %f(txtime=%f) (type=%s)\n", NOW, index_, NOW+ch->txtime(),ch->txtime(), packet_info.name(ch->ptype()));
  //*debug10 ("At %f (SS) in Mac %d, SENDUP, receive first bit..over at %f(txtime=%f) (type=%s)\n", NOW, index_, NOW+ch->txtime(),ch->txtime(), packet_info.name(ch->ptype()));

  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(p)->header.ht == 0)
      {
        //if((mac802_16_dl_map_frame*) p->accessdata() != NULL)
          //debug10 ("\t generic mngt :%d\n", ((mac802_16_dl_map_frame*) p->accessdata())->type);
      }
    else {
        hdr_mac802_16 *wimaxHdr_tmp = HDR_MAC802_16(p);
        gen_mac_header_t header_tmp = wimaxHdr_tmp->header;

        cdma_req_header_t *req_tmp;
        req_tmp = (cdma_req_header_t *)&header_tmp;

        if (req_tmp->type == 0x3) {
          //*debug10 ("\t Impossible (PANIC) cdma_bw_req code :%d, top :%d\n", req_tmp->code, req_tmp->top);
  	  flag_cdma_packet = 1;
        } else if (req_tmp->type == 0x2) {
          //*debug10 ("\t Impossible (PANIC) cdma_init_req code :%d, top :%d\n", req_tmp->code, req_tmp->top);
  	  flag_cdma_packet = 1;
        } else {
          //*debug10 ("\t Impossible (PANIC) bwreq\n");
        }

    }
  } else {
    //*debug10 ("\t unknown => %s\n",  packet_info.name(ch->ptype()));
  }

#endif
  //sam

  //wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;
  int cid = header.cid;
  Connection *con = connectionManager_->get_connection (cid, IN_CONNECTION);

  if( (wimaxHdr->phy_info.OFDMSymbol_offset == 0 && wimaxHdr->phy_info.num_OFDMSymbol == 0) 
  		|| wimaxHdr->header.cid == BROADCAST_CID)   // this kind of packets are treated diff(OFDM types) basically it a bw req packet.
    {
      if (con == NULL) {
	//This packet is not for us
	//    //    //debug2 ("At %f in Mac %d Connection null\n", NOW, index_);
	update_watch (&loss_watch_, 1);
 
	Packet::free(p);
	//*debug2 (" packet getting dropped header.ht = %d",HDR_MAC802_16(p)->header.ht);
	//pktRx_=NULL;
	return;
      } 
      
      if(contPktRxing_ == TRUE)
	{
	  // if there is a collision and the power of the currently recvd packet is less by factor 10 thn discard it. 
           
	  if (head_pkt_ !=NULL) {
            
	    if(head_pkt_->p->txinfo_.RxPr / p->txinfo_.RxPr >= p->txinfo_.CPThresh)
	      Packet::free(p);
            else if(head_pkt_->p->txinfo_.RxPr / p->txinfo_.RxPr <= p->txinfo_.CPThresh)
	      {
		head_pkt_->timer.stop();
		removePacket(head_pkt_);
		addPacket(p);

	      }
            else 
	      {
		if(txtime(p) > head_pkt_->timer.expire())
		  {
		    head_pkt_->timer.stop();
		    removePacket(head_pkt_);
		    ch->error() = 1;
		    update_watch (&loss_watch_, 1);
		    collision_ = true;
		    addPacket(p);	

		  }

	      }
            
       
	  }
	}

      else 

        {
          //contPktRxing_ = TRUE;
          addPacket(p);
	}  
    }
  
  else {

    if (con == NULL) {
      //This packet is not for us
      //*debug2 ("At %f in Mac %d Connection null -- packet getting dropped at SS not for us \n", NOW, index_);
      update_watch (&loss_watch_, 1);
    
      // remeber to take care of collision, tht is if a packet destined to this is already being recvd and another packets destined to this comes thn thr will be collision    

      Packet::free(p);  //removed jut to chk
      return;


    }  // free and end if con == NULL 
    

    // collision check .  rpi

    bool collision = FALSE; 

    //collision = IsCollision ( wimaxHdr, 0.0);   // removed replace it later

    if (collision == TRUE)

      {
	collision = FALSE; 
	hdr_cmn *hdr = HDR_CMN(p);
	hdr->error() = 1;   // we do not discard this packet but just mark it and r=transmit coz, this packet and the other collided packet might not have symbols and carrriers exactly same, but they can collide, we shld also keep track of this packets transmission, because other packets overlapping with this can collide. 
 
	// set to the symbol and corresponding subcarriers over which this packet was transmitted to -1, so tht for future packets we can check for collision and also after the other collided packet is received we can chk for collision.    
	//collision_ = true; 
	addPowerinfo(wimaxHdr, 1.0,true);  // enable , diabled just to compile // will be in the base power 

	// set end

      }
    else
      addPowerinfo(wimaxHdr, BASE_POWER,true ); // enable , disabled just to compile 
  
    addPowerinfo(wimaxHdr, 0.0,false);   // enable, disabled just to compile  // separated the variabled for interference and collision detection. 

    // collision check rpi 


    //remove temporary to chk without collision and interference 

    addPacket(p);    // adding the collided packet also , which will be dropped in the receive function, this will help for collision detection of packets which might over lap with this.

  } // ends else for bwreq  replace this } later when testing over 

}

/**
 * Process the fully received packet
 */
void Mac802_16SS::receive (Packet *pktRx_)
{
  assert (pktRx_);
  struct hdr_cmn *ch = HDR_CMN(pktRx_);
  hdr_mac802_16 *wimaxHdr;
  int flag_cdma_packet = 0;
  wimaxHdr = HDR_MAC802_16(pktRx_);

#ifdef DEBUG_WIMAX
  //*debug10 ("At %f (SS) in Mac %d, RECEIVE, packet received (type=%s)\n", NOW, index_, packet_info.name(ch->ptype()));
  //debug10 (" phyinfo header - symbol offset = %d, numsymbol = %d\n", wimaxHdr->phy_info.OFDMSymbol_offset,wimaxHdr->phy_info.num_OFDMSymbol);

  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(pktRx_)->header.ht == 0)
      {
        //if((mac802_16_dl_map_frame*) pktRx_->accessdata() != NULL)
          //debug10 (" generic mngt :%d\n", ((mac802_16_dl_map_frame*) pktRx_->accessdata())->type);
      }
    else {
      hdr_mac802_16 *wimaxHdr_tmp = HDR_MAC802_16(pktRx_);
      gen_mac_header_t header_tmp = wimaxHdr_tmp->header;

      cdma_req_header_t *req_tmp;
      req_tmp = (cdma_req_header_t *)&header_tmp;

      if (req_tmp->type == 0x3) {
          //*debug10 ("\t Impossible (PANIC) cdma_bw_req code :%d, top :%d\n", req_tmp->code, req_tmp->top);
  	  flag_cdma_packet = 1;
      } else if (req_tmp->type == 0x2) {
          //*debug10 ("\t Impossible (PANIC) cdma_init_req code :%d, top :%d\n", req_tmp->code, req_tmp->top);
  	  flag_cdma_packet = 1;
      } else {
          //*debug10 ("\t Impossible (PANIC) bwreq\n");
      }
    }

  } else {
    //*debug10 ("\t unknown => %s\n",  packet_info.name(ch->ptype()));
  }
#endif
    
  // commenting here to chk without collision start 

  if( (wimaxHdr->phy_info.OFDMSymbol_offset == 0 && wimaxHdr->phy_info.num_OFDMSymbol == 0) || wimaxHdr->header.cid == BROADCAST_CID)   // this kind of packets are treated diff(OFDM types) basically it a bw req packet.
    {
      if (ch->error())
	{
          if (collision_) {
	    //*debug2 ("\t drop new pktRx..collision\n");
	    drop (pktRx_, "COL");
	    collision_ = false;
	  } 
          else {
            //*debug2("\t error in the packet, the Mac does not process");
      	    Packet::free(pktRx_);
	  }

	  //update drop stat
	  update_watch (&loss_watch_, 1);
	  contPktRxing_ = FALSE;
	  pktRx_ = NULL;
    	  return;
	}          
      //else
      contPktRxing_ = FALSE ; 
    }
  else {   //bwreq packet treatment else 
  
    //collision_ = false; // bwreq packet recvd , so no packet being transmitted. 
    //drop the packet if corrupted
    if (ch->error()) {
      //  if (collision_) {
      //   debug2 ("\t drop new pktRx..collision\n");
      addPowerinfo(wimaxHdr, 0.0,true);   
      //   drop (pktRx_, "COL");
      //   collision_ = false;
      // } else {
      //*debug2("\t error in the packet, the Mac does not process");
      //  addPowerinfo(wimaxHdr, 0.0,false);  // when we drop or discard a packet we have to reinitialise the intpower array to 0.0 , which shows tht there is no packet being recvd over the symbols and subcarriers.   // interference initialisation not reqd cause it is initialised whenever a packet is recvd
      Packet::free(pktRx_);
    
      //update drop stat
      update_watch (&loss_watch_, 1);
      pktRx_ = NULL;
      return;
    }

    // chk here for collision of the packet which was received before the packet that caused actual collision, so we have to chk whether collision had occur on this packet or not. , and the collsion was caused  

    bool collision = FALSE; 

    //collision = IsCollision(wimaxHdr, BASE_POWER);  //removed replace it later

    if (collision == TRUE)
      {
	collision = FALSE; 
	addPowerinfo(wimaxHdr, 0.0,true);  
	drop (pktRx_, "COL"); 
	//update drop stat
	update_watch (&loss_watch_, 1);
	pktRx_ = NULL;
	//*debug2("\t Drop this packet because of collision.\n");
	return;
      }

    addPowerinfo(wimaxHdr, 0.0,true); 

    //commenting here to chk without collision  ends

    // chk collision ends

    //   SINR calcualtions

    //removed for testing without Rxinpwr in packet header

    int total_subcarriers=0; 
    //int num_data_subcarrier = getPhy()->getNumDataSubcarrier (DL_);         
    int num_data_subcarrier = getPhy()->getNumSubcarrier (DL_);         
    int num_subchannel      = getPhy()->getNumsubchannels (DL_); 
    int num_symbol_per_slot = getPhy()->getSlotLength (DL_); 

    // calculate the total subcarriers needed
    if(wimaxHdr->phy_info.num_OFDMSymbol != 0)
      { 
             
	//total_subcarriers = wimaxHdr->phy_info.num_subchannels * wimaxHdr->phy_info.num_OFDMSymbol * num_data_subcarrier ;
     
	if(wimaxHdr->phy_info.num_OFDMSymbol % num_symbol_per_slot == 0)   // chk this condition , chk whether broacastcid reqd ir not. 
	  {
	    if(wimaxHdr->phy_info.num_OFDMSymbol > num_symbol_per_slot) 
	      {
		// for the first 3 symbols 
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
		  //   for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers += (num_subchannel*num_data_subcarrier) - ((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ) ;
		  }
		// except the last 3 and first 3 whatever is thr
          
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+num_symbol_per_slot ; i< (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot) ; i++)
		  //for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers +=  num_subchannel*num_data_subcarrier;
		  } 

		// last 3 
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i<  wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol ; i++)
		  //for(int j = 0 ; j<(wimaxHdr->phy_info.num_subchannels%num_subchannel)*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers += ((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier;	
		  }
	      }
	    else 
	      {

		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
		  //for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<wimaxHdr->phy_info.num_subchannels*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers += wimaxHdr->phy_info.num_subchannels*num_data_subcarrier ;//-  (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ;	
		  }	   
	      }
	  }
	else
	  {
	    // for the first 3 symbols 
	    if(wimaxHdr->phy_info.num_OFDMSymbol > 1) 
	      {
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
		  //   for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers += num_subchannel*num_data_subcarrier - (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ;
		  }
		// except the last 3 and first 3 whatever is thr
          
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+1 ; i< (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
		  //for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers +=  num_subchannel*num_data_subcarrier ;
		  }  

		// last 3 
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-1 ; i< wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol ; i++)
		  // for(int j = 0 ; j<(wimaxHdr->phy_info.num_subchannels%num_subchannel)*num_data_subcarrier ; j++ )
		  {
                    total_subcarriers += ((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier ; 
		  }

	      }
	    else
	      {
          
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
		  // for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<wimaxHdr->phy_info.num_subchannels*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers +=    wimaxHdr->phy_info.num_subchannels*num_data_subcarrier;// - (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; 
		  }	
	      }
	  }
      } 
    //     else
    //      total_subcarriers = wimaxHdr->phy_info.num_subchannels * num_data_subcarrier ;  

    // total subcarrier calculation ends 

    //debug2(" total_subcarriers = %d \n", total_subcarriers); 

    double *signalpower = (double *) new double [total_subcarriers] ;  
    double *interferencepower = (double *) new double [total_subcarriers]; 
    double *SINR = (double *) new double [total_subcarriers]; 
    for(int i = 0; i< total_subcarriers ; i++)
      {
	SINR[i] = 0.0;
	signalpower[i] = 0.0;
	interferencepower[i] = 0.0;
      }

    int n=0,m=0; 

    if(wimaxHdr->phy_info.num_OFDMSymbol % num_symbol_per_slot == 0)   // chk this condition 
      {
	if(wimaxHdr->phy_info.num_OFDMSymbol > num_symbol_per_slot) 
	  {
	    // for the first num_symbol_per_slot symbols 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
		{
		  interferencepower[n++]= intpower_[i][j] + BASE_POWER /*+ pktRx_->txinfo_.RxIntPr[i][j]*/; 
		  signalpower[m++]= pktRx_->txinfo_.RxPr_OFDMA[j];
		}
	    // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+num_symbol_per_slot ; i< (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot) ; i++)
	      for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
		{
		  interferencepower[n++]= intpower_[i][j] + BASE_POWER /*+ pktRx_->txinfo_.RxIntPr[i][j]*/; 
		  signalpower[m++]= pktRx_->txinfo_.RxPr_OFDMA[j];
		} 

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i<  wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol ; i++)
	      for(int j = 0 ; j<((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier; j++ )
		{
		  interferencepower[n++]= intpower_[i][j] + BASE_POWER /*+ pktRx_->txinfo_.RxIntPr[i][j]*/; 
		  signalpower[m++]= pktRx_->txinfo_.RxPr_OFDMA[j];
		}
          }
	else 
	  {

	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; 	i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; 
		  j<(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier) + wimaxHdr->phy_info.num_subchannels*num_data_subcarrier); 
		  j++ )
		{
		  interferencepower[n++]= intpower_[i][j] + BASE_POWER /*+ pktRx_->txinfo_.RxIntPr[i][j]*/; 
		  signalpower[m++]= pktRx_->txinfo_.RxPr_OFDMA[j];
		}	    
	  }
      }

    else

      {
	//numsymbols = (int)  ceil((((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) - 30) % 30));
	//numsymbols *= num_symbol_per_slot;  
        // for the first num_symbol_per_slot symbols 
	if(wimaxHdr->phy_info.num_OFDMSymbol > 1) 
	  {
            for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
		{
		  interferencepower[n++]= intpower_[i][j] + BASE_POWER /*+ pktRx_->txinfo_.RxIntPr[i][j]*/; 
		  signalpower[m++]= pktRx_->txinfo_.RxPr_OFDMA[j];
		}
	    // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
            for(int i = (wimaxHdr->phy_info.OFDMSymbol_offset+1) ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
	      for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
		{
		  interferencepower[n++]= intpower_[i][j] + BASE_POWER /*+ pktRx_->txinfo_.RxIntPr[i][j]*/; 
		  signalpower[m++]= pktRx_->txinfo_.RxPr_OFDMA[j];
		}

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-1 ; i< wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol ; i++)
	      for(int j = 0 ; j<(((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel))*num_data_subcarrier ; j++ )
		{
		  interferencepower[n++]= intpower_[i][j] + BASE_POWER /*+ pktRx_->txinfo_.RxIntPr[i][j]*/; 
		  signalpower[m++]= pktRx_->txinfo_.RxPr_OFDMA[j];
		}

          }
	else
	  {
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier) + wimaxHdr->phy_info.num_subchannels*num_data_subcarrier) ; j++ )
		{
		  interferencepower[n++]= intpower_[i][j] + BASE_POWER /*+ pktRx_->txinfo_.RxIntPr[i][j]*/; 
		  signalpower[m++]= pktRx_->txinfo_.RxPr_OFDMA[j];
		}	
	  }
      }

	int counter = 0;
	for(int k = 0; k < total_subcarriers; k++)
	{
		if( signalpower[k] > DOUBLE_INT_ZERO) // not equals to zero
		{
			SINR[k] =  signalpower[k]/interferencepower[k];
			counter++;
		}
	}

	//debug2(" in MAC SS, [%d] subcarriers signal power are calculated.\n ", counter);
   

    double SIR = 0.0;
    //debug2("In MAC BS: the total number of subcarrier/3 is [%d]\n", total_subcarriers/3);
 

    //process packet
    gen_mac_header_t header = wimaxHdr->header;
    OFDMAPhy *phy = getPhy();

    //debug2(" checking for intereference power \n" );
 
    // BLER calculation

    GlobalParams_Wimax* global ; 

    global =  GlobalParams_Wimax::Instance();

    int num_of_slots,num_of_complete_block,max_block_size,last_block_size , index, num_subcarrier_block ;

    double BLER= 0.0, beta=0.0, eesm_sum = 0.0, rand1 =0.0; 

    num_of_slots = (int) ceil((double)ch->size() / (double)phy->getSlotCapacity(wimaxHdr->phy_info.modulation_, DL_));

    max_block_size=phy->getMaxBlockSize(wimaxHdr->phy_info.modulation_);   // get max  block size in number of slots

    last_block_size = num_of_slots % max_block_size; 

    num_of_complete_block = (int) floor(num_of_slots / max_block_size);

    int ITU_PDP = getITU_PDP (); 

    bool pkt_error = FALSE; 

    //*debug2(" num of complete blocks = %d , ITU_PDP = %d, num of slots = %d, max block size = %d, last block size = %d \n" , num_of_complete_block, ITU_PDP, num_of_slots, max_block_size, last_block_size ); 

    if (num_of_complete_block == 0) { 
      //num_of_complete_block =1;
      index =  phy->getMCSIndex( wimaxHdr->phy_info.modulation_ ,  last_block_size);
      
      beta = global->Beta[ITU_PDP][index]; 
      //debug2(" beta = %.2f = \n" , beta ); 

      num_subcarrier_block = num_symbol_per_slot * getPhy()->getNumSubcarrier (DL_) * last_block_size; 

      if(num_subcarrier_block > total_subcarriers) num_subcarrier_block = total_subcarriers; 

/*      for( int i = 0; i< num_subcarrier_block ; i++)     
	{    
          eesm_sum = eesm_sum + exp( -(SINR[i]/beta));
	  //printf( " SINR (%d) = %g , eesm_sum = %g %g\n", i, SINR[i], eesm_sum, exp(-(SINR[i]/beta)));    
	}*/

	for(int i=0; i<counter; i++)
	{
//Print a lot of lines
		//*printf( " SINR (%d) = %g , eesm_sum = %g %g\n", i, SINR[i], eesm_sum, exp(-(SINR[i]/beta)));    
		eesm_sum = eesm_sum + exp( -(SINR[i]/beta));		
	}


	
      if (num_subcarrier_block==0) {
	fprintf(stderr, "ERROR: #subcarrier_block :%d\n", num_subcarrier_block); 
	exit(1);
      }	
      if (eesm_sum >= BASE_POWER) {
	//SIR =  (-beta) * log( eesm_sum/(num_subcarrier_block) );
	SIR =  (-beta) * log(eesm_sum/counter);
	//debug2(" SIR-SS = %.2f = \n" , SIR ); 
	SIR=10*log10(SIR);
	//debug2(" SIR-SS in dB= %.2f = \n" , SIR ); 
	BLER = global->TableLookup(index, SIR);
      } else {
	BLER = 0;
      }
      //debug2(" BLER at SS 0 complete blocks = %.2f  \n" , BLER ); 

#if 0
	  int rand_num = ((rand() % 100) +1 ) ; 

	  //debug2(" random num = %d = \n" , rand_num ); 
 
	  rand1 = rand_num/100.00; 

	  if (!phymib_.disableInterference && BLER > rand1) 
	    pkt_error = TRUE; 
#endif
	 if (!phymib_.disableInterference && BLER > 0.96) 
	pkt_error = TRUE;
      
      // getBLER(wimaxHdr->phy_info.modulation_, SINR, last_block_size);
    }
    else {

      // First the complete blocks are checked fro error 

      index =  phy->getMCSIndex( wimaxHdr->phy_info.modulation_ ,  max_block_size);
      
      beta = global->Beta[ITU_PDP][index];  

 
      num_subcarrier_block = num_symbol_per_slot * getPhy()->getNumSubcarrier (DL_) * max_block_size; 

      if(num_subcarrier_block > total_subcarriers) num_subcarrier_block = total_subcarriers; 

/*      for( int i = 0; i< num_subcarrier_block ; i++)     
	eesm_sum = eesm_sum + exp( -(SINR[i]/beta));*/

	for(int i=0; i<counter; i++)
	{
		//printf( " SINR (%d) = %g , eesm_sum = %g %g\n", i, SINR[i], eesm_sum, exp(-(SINR[i]/beta)));    
		eesm_sum = eesm_sum + exp( -(SINR[i]/beta));		
	}


      if (num_subcarrier_block==0) {
	fprintf(stderr, "ERROR: #subcarrier_block :%d\n", num_subcarrier_block); 
	exit(1);
      }	
      if (eesm_sum >= BASE_POWER) {
	//SIR =  (-beta) * log( eesm_sum/(num_subcarrier_block) );
	SIR =  (-beta) * log(eesm_sum/counter);
	SIR=10*log10(SIR);
	//debug2(" SIR-SS = %.2f = \n" , SIR );
	BLER = global->TableLookup(index, SIR);
      } else {
	BLER = 0;
      }
 
      //debug2(" BLER at SS of complete blocks = %.2f  \n" , BLER ); 
 
#if 0
	  int rand_num = ((rand() % 100) +1 ) ; 

	  //debug2(" random num = %d = \n" , rand_num ); 
 
	  rand1 = rand_num/100.00; 

	  if (!phymib_.disableInterference && BLER > rand1) 
	    pkt_error = TRUE; 
#endif
	 if (!phymib_.disableInterference && BLER > 0.96) 
	pkt_error = TRUE;

      // Chk if thr is any last block, compute whether it is in error or not. 
      if(last_block_size > 0)

	{
	  eesm_sum =0;

	  index =  phy->getMCSIndex( wimaxHdr->phy_info.modulation_ ,  last_block_size);
      
	  beta = global->Beta[ITU_PDP][index];  
 
	  num_subcarrier_block = num_symbol_per_slot * getPhy()->getNumSubcarrier (DL_) * last_block_size; 

	  if(num_subcarrier_block > total_subcarriers) num_subcarrier_block = total_subcarriers; 

/*	  for( int i = 0; i< num_subcarrier_block ; i++)     
	    eesm_sum = eesm_sum + exp( -(SINR[i]/beta));*/

	for(int i=0; i<counter; i++)
	{
//Print a lot of lines
//		printf( " SINR (%d) = %g , eesm_sum = %g %g\n", i, SINR[i], eesm_sum, exp(-(SINR[i]/beta)));    
		eesm_sum = eesm_sum + exp( -(SINR[i]/beta));		
	}


	  if (num_subcarrier_block==0) {
	    fprintf(stderr, "ERROR: #subcarrier_block :%d\n", num_subcarrier_block); 
	    exit(1);
	  }	

	  if (eesm_sum >= BASE_POWER) {
	    //SIR =  (-beta) * log( eesm_sum/(num_subcarrier_block) );
	    SIR =  (-beta) * log(eesm_sum/counter);
	    if (SIR < 0) {
	      exit (1);
	    }
	    SIR=10*log10(SIR);
	    //debug2(" SIR-SS = %.2f = \n" , SIR );	    
	    BLER = global->TableLookup(index, SIR);
	  } else {
	    BLER = 0;
	  }
 	  //debug2(" BLER at SS last block = %.2f = \n" , BLER );	


#if 0	
	  int rand_num = ((rand() % 100) +1 ) ; 

	  //debug2(" random num = %d = \n" , rand_num ); 

	  rand1 = rand_num/100.00; 

	  if (!phymib_.disableInterference && BLER > rand1) 
	    pkt_error = TRUE;
#endif
	 if (!phymib_.disableInterference && BLER > 0.96) 
	    pkt_error = TRUE; 

	}

 
    }

    //debug2( " BLER = %.25f" , BLER); 

    // deleting dynamic allocations  //removed for testing without Rxinpwr in packet header  

    delete [] SINR;
    delete [] signalpower;
    delete [] interferencepower; 


    if( pkt_error == TRUE)

      {
	addPowerinfo(wimaxHdr, 0.0,true);   
   
	//*debug2("error in the packet, drop this packet. the Mac does not process\n");
	Packet::free(pktRx_);
	pkt_error = FALSE;
	//update drop stat
	update_watch (&loss_watch_, 1);
	pktRx_ = NULL;
	return;    

      }

    addPowerinfo(wimaxHdr, 0.0,true);
 
    //removed for testing without Rxinpwr in packet header  

    //BLER calcualtion ends


  } //bwreq packet treatment else ends here.  replace { when testing ends 

 
  // normal packet processing starts. 

  gen_mac_header_t header = wimaxHdr->header;
  int cid = header.cid;
  Connection *con = connectionManager_->get_connection (cid, IN_CONNECTION);

  //update rx time of last packet received
  PeerNode *peer_ = getPeerNode_head();
  if (peer_) {
    peer_->setRxTime (NOW);
    if (pktRx_->txinfo_.RxPr==0) {
	//*printf("ERROR: RXpr = 0, please double check; pktRx_->txinfo_.RxPr :%g\n", pktRx_->txinfo_.RxPr); 
	exit(1);
    }	
    //moved monitoring of received power to process_mac_packet
  }
  //Begin RPI 
  // New function for reassembly will be implemented for ARQ, Fragmentation and Packing
  if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1 && con->isFragEnable () == true && con->isPackingEnable () == true && HDR_CMN(pktRx_)->ptype()!=PT_MAC){
    process_mac_pdu_witharqfragpack(con, pktRx_);
    return;		
  }	  
  //End RPI  
  //process reassembly
  if (wimaxHdr->header.type_frag) {
    bool drop_pkt = true;
    bool frag_error = false;
    //debug2 (" Frag type = %d %d\n",wimaxHdr->frag_subheader.fc,wimaxHdr->frag_subheader.fc & 0x3);
    switch (wimaxHdr->frag_subheader.fc & 0x3) {
    case FRAG_NOFRAG: 
      if (con->getFragmentationStatus()!=FRAG_NOFRAG)
	con->updateFragmentation (FRAG_NOFRAG, 0, 0); //reset
      drop_pkt = false;
      break; 
    case FRAG_FIRST: 
      //when it is the first fragment, it does not matter if we previously
      //received other fragments, since we reset the information
      assert (wimaxHdr->frag_subheader.fsn == 0);
      //printf ("\tReceived first fragment\n");
      con->updateFragmentation (FRAG_FIRST, 0, ch->size()-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
      break; 
    case FRAG_CONT: 
      if ( (con->getFragmentationStatus()!=FRAG_FIRST
	    && con->getFragmentationStatus()!=FRAG_CONT)
	   || ((wimaxHdr->frag_subheader.fsn&0x7) != (con->getFragmentNumber ()+1)%8) ) {
	frag_error = true;
	con->updateFragmentation (FRAG_NOFRAG, 0, 0); //reset
      } else {
	//printf ("\tReceived cont fragment\n");
	con->updateFragmentation (FRAG_CONT, wimaxHdr->frag_subheader.fsn&0x7, con->getFragmentBytes()+ch->size()-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));	
      }
      break; 
    case FRAG_LAST: 
      if ( (con->getFragmentationStatus()==FRAG_FIRST
	    || con->getFragmentationStatus()==FRAG_CONT)
	   && ((wimaxHdr->frag_subheader.fsn&0x7) == (con->getFragmentNumber ()+1)%8) ) {
	//printf ("\tReceived last fragment\n");
	ch->size() += con->getFragmentBytes()-HDR_MAC802_16_FRAGSUB_SIZE;
	drop_pkt = false;
      } else {
	//printf ("ERROR with last frag seq=%d (expected=%d)\n", wimaxHdr->fsn&0x7, (con->getFragmentNumber ()+1)%8);
	frag_error = true;
      }     
      con->updateFragmentation (FRAG_NOFRAG, 0, 0); //reset
      break; 
    default:
      fprintf (stderr,"Error, unknown fragmentation type\n");
      exit (-1);
    }
    //if we got an error, or it is a fragment that is not the last, free the packet
    if (drop_pkt) {
      if (frag_error) {
	//update drop stat
	update_watch (&loss_watch_, 1);
	drop (pktRx_, "FRG"); //fragmentation error
      } else {
	//silently discard this fragment.
	Packet::free(pktRx_);
      }
      pktRx_=NULL;
      return;
    } 
  }

  //We check if it is a MAC packet or not
  if (HDR_CMN(pktRx_)->ptype()==PT_MAC) {
    process_mac_packet (pktRx_);
    update_throughput (&rx_traffic_watch_, 8*ch->size());
    mac_log(pktRx_);
    Packet::free(pktRx_);
  }
  else {    
    //only send to upper layer if connected
    if (state_ == MAC802_16_CONNECTED) {
      update_throughput (&rx_data_watch_, 8*ch->size());    
      update_throughput (&rx_traffic_watch_, 8*ch->size());
      ch->size() -= HDR_MAC802_16_SIZE;
      uptarget_->recv(pktRx_, (Handler*) 0);
    }
    else {
      //update drop stat, could be used to detect disconnect
      update_watch (&loss_watch_, 1);
      Packet::free(pktRx_);
      pktRx_=NULL;
      return;
    }
  }

  update_watch (&loss_watch_, 0);
  pktRx_=NULL;
}

//Begin RPI
/**
 * The function is used to process the MAC PDU when ARQ,Fragmentation and Packing are enabled
 * @param con The connection by which it arrived
 * @param p The packet to process
 */
void Mac802_16SS::process_mac_pdu_witharqfragpack (Connection *con, Packet * pkt)
{
  Packet *mac_pdu, *p_current, *p, *mac_sdu, *p_previous, *p_temp;
  hdr_cmn *ch_pdu, *ch_current, *ch, *ch_sdu;
  hdr_mac802_16 *wimaxHdr_pdu, *wimaxHdr_current, *wimaxHdr, *wimaxHdr_sdu;
  double tmp;
  u_int8_t isInOrder;
  u_int32_t seqno;
  bool mac_sdu_gen = false;
  int pdu_size = 0;	
  
  //debug2("ARQ SS : Entering the process_mac_pdu_witharqfragpack function \n");	 
  //The packet received is always MAC PDU
  mac_pdu = pkt;
  wimaxHdr_pdu= HDR_MAC802_16(mac_pdu);
  ch_pdu = HDR_CMN(mac_pdu);
  pdu_size =ch_pdu->size(); 	

  // We shall first handle the acknowledgements received
  if(wimaxHdr_pdu->header.type_arqfb == 1)
    {
      for(u_int16_t i=0; i < wimaxHdr_pdu->num_of_acks; i++)
	{
	  Connection *connection = this->getCManager ()->get_connection(wimaxHdr_pdu->arq_ie[i].cid, true);
	  if(connection)
	    { 
	      if(connection->getArqStatus () && connection->getArqStatus ()->isArqEnabled () == 1){
		//debug2("ARQ : SS Feedback in Data Payload Received: Has a feedback: Value of i:%d , Value of number of acks:%d \n" , i, wimaxHdr_pdu->num_of_acks);
		connection->getArqStatus ()->arqRecvFeedback(mac_pdu, i, connection);
	      }
	    }  
	}
      ch_pdu->size() -= (wimaxHdr_pdu->num_of_acks * HDR_MAC802_16_ARQFEEDBK_SIZE);	
    }

  //Add loss on data connection
  tmp = Random::uniform(0, 1);
  if (tmp < data_loss_) {
    update_watch(&loss_watch_,1); 
    //drop (pktRx_, DROP_MAC_PACKET_ERROR);
		drop (pkt, DROP_MAC_PACKET_ERROR);
    pktRx_ = NULL;
    //debug2 ("ARQ: SS process_mac_pdu_witharqfragpack %f Drop data loss %f %f\n", NOW, tmp, data_loss_);
    return;
  } else {
    //debug2 ("ARQ: SS process_mac_pdu_witharqfragpack %f No drop data loss %f %f\n", NOW, tmp, data_loss_);
  }
  
  // We need to change the size of the first packet to its original size as in transmission logic 
  ch_pdu->size() -= HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE; /*Size of generic Mac Header was set (Note one packet header and first ARQ Block is present)*/
  if(wimaxHdr_pdu->header.ci)
    ch_pdu->size() -= HDR_MAC802_16_CRC_SIZE;	
  // Initial Condition
  p_previous = mac_pdu;	
  p_temp = (Packet*) mac_pdu->accessdata();
 
  while(p_temp)
    {
      // Update the Mac PDU Size
      //debug2("ARQ SS: p-previous :%p p-temp : %p \n", p_previous, p_temp);	
      //debug2 ("ARQ SS: type of ARQ to update size:  %d, %d\n ",HDR_MAC802_16(p_temp)->pack_subheader.fc, (((HDR_MAC802_16(p_temp)->pack_subheader.sn) - (HDR_MAC802_16(p_previous)->pack_subheader.sn)) &0x7FF));	
      if((HDR_MAC802_16(p_temp)->pack_subheader.fc == FRAG_FIRST) || ((((HDR_MAC802_16(p_temp)->pack_subheader.sn) - (HDR_MAC802_16(p_previous)->pack_subheader.sn)) &0x7FF) != 1) || (HDR_MAC802_16(p_temp)->pack_subheader.fc == FRAG_NOFRAG))
	ch_pdu->size() -= HDR_CMN(p_temp)->size()+HDR_MAC802_16_FRAGSUB_SIZE ;  	
      else
	ch_pdu->size() -= HDR_CMN(p_temp)->size();

      //Initial conditions
      p_previous = p_temp;
      p_temp = (Packet*) p_temp->accessdata();
    }	
 // debug2("ARQ SS: Finally the ch_pdu size: %d \n",ch_pdu->size()); 

  // Time to perform ARQ for The MAC_PDU since it is the first ns packet
  if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) {
    isInOrder = 1;
    con->getArqStatus ()->arqReceive(mac_pdu,con,&isInOrder);
    //debug2("Receive: ARQ SS :ORDER is:%d\n",isInOrder);
    if (isInOrder == 0) {
      // packet is out of order, so queue it in retransmission queue at receiver end since it is not being used
      //debug2("Receive: ARQ SS : Out of order packet, buffering in retransmission queue\n");
      con->getArqStatus ()->arq_retrans_queue_->enque(mac_pdu->copy());
    }
    else if (isInOrder == 1){
      // packet is in order, so queue it in transmission queue at receiver end since it is not being used
      //debug2("Receive: ARQ SS : In order packet, buffering in arq transmission queue\n");
      con->getArqStatus ()->arq_trans_queue_->enque(mac_pdu->copy());
    }
    else {
      //debug2("Receive: ARQ SS : Not buffering in arq trans/retrans queue\n");
    }	 
  }
  // The the initial conditions
  p_current = (Packet*) mac_pdu->accessdata();
  
  while(p_current)
    {
      wimaxHdr_current= HDR_MAC802_16(p_current);
      ch_current = HDR_CMN(p_current);

      //Time to perform ARQ for the new packet
      if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) {
	isInOrder = 1;
	con->getArqStatus ()->arqReceive(p_current,con,&isInOrder);
	//debug2("Receive: ARQ SS :ORDER is:%d\n",isInOrder);
	if (isInOrder == 0) {
          // packet is out of order, so queue it in retransmission queue at receiver end since it is not being used
          //debug2("Receive: ARQ SS : Out of order packet, buffering in retransmission queue\n");
          con->getArqStatus ()->arq_retrans_queue_->enque(p_current->copy());
	}
	else if(isInOrder == 1) {
	  // packet is in order, so queue it in transmission queue at receiver end since it is not being used
	  //debug2("Receive: ARQ SS : In order packet, buffering in arq transmission queue\n");
	  con->getArqStatus ()->arq_trans_queue_->enque(p_current->copy());
	}
	else {
	  //debug2("Receive: ARQ SS :  Not buffering in arq trans/retrans queue\n"); 	
	} 
      }
      // The the initial conditions
      p_current = (Packet*) p_current->accessdata();
    }
 
  // Here we will check if packets in retransmission queue can be fit into transmission queue now that a lost packet may have been retransmitted
 primary:
  con->getArqStatus ()->arq_retrans_queue_->resetIterator();
   
  while(con->getArqStatus ()->arq_retrans_queue_)
    {
    secondary:
      p = con->getArqStatus ()->arq_retrans_queue_->getNext ();
     
      if(!p){
	//debug2("ARQ SS: Retransmission queue is empty- so nothing to fit or packets are there but of no use\n");
	break;
      }  		 

      //Time to perform ARQ for the new packet
      if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) {
        isInOrder = 1;
        con->getArqStatus ()->arqReceiveBufferTransfer(p,con,&isInOrder);
	if(isInOrder == 2)
	  {
	    con->getArqStatus ()->arq_retrans_queue_->remove(p);
	    Packet::free(p);
	    goto secondary;		
	  }
        if (isInOrder == 0) {
	  goto secondary;
        }
        else {
          con->getArqStatus ()->arq_trans_queue_->enque(p->copy());
	  con->getArqStatus ()->arq_retrans_queue_->remove(p);
	  Packet::free(p);	
          
	  while(con->getArqStatus ()->arq_retrans_queue_)
	    {

	      p = con->getArqStatus ()->arq_retrans_queue_->getNext ();
	    
	      if(!p){
		//debug2("ARQ SS: Retransmission queue is empty- so nothing to fit or packets are there but of no use\n");
		break;
	      }   		
	      //Time to perform ARQ for the new packet
	      if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) {
		isInOrder = 1;
		con->getArqStatus ()->arqReceiveBufferTransfer(p,con,&isInOrder);
		if(isInOrder == 2)
		  {
		    con->getArqStatus ()->arq_retrans_queue_->remove(p);
		    Packet::free(p);
		    goto primary; 		
		  }  	
		if (isInOrder == 0) {
		  goto primary;
		}
		else {
		  con->getArqStatus ()->arq_trans_queue_->enque(p->copy());
		  con->getArqStatus ()->arq_retrans_queue_->remove(p);
		  Packet::free(p);	
		}
	      }
	    }
	  //break;
	  goto primary;	  					 
        }
	 
      }		    	
    }
   
  //The advantage of seperating the packets in transmission and retransmission queue is the all the packets in transmission queue will be in order.
  while(1) {
    con->getArqStatus ()->arq_trans_queue_->resetIterator();
    p = con->getArqStatus ()->arq_trans_queue_->getNext ();
  
    if(!p) {
      //debug2("ARQ SS: Transmission Queue is Empty \n ");	
      break;
    }	
    wimaxHdr= HDR_MAC802_16(p);
    ch = HDR_CMN(p);

    // This will be first part of the MAC SDU
    if(wimaxHdr->pack_subheader.fc == FRAG_FIRST) {
      mac_sdu = p->copy();
      wimaxHdr_sdu= HDR_MAC802_16(mac_sdu);
      ch_sdu = HDR_CMN(mac_sdu);
      seqno = wimaxHdr_sdu->pack_subheader.sn; 
      //debug2("ARQ SS: This is the first packet in transmission queue and FRAG_FIRST Fsn: %d Size :%d\n",wimaxHdr_sdu->pack_subheader.sn, ch_sdu->size());
      while(1) {
        p =  con->getArqStatus ()->arq_trans_queue_->getNext ();
        if(!p){
	  //debug2("ARQ SS: All ARQ Blocks for the MAC SDU are not present \n");
          break;
	}
        wimaxHdr= HDR_MAC802_16(p);
        ch = HDR_CMN(p);

	if((wimaxHdr->pack_subheader.fc == FRAG_CONT) && (((wimaxHdr->pack_subheader.sn - seqno) &0x7FF)==1)) {
	  //debug2("ARQ SS: Generating the MAC SDU Current SN: %d Previous SN: %d Frag Type: %d Size: %d, %d\n ",wimaxHdr->pack_subheader.sn, seqno, wimaxHdr->pack_subheader.fc, ch->size(), ch_sdu->size()); 	
	  ch_sdu->size() += ch->size();
	  seqno++;	
	}
	else if(wimaxHdr->pack_subheader.fc == FRAG_LAST && (((wimaxHdr->pack_subheader.sn - seqno) &0x7FF)==1)) {
          //debug2("ARQ SS: Generating the MAC SDU Current SN: %d Previous SN: %d Frag Type: %d Size: %d, %d\n ",wimaxHdr->pack_subheader.sn, seqno, wimaxHdr->pack_subheader.fc, ch->size(), ch_sdu->size());
	  ch_sdu->size() += ch->size();
	  mac_sdu_gen = true; 
	  break;		
        }
	else {
          //debug2("ARQ SS:This is a unsual case FSN: %d  and Frag_Type: %d \n",wimaxHdr->pack_subheader.sn, wimaxHdr->pack_subheader.fc);		 
	  break;
	}		
      }
      if(mac_sdu_gen == true){
	//debug2("ARQ SS: Created the Mac SDU SIZE: %d\n ", ch_sdu->size());
        //update_throughput (&rx_data_watch_, 8*pdu_size);    
        //update_throughput (&rx_traffic_watch_, 8*pdu_size);
        ch_sdu->direction() = hdr_cmn::UP; 
        uptarget_->recv(mac_sdu, (Handler*) 0);
	mac_sdu_gen = false;
        // Go ahead and delete all the packets pertaining to this MAC SDU from the transmission queue
	do {
	  p_temp =con->getArqStatus ()->arq_trans_queue_->deque();
	  Packet::free(p_temp);
	  //debug2("ARQ SS: To keep a count on how many times Dequeue function is called \n");
	  p = con->getArqStatus ()->arq_trans_queue_->head();
	  if(!p){
	    goto end;
	  }
	  wimaxHdr= HDR_MAC802_16(p);
	  ch = HDR_CMN(p);
	  //debug2("ARQ SS: Next Packet to Dequeue FSN: %d FRAG_STATUS: %d \n",wimaxHdr->pack_subheader.sn, wimaxHdr->pack_subheader.fc);
	} while(wimaxHdr->pack_subheader.fc != FRAG_FIRST);
		
      }
      // Either no packets in trans queue or all the mac sdu that could be generated are sent above
      else {
	goto end; 
	//debug2("ARQ SS: Either no packets in trans queue or all the mac sdu that could be generated are sent above \n");
      }
    }
    // This will be the only part of the MAC SDU
    else if(wimaxHdr->pack_subheader.fc == FRAG_NOFRAG) {
      mac_sdu = p->copy();
      wimaxHdr_sdu= HDR_MAC802_16(mac_sdu);
      ch_sdu = HDR_CMN(mac_sdu);
      seqno = wimaxHdr_sdu->pack_subheader.sn; 
      //debug2("ARQ SS: This is the only fragment  of MAC SDU FRAG_NOFRAG Fsn: %d Size :%d\n",wimaxHdr_sdu->pack_subheader.sn, ch_sdu->size());
      //debug2("ARQ SS: Created the Mac SDU SIZE: %d\n ", ch_sdu->size());
      //update_throughput (&rx_data_watch_, 8*pdu_size);    
      //update_throughput (&rx_traffic_watch_, 8*pdu_size);
      uptarget_->recv(mac_sdu, (Handler*) 0);
      mac_sdu_gen = false;
      // Go ahead and delete all the packets pertaining to this MAC SDU from the transmission queue
      p_temp =con->getArqStatus ()->arq_trans_queue_->deque();
      Packet::free(p_temp);
      //debug2("ARQ SS: To keep a count on how many times Dequeue function is called \n");
    } 
    else {
      //debug2("ARQ SS: Something really wrong with the implementation\n");	 
      abort();
    }			
  }
 end:
  Packet::free(pkt);	
  update_watch(&loss_watch_,0);
  pktRx_=NULL; 
}
//End RPI

/**
 * Process a MAC packet
 * @param p The MAC packet received
 */
void Mac802_16SS::process_mac_packet (Packet *p) {

  assert (HDR_CMN(p)->ptype()==PT_MAC);
  //*debug10 ("SS %d received MAC packet to process\n", addr());
  
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;

  if (header.ht == 1) {
    //*debug10 ("SS %d received bandwitdh request packet..don't process\n", addr());
    return;
  }
  //Begin RPI
  if((mac802_16_dl_map_frame*) p->accessdata() == NULL)
    {
      if(header.type_arqfb == 1)
	{
	  for(u_int16_t i=0; i < wimaxHdr->num_of_acks; i++)
	    {
	      Connection *connection = this->getCManager ()->get_connection(wimaxHdr->arq_ie[i].cid, true);
	      if(connection)
		{ 
		  if(connection->getArqStatus () && connection->getArqStatus ()->isArqEnabled () == 1){
		    //*debug2("ARQ SS: Feedback Payload Received: Has a feedback: Value of i:%d , Value of number of acks:%d \n" , i, wimaxHdr->num_of_acks);
		    connection->getArqStatus ()->arqRecvFeedback(p, i, connection);
		  }
		}  
	    }
	  Packet::free(p);
	  return ;
	}
    }
  //End RPI
  //we cast to this frame because all management frame start with a type 
  mac802_16_dl_map_frame *frame = (mac802_16_dl_map_frame*) p->accessdata();

  switch (frame->type) {
  case MAC_DL_MAP: 
  {
    getMap()->setStarttime (NOW-HDR_CMN(p)->txtime());
    //debug2 ("SS MAC_DL_MAP At %f frame start at %f\n", NOW, getMap()->getStarttime());
   
    process_dl_map (frame);

    //collect receive signal strength stats
    PeerNode *peer = getPeerNode_head(); 
    if (peer) { //it is possible that during a scan the peer is removed if switching channel after processing the dl_map
      peer->getStatWatch()->update(10*log10(p->txinfo_.RxPr*1e3));
      //*debug ("At %f in Mac %d weighted RXThresh: %e rxp average %e lgd %d\n", NOW, index_, macmib_.lgd_factor_*macmib_.RXThreshold_, pow(10,peer->getStatWatch()->average()/10)/1e3);
      double avg_w = pow(10,(peer->getStatWatch()->average()/10))/1e3;
      
      if ( avg_w < (macmib_.lgd_factor_*macmib_.RXThreshold_)) {
	if (state_==MAC802_16_CONNECTED) {
#ifdef USE_802_21
	  if(mih_){
	    double probability = ((macmib_.lgd_factor_*macmib_.RXThreshold_)-avg_w)/((macmib_.lgd_factor_*macmib_.RXThreshold_)-macmib_.RXThreshold_);
	    Mac::send_link_going_down (addr(), peer->getAddr(), -1, (int)(100*probability), LGD_RC_LINK_PARAM_DEGRADING, eventId_++);
	  }else{
#endif
	    if (!peer->isGoingDown ()) { //when we don't use 802.21, we only want to send the scan request once	    
	      link_scan (NULL); //replaced send_scan_request (); so it checks for pending requests
	    }
#ifdef USE_802_21
	  }
#endif
	  peer->setGoingDown (true);
	}
      }
      else {
	if (peer->isGoingDown()) {
#ifdef USE_802_21
	  Mac::send_link_rollback (addr(), peer->getAddr(), eventId_-1);
#endif
	  peer->setGoingDown (false);
	}
      }
    }
  break;
  }
  case MAC_DCD: 
    //*debug2 ("At SS MAC_DCD \n");
    process_dcd ((mac802_16_dcd_frame*)frame);
    break;
  case MAC_UL_MAP: 
    //*debug2 ("At SS MAC_UL_MAP \n");
    process_ul_map ((mac802_16_ul_map_frame*)frame);
    break;
  case MAC_UCD: 
    //*debug2 ("At SS MAC_UCD \n");
    process_ucd ((mac802_16_ucd_frame*)frame);
    break;
  case MAC_RNG_RSP:
    //*debug2 ("At SS MAC_RNG_RSP \n");
    process_ranging_rsp ((mac802_16_rng_rsp_frame*) frame);
    break;
  case MAC_REG_RSP:
    //*debug2 ("At SS MAC_REG_RSP \n");
    process_reg_rsp ((mac802_16_reg_rsp_frame*) frame);
    break;    
  case MAC_MOB_SCN_RSP:
    process_scan_rsp ((mac802_16_mob_scn_rsp_frame *) frame);
    break;
  case MAC_MOB_BSHO_RSP:
    process_bsho_rsp ((mac802_16_mob_bsho_rsp_frame *) frame);
    break;
  case MAC_MOB_NBR_ADV:
    process_nbr_adv ((mac802_16_mob_nbr_adv_frame *) frame);
    break;
  case MAC_DSA_REQ: 
  case MAC_DSA_RSP: 
  case MAC_DSA_ACK: 
    serviceFlowHandler_->process (p);  // rpi changed pktRx_ to p, coz pktRx_ is not in this scope anymore. 
    break;
  default:
    ;//*debug ("unknown packet in SS %d\n", addr());
    //exit (0);
  }
}

/**
 * Process a DL_MAP message
 * @param frame The dl_map information
 */
void Mac802_16SS::process_dl_map (mac802_16_dl_map_frame *frame)
{
  assert (frame);
  
  //create an entry for the BS
  if (getPeerNode_head ()==NULL)
    addPeerNode (new PeerNode (frame->bsid));

  getMap()->parseDLMAPframe (frame);

  if (getMacState()==MAC802_16_WAIT_DL_SYNCH) {
    //*debug ("At %f in %d, received DL_MAP for synch from %d (substate=%d)\n", 
	   //*NOW, addr(), frame->bsid,scan_info_->substate);
    assert (t21timer_->busy()!=0);
    //synchronization is done
    t21timer_->stop();
    //start lost_dl_map
    lostDLMAPtimer_->start (macmib_.lost_dlmap_interval);
    //start T1: DCD
    t1timer_->start (macmib_.t1_timeout);
    //start T12: UCD
    t12timer_->start (macmib_.t12_timeout);

#ifdef USE_802_21
    if (scan_info_->substate != SCANNING) {
      //*debug ("At %f in Mac %d, send link detected\n", NOW, addr());
      send_link_detected (addr(), frame->bsid, 1);
    }
#endif
    
    setMacState(MAC802_16_WAIT_DL_SYNCH_DCD);

    //if I am doing handoff and we have dcd/ucd information 
    //from scanning, use it
    if (scan_info_->substate == HANDOVER || scan_info_->substate == SCANNING) {
      if (scan_info_->substate == SCANNING) {
	if (scan_info_->nbr == NULL || scan_info_->nbr->getID()!=frame->bsid) {
	  //check if an entry already exist in the database
	  scan_info_->nbr = nbr_db_->getNeighbor (frame->bsid);
	  if (scan_info_->nbr == NULL) {
	    //create entry
	    //debug2 ("Creating nbr info for node %d\n", frame->bsid);
	    scan_info_->nbr = new WimaxNeighborEntry (frame->bsid);
	    nbr_db_->addNeighbor (scan_info_->nbr);
	  } else {
	    //debug2 ("loaded nbr info\n");
	    if (scan_info_->nbr->isDetected ()) {
 	      //we already synchronized with this AP...skip channel
 	      nextChannel();
 	      lost_synch ();
 	      return;
 	    } 
	  }
	}
      }//if HANDOVER, scan_info_->nbr is already set

      bool error = false;
      //we check if we can read the DL_MAP
      mac802_16_dcd_frame *dcd = scan_info_->nbr->getDCD();
      if (dcd!=NULL) {
	//debug2 ("Check if we can decode stored dcd\n");
	//check if we can decode dl_map with previously acquired dcd      
	bool found;
	for (int i = 0 ; !error && i < getMap()->getDlSubframe()->getPdu()->getNbBurst() ; i++) {
	  int diuc = getMap()->getDlSubframe()->getPdu()->getBurst(i)->getIUC();
	  if (diuc == DIUC_END_OF_MAP)
	    continue;
	  found = false;
	  for (u_int32_t j = 0 ; !found && j < dcd->nb_prof; j++) {
	    found = dcd->profiles[j].diuc==diuc;	    
	  }
	  error = !found;
	}
	if (!error)
	  process_dcd (dcd);
      } else {
	//debug2 ("No DCD information found\n");
      }
    }
  } else {
    //maintain synchronization
    assert (lostDLMAPtimer_->busy());
    lostDLMAPtimer_->stop();
    //printf ("update dlmap timer\n");
    lostDLMAPtimer_->start (macmib_.lost_dlmap_interval);

    if (getMacState()!= MAC802_16_WAIT_DL_SYNCH_DCD
	&& getMacState()!=MAC802_16_UL_PARAM) {

      //since the map may have changed, we need to adjust the timer 
      //for the DLSubframe
      double stime = getMap()->getStarttime();
      stime += getMap()->getDlSubframe()->getPdu()->getBurst(1)->getStarttime()*getPhy()->getSymbolTime();
      //printf ("received dl..needs to update expiration to %f, %f,%f\n", stime, NOW,getMap()->getStarttime());
      getMap()->getDlSubframe()->getTimer()->resched (stime-NOW);
      dl_timer_->resched (getMap()->getStarttime()+getFrameDuration()-NOW);
    }
  }
}

/**
 * Process a DCD message
 * @param frame The dcd information
 */
void Mac802_16SS::process_dcd (mac802_16_dcd_frame *frame)
{
  if (getMacState()==MAC802_16_WAIT_DL_SYNCH) {
    //we are waiting for DL_MAP, ignore this message
    return;
  }

  getMap()->parseDCDframe (frame);
  if (getMacState()==MAC802_16_WAIT_DL_SYNCH_DCD) {
    //*debug ("At %f in %d, received DCD for synch\n", NOW, addr());
    //now I have all information such as frame duration
    //adjust timing in case the frame we received the DL_MAP
    //and the DCD is different
    while (NOW - getMap()->getStarttime () > getFrameDuration()) {
      getMap()->setStarttime (getMap()->getStarttime()+getFrameDuration());
    }
    
    //store information to be used during potential handoff
    if (scan_info_->substate == SCANNING) {
      mac802_16_dcd_frame *tmp = (mac802_16_dcd_frame *) malloc (sizeof (mac802_16_dcd_frame));
      memcpy (tmp, frame, sizeof (mac802_16_dcd_frame));
      mac802_16_dcd_frame *old = scan_info_->nbr->getDCD(); 
      if (frame == old)
	frame = tmp;
      if (old)
	free (old); //free previous entry
      scan_info_->nbr->setDCD(tmp);    //set new one
    }

    setMacState(MAC802_16_UL_PARAM);
    //we can schedule next frame
    //printf ("SS schedule next frame at %f\n", getMap()->getStarttime()+getFrameDuration());
    //dl_timer_->sched (getMap()->getStarttime()+getFrameDuration()-NOW);
  }

  if (t1timer_->busy()!=0) {
    //we were waiting for this packet
    t1timer_->stop();
    t1timer_->start (macmib_.t1_timeout);
  }
}

/**
 * Process a UCD message
 * @param frame The ucd information
 */
void Mac802_16SS::process_ucd (mac802_16_ucd_frame *frame)
{
  if (getMacState()==MAC802_16_WAIT_DL_SYNCH
      ||getMacState()==MAC802_16_WAIT_DL_SYNCH_DCD) {
    //discard the packet
    return;
  }
  assert (t12timer_->busy()!=0); //we are waiting for this packet

  if (getMacState()==MAC802_16_UL_PARAM) {
    //check if uplink channel usable
    //debug ("At %f in %d, received UL(UCD) parameters\n", NOW, addr());
    //start T2: ranging
    t2timer_->start (macmib_.t2_timeout);
    //start Lost UL-MAP
    lostULMAPtimer_->start (macmib_.lost_ulmap_interval);

    //store information to be used during potential handoff
    if (scan_info_->substate == SCANNING) {
      mac802_16_ucd_frame *tmp = (mac802_16_ucd_frame *) malloc (sizeof (mac802_16_ucd_frame));
      memcpy (tmp, frame, sizeof (mac802_16_ucd_frame));
      mac802_16_ucd_frame *old = scan_info_->nbr->getUCD(); 
      if (frame == old)
	frame = tmp;
      if (old) 
	free (old); //free previous entry
      scan_info_->nbr->setUCD(tmp);    //set new one            
      
    }

    //change state
    //debug10 (" received ucd and prepare to do init_ranging set state to MAC802_16_RANGING\n");
    setMacState (MAC802_16_RANGING);
  }

  //reset T12
  t12timer_->stop();
  t12timer_->start (macmib_.t12_timeout);

  getMap()->parseUCDframe (frame);
}

/**
 * Process a UL_MAP message
 * @param frame The ul_map information
 */
void Mac802_16SS::process_ul_map (mac802_16_ul_map_frame *frame)
{
  if (getMacState()==MAC802_16_WAIT_DL_SYNCH 
      || getMacState()==MAC802_16_WAIT_DL_SYNCH_DCD) {
    //discard the packet
    //debug("At %f in %d, drop UL_MAP because not in right state (state=%d)\n", NOW, addr(), getMacState());
    return;
  }

  //debug ("At %f in %d, received UL_MAP for synch (substate=%d)\n",
	 //NOW, addr(),scan_info_->substate);

  if (getMacState()==MAC802_16_UL_PARAM) {
    if (scan_info_->substate == HANDOVER || scan_info_->substate==SCANNING) {
      FrameMap *tmpMap = new FrameMap (this);
      tmpMap->parseULMAPframe (frame); 
      //printf ("Checking if we can read UL_MAP\n");
      bool error = false;
      //we check if we can read the UL_MAP
      mac802_16_ucd_frame *ucd = scan_info_->nbr->getUCD();
      if (ucd!=NULL) {
	//check if we can decode ul_map with previously acquired ucd      
	bool found;
	for (int i = 0 ; !error && i < tmpMap->getUlSubframe()->getNbPdu() ; i++) {
	  UlBurst *b = (UlBurst*)tmpMap->getUlSubframe()->getPhyPdu(i)->getBurst(0);
	  int uiuc = b->getIUC();
	  if (uiuc == UIUC_END_OF_MAP)
	    continue;
	  if (uiuc == UIUC_EXT_UIUC && b->getExtendedUIUC ()== UIUC_FAST_RANGING)
 	    uiuc = b->getFastRangingUIUC();	  
	  found = false;
	  for (u_int32_t j = 0 ; !found && j < ucd->nb_prof; j++) {
	    //printf ("\t prof=%d, search=%d\n", ucd->profiles[j].uiuc, uiuc);
	    found = ucd->profiles[j].uiuc==uiuc;	    
	  }
	  error = !found;
	}
	if (!error)
	  process_ucd (ucd);
      }
      delete (tmpMap);
      if (error) {
	//we cannot read message
	return;
      }
    } else
      return;
  }

  if (scan_info_->substate == SCANNING) {
    //TBD: add checking scanning type for the given station
    u_char scanning_type = 0;
    for (int i = 0 ; i < scan_info_->rsp->n_recommended_bs_full ; i++) {
      if (scan_info_->rsp->rec_bs_full[i].recommended_bs_id == scan_info_->nbr->getID()) {
	scanning_type = scan_info_->rsp->rec_bs_full[i].scanning_type;
	break;
      }
    }
    if (scanning_type == 0) {
      //store information about possible base station and keep scanning
      scan_info_->nbr->getState()->state_info= backup_state();
      //*debug ("At %f in Mac %d bs %d detected during scanning\n", NOW, addr(), scan_info_->nbr->getID());
      scan_info_->nbr->setDetected (true);
      nextChannel();
      lost_synch ();
      return;
    }
  }

  getMap()->parseULMAPframe (frame);  
  if (getMacState()==MAC802_16_RANGING) 
  {
    //execute ranging
    assert (t2timer_->busy()!=0); //we are waiting for this packet
    init_ranging ();
  }

  //schedule when to take care of outgoing packets
  double start = getMap()->getStarttime();
  start += getMap()->getUlSubframe()->getStarttime()*getPhy()->getPS(); //offset for ul subframe
  start -= NOW; //works with relative time not absolute
  //*debug2 ("Uplink starts in %f (framestate=%f) %f %f\n", 
	  //*start, 
	  //*getMap()->getStarttime(),
	  //*getFrameDuration()/getPhy()->getPS(), 
	  //*getFrameDuration()/getPhy()->getSymbolTime());
  
  ul_timer_->resched (start);

  //reset Lost UL-Map
  lostULMAPtimer_->stop();
  lostULMAPtimer_->start (macmib_.lost_ulmap_interval);
}

/**
 * Process a ranging response message 
 * @param frame The ranging response frame
 */
void Mac802_16SS::process_ranging_rsp (mac802_16_rng_rsp_frame *frame)
{
  //check the destination
  if (frame->ss_mac_address != addr())
    return;
  
  Connection *basic, *primary;
  PeerNode *peer;

  //TBD: add processing for periodic ranging

  //check status 
  switch (frame->rng_status) {
  case RNG_SUCCESS:
    //*debug ("Ranging response (remove cdma intial ranging request) : status = Success.Basic :%d, Primary :%d\n",
	   //*frame->basic_cid, frame->primary_cid);

    peer = getPeerNode_head();
    assert (peer);
//getMap()->getUlSubframe()->getRanging()->removeRequest ();
    getMap()->getUlSubframe()->getRanging()->removeRequest_mac (addr());
    Connection *c_tmp;
    c_tmp  = getCManager ()->get_connection (0, true);
    c_tmp->setINIT_REQ_QUEUE(addr(), 0);

    if (scan_info_->substate == SCANNING) {
      //store information about possible base station and keep scanning
      scan_info_->nbr->getState()->state_info= backup_state();
      scan_info_->nbr->setDetected (true);
      //keep the information for later
      mac802_16_rng_rsp_frame *tmp = (mac802_16_rng_rsp_frame *) malloc (sizeof (mac802_16_rng_rsp_frame));
      memcpy (tmp, frame, sizeof (mac802_16_rng_rsp_frame));
      scan_info_->nbr->setRangingRsp (tmp);
      nextChannel();
      lost_synch ();
      return;
    }

    //ranging worked, now we must register
    basic = peer->getBasic(IN_CONNECTION);
    primary = peer->getPrimary(IN_CONNECTION);
    if (basic!=NULL && basic->get_cid ()==frame->basic_cid) {
      //duplicate response
      assert (primary->get_cid () == frame->primary_cid);
    } else {
      if (basic !=NULL) {
	//we have been allocated new cids..clear old ones
	getCManager ()->remove_connection (basic->get_cid());
	getCManager ()->remove_connection (primary->get_cid());
	if (peer->getSecondary(IN_CONNECTION)!=NULL) {
	  getCManager ()->remove_connection (peer->getSecondary(IN_CONNECTION));
	  getCManager ()->remove_connection (peer->getSecondary(OUT_CONNECTION));
	}
	if (peer->getOutData()!=NULL)
	  getCManager ()->remove_connection (peer->getOutData());
	if (peer->getInData()!=NULL)
	  getCManager ()->remove_connection (peer->getInData());
      } 

      basic = new Connection (CONN_BASIC, frame->basic_cid);
      Connection *upbasic = new Connection (CONN_BASIC, frame->basic_cid);
//      upbasic->setBW_REQ_QUEUE(0);
//      upbasic->initCDMA_SSID();
      primary = new Connection (CONN_PRIMARY, frame->primary_cid);
      Connection *upprimary = new Connection (CONN_PRIMARY, frame->primary_cid);
//      upprimary->setBW_REQ_QUEUE(0);
//      upprimary->initCDMA_SSID();

      //a SS should only have one peer, the BS
      peer->setBasic (basic, upbasic); //set outgoing
      peer->setPrimary (primary, upprimary); //set outgoing
      getCManager()->add_connection (upbasic, OUT_CONNECTION);
      getCManager()->add_connection (basic, IN_CONNECTION);
      getCManager()->add_connection (upprimary, OUT_CONNECTION);
      getCManager()->add_connection (primary, IN_CONNECTION);
    }

    //registration must be sent using Primary Management CID
    setMacState (MAC802_16_REGISTER);
    //stop timeout timer
    t2timer_->stop ();
    nb_reg_retry_ = 0; //first time sending
    send_registration();

    break;
  case RNG_ABORT:
  case RNG_CONTINUE:
  case RNG_RERANGE:
    break;
  default:
    fprintf (stderr, "Unknown status reply\n");
    exit (-1);
  }
}

/**
 * Schedule a ranging
 */
void Mac802_16SS::init_ranging ()
{
  //check if there is a ranging opportunity
  UlSubFrame *ulsubframe = getMap()->getUlSubframe();
  DlSubFrame *dlsubframe = getMap()->getDlSubframe();
  PeerNode *peer = getPeerNode_head();

  // If I am doing a Handoff, check if I already associated 
  // with the target AP
  //printf(" inside init ranging" );  

  if (scan_info_->substate == HANDOVER && scan_info_->nbr->getRangingRsp()!=NULL) {
    //*debug ("At %f in Mac %d MN already executed ranging during scanning\n", NOW, addr());
    process_ranging_rsp (scan_info_->nbr->getRangingRsp());
    return;
  }

  //check if there is Fast Ranging IE
  for (PhyPdu *p = getMap()->getUlSubframe ()->getFirstPdu(); p ; p= p ->next_entry()) {
    UlBurst *b = (UlBurst*) p->getBurst(0);
    if (b->getIUC() == UIUC_EXT_UIUC && 
	b->getExtendedUIUC ()== UIUC_FAST_RANGING &&
	b->getFastRangingMacAddr ()==addr()) {
      //debug2 ("Found fast ranging\n");
      //we should put the ranging request in that burst
      Packet *p= getPacket();
      hdr_cmn* ch = HDR_CMN(p);
      HDR_MAC802_16(p)->header.cid = INITIAL_RANGING_CID; 

      p->allocdata (sizeof (struct mac802_16_rng_req_frame));
      mac802_16_rng_req_frame *frame = (mac802_16_rng_req_frame*) p->accessdata();
      frame->type = MAC_RNG_REQ;
      frame->dc_id = dlsubframe->getChannelID();
      frame->ss_mac_address = addr();
      frame->channel_num = GetInitialChannel();
      peer->setchannel(frame->channel_num);
      //other elements??      
      frame->req_dl_burst_profile = default_diuc_ & 0xF; //we use lower bits only
      ch->size() += RNG_REQ_SIZE;

      //fill phyinfo header
      hdr_mac802_16 *wimaxHdr;

      wimaxHdr = HDR_MAC802_16(p);
      wimaxHdr->phy_info.num_subchannels = getPhy()->getNumsubchannels(UL_);
      wimaxHdr->phy_info.subchannel_offset = 0; //richard: changed 1->0
      wimaxHdr->phy_info.num_OFDMSymbol = 0;
      wimaxHdr->phy_info.OFDMSymbol_offset = 0; //initial_offset;
      wimaxHdr->phy_info.channel_index = frame->channel_num;
      wimaxHdr->phy_info.direction = 1;
 

      //compute when to send message
      double txtime = getPhy()->getTrxTime (ch->size(), ulsubframe->getProfile (b->getFastRangingUIUC ())->getEncoding());
      ch->txtime() = txtime;
      //starttime+backoff
      ch->timestamp() = NOW; //add timestamp since it bypasses the queue
      b->enqueue(p);
      setMacState(MAC802_16_WAIT_RNG_RSP);
      return;
    }
  }


  for (PhyPdu *pdu = ulsubframe->getFirstPdu(); pdu ; pdu = pdu->next_entry()) 
  {
    if (pdu->getBurst(0)->getIUC()==UIUC_INITIAL_RANGING) 
    {
      //*debug10 ("At %f (SS) Mac %d found ranging opportunity => prepare to create MAC_RNG_REQ (cdma_init_ranging proccess)\n", NOW, addr());
/*
//Old code 
      Packet *p= getPacket();
      hdr_cmn* ch = HDR_CMN(p);
      HDR_MAC802_16(p)->header.cid = INITIAL_RANGING_CID;

      p->allocdata (sizeof (struct mac802_16_rng_req_frame));
      mac802_16_rng_req_frame *frame = (mac802_16_rng_req_frame*) p->accessdata();
      frame->type = MAC_RNG_REQ;
      frame->dc_id = dlsubframe->getChannelID();
      frame->ss_mac_address = addr();
      frame->channel_num = GetInitialChannel();
      peer->setchannel(frame->channel_num);
      //other elements??      
      frame->req_dl_burst_profile = default_diuc_ & 0xF; //we use lower bits only
      ch->size() += RNG_REQ_SIZE;
      //fill phyinfo header
      hdr_mac802_16 *wimaxHdr;

      wimaxHdr = HDR_MAC802_16(p);
      wimaxHdr->phy_info.num_subchannels = getPhy()->getNumsubchannels(UL_);
      wimaxHdr->phy_info.subchannel_offset = 0; //richard: changed 1->0
      wimaxHdr->phy_info.num_OFDMSymbol = 0;
      wimaxHdr->phy_info.OFDMSymbol_offset = 0; //initial_offset;
      wimaxHdr->phy_info.channel_index = frame->channel_num;
      wimaxHdr->phy_info.direction = 1;
      //compute when to send message
      double txtime = getPhy()->getTrxTime (ch->size(), ulsubframe->getProfile (pdu->getBurst(0)->getIUC())->getEncoding());
      ch->txtime() = txtime;
      //starttime+backoff
      debug10 (" symbol_offset :%d, symbol :%d, subchannel_offset :%d, subchannel :%d, channel_index :%d, direction :%d, txtime :%e\n", wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.subchannel_offset, wimaxHdr->phy_info.num_subchannels, wimaxHdr->phy_info.channel_index , wimaxHdr->phy_info.direction, ch->txtime());
      getMap()->getUlSubframe()->getRanging()->addRequest (p);
*/

      if (getMap()->getUlSubframe()->getRanging()->getRequest_mac (addr())!=NULL) {
        //*debug2 ("Create CDMA-INIT-REQ, SS addr :%d -- already pending requests => return\n", addr());
        continue;  
      } else {
        //*debug2("Create CDMA-INIT-REQ, SS addr :%d\n", addr());
      }

      ContentionSlot *slot1 = getMap()->getUlSubframe()->getRanging ();

      int s_init_contention_size = slot1->getSize(); //0 to 4
      int s_nbretry = 0;
      int s_backoff_start = slot1->getBackoff_start();
      int s_backoff_stop = slot1->getBackoff_stop();
      int s_window = s_backoff_start;
      int fix_six_subchannel = 6;
      int result = rand() % ((int)(pow (2, s_window)+1));
      int s_backoff = floor(result / s_init_contention_size); 
      int sub_channel_off = result % s_init_contention_size;
      int s_flagtransmit = 0;
      int s_flagnowtransmit = 0;

      Packet *p= getPacket();
      hdr_cmn* ch = HDR_CMN(p);
      hdr_mac802_16 *wimaxHdr;
      HDR_MAC802_16(p)->header.cid = INITIAL_RANGING_CID;
      p->allocdata (6);

      wimaxHdr = HDR_MAC802_16(p);
      wimaxHdr->cdma = 1;
      wimaxHdr->phy_info.num_subchannels = fix_six_subchannel;
      wimaxHdr->phy_info.subchannel_offset = sub_channel_off*fix_six_subchannel;
      wimaxHdr->phy_info.num_OFDMSymbol = 2;
      wimaxHdr->phy_info.OFDMSymbol_offset = 0; //initial_offset;
      wimaxHdr->phy_info.direction = 1;

      /*Xingting randomly assign channel index for MS.*/
	  wimaxHdr->phy_info.channel_index = GetInitialChannel();
      
      u_char header_top = sub_channel_off;

      int code_range = macmib_.cdma_code_init_start - macmib_.cdma_code_init_stop + 1;
      int c_rand = rand() % code_range;
      u_char header_code = (u_char)macmib_.cdma_code_init_start + (u_char)c_rand;
//      debug10 (" CODE init start :%d, stop :%d, rand :%d, final code :%d\n", macmib_.cdma_code_init_start, macmib_.cdma_code_init_stop, c_rand, header_code);

//      u_char header_code = rand() % (MAXCODE-1);
      cdma_req_header_t *header = (cdma_req_header_t *)&(HDR_MAC802_16(p)->header);
      header->ht=1;
      header->ec=1;
      header->type = 0x2;
      header->top = header_top;
      header->br = addr();
      header->code = header_code;
      header->cid = INITIAL_RANGING_CID;
      double txtime = 2*getPhy()->getSymbolTime();
      ch->txtime() = txtime;

      //debug10 (" SSscheduler enqueued cdma_initial ranging request for cid :%d, q-len :%d, (nbPacket :%d), code :%d, top :%d, ss_id :%d, backoff :%d, window :%d, nbretry :%d, flagtransmit :%d, flagnowtransmit :%d\n", INITIAL_RANGING_CID, 0, 0, header_code, header_top, header->br, result, s_window, s_nbretry, s_flagtransmit, s_flagnowtransmit);

      //debug10 (" symbol_offset :%d, symbol :%d, subchannel_offset :%d, subchannel :%d, channel_index :%d, direction :%d, txtime :%e\n", wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.subchannel_offset, wimaxHdr->phy_info.num_subchannels, wimaxHdr->phy_info.channel_index , wimaxHdr->phy_info.direction, ch->txtime());

      getMap()->getUlSubframe()->getRanging()->addRequest (p, INITIAL_RANGING_CID, 0, result, CDMA_TIMEOUT, s_nbretry, s_window, (int)header_code, (int)header_top, s_flagtransmit, s_flagnowtransmit, addr());

      setMacState(MAC802_16_WAIT_RNG_RSP);

      return;
    }
  }
}

/**
 * Prepare to send a registration message
 */
void Mac802_16SS::send_registration ()
{
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_reg_req_frame *reg_frame;
  PeerNode *peer;

  //create packet for request
  p = getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_reg_req_frame));
  reg_frame = (mac802_16_reg_req_frame*) p->accessdata();
  reg_frame->type = MAC_REG_REQ;
  ch->size() += REG_REQ_SIZE;

  peer = getPeerNode_head();  
  wimaxHdr->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
  peer->getPrimary(OUT_CONNECTION)->enqueue (p);
  //*debug2(" sending registration\n");

  //start reg timeout
  if (t6timer_==NULL) {
    t6timer_ = new WimaxT6Timer (this);
  }
  t6timer_->start (macmib_.t6_timeout);
  nb_reg_retry_++;
}

/**
 * Process a registration response message 
 * @param frame The registration response frame
 */
void Mac802_16SS::process_reg_rsp (mac802_16_reg_rsp_frame *frame)
{
  //check the destination
  PeerNode *peer = getPeerNode_head();

  //*debug2(" registration response recvd \n " ); 

  if (frame->response == 0) {
    //status OK
    //*debug ("At %f (SS) in Mac %d, registration sucessful (nbretry=%d)\n", NOW, addr(),
	   //*nb_reg_retry_);
    //*debug2 ("At %f (SS) in Mac %d, registration sucessful (nbretry=%d)\n", NOW, addr(),
	    //*nb_reg_retry_);
    Connection *secondary = peer->getSecondary(IN_CONNECTION);
    if (!secondary) {
      Connection *secondary = new Connection (CONN_SECONDARY, frame->sec_mngmt_cid);
//      secondary->setBW_REQ_QUEUE(0);
//      secondary->initCDMA_SSID();
      Connection *upsecondary = new Connection (CONN_SECONDARY, frame->sec_mngmt_cid);
//      upsecondary->setBW_REQ_QUEUE(0);
//      upsecondary->initCDMA_SSID();
      getCManager()->add_connection (upsecondary, OUT_CONNECTION);
      getCManager()->add_connection (secondary, IN_CONNECTION);
      peer->setSecondary (secondary, upsecondary);
    }
    //cancel timeout (could be idle if receiving retransmissions)
    if (t6timer_->busy()!=0) {
      t6timer_->stop ();
    }
    //update status
    setMacState(MAC802_16_CONNECTED);
    /*
    //we need to setup a data connection (will be moved to service flow handler)
    getServiceHandler ()->sendFlowRequest (peer->getAddr(), OUT_CONNECTION);
    getServiceHandler ()->sendFlowRequest (peer->getAddr(), IN_CONNECTION);
    */
    // we will setup the static flows created by the tcl interface
    getServiceHandler ()->init_static_flows (peer->getAddr());

#ifdef USE_802_21
    if (scan_info_->substate==HANDOVER) {
      //*debug ("At %f in Mac %d link handoff complete\n", NOW, addr());      
      send_link_handover_complete (addr(), scan_info_->serving_bsid, peer->getAddr());
      scan_info_->handoff_timeout = -1;
    }
    //*debug ("At %f in Mac %d, send link up\n", NOW, addr());
    send_link_up (addr(), peer->getAddr(), -1);
#endif
    
  } else {
    //status failure
    //*debug ("At %f in Mac %d, registration failed (nbretry=%d)\n", NOW, addr(),
	   //*nb_reg_retry_);
    if (nb_reg_retry_ == macmib_.reg_req_retry) {
#ifdef USE_802_21
      if (scan_info_ && scan_info_->handoff_timeout == -2) {
	//*debug ("At %f in Mac %d link handoff failure\n", NOW, addr());      
	//send_link_handoff_failure (addr(), scan_info_->serving_bsid, peer->getAddr());
	scan_info_->handoff_timeout = -1;
      }
#endif
      lost_synch ();
    } else {
      send_registration();
    }
  }
}

/**
 * Send a scanning message to the serving BS
 */
void Mac802_16SS::send_scan_request ()
{
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_scn_req_frame *req_frame;
  PeerNode *peer;

  //if the mac is not connected, cannot send the request
  if (getMacState() != MAC802_16_CONNECTED) {
    //*debug ("At %f in Mac %d scan request invalid because MAC is disconnected\n", NOW, addr());
    return;
  }


  //*debug ("At %f in Mac %d enqueue scan request\n", NOW, addr());

  //create packet for request
  p = getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_scn_req_frame));
  req_frame = (mac802_16_mob_scn_req_frame*) p->accessdata();
  req_frame->type = MAC_MOB_SCN_REQ;

  req_frame->scan_duration = macmib_.scan_duration;
  req_frame->interleaving_interval = macmib_.interleaving;
  req_frame->scan_iteration = macmib_.scan_iteration;
  req_frame->n_recommended_bs_index = 0;
  req_frame->n_recommended_bs_full = 0;

  ch->size() += Mac802_16pkt::getMOB_SCN_REQ_size(req_frame);
  peer = getPeerNode_head();  
  wimaxHdr->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
  peer->getPrimary(OUT_CONNECTION)->enqueue (p);

  //start reg timeout
  if (t44timer_==NULL) {
    t44timer_ = new WimaxT44Timer (this);
  }
  t44timer_->start (macmib_.t44_timeout);
  nb_scan_req_++;
}

/**
 * Process a scanning response message 
 * @param frame The scanning response frame
 */
void Mac802_16SS::process_scan_rsp (mac802_16_mob_scn_rsp_frame *frame)
{
  //PeerNode *peer = getPeerNode_head();
  if (!t44timer_->busy()) {
    //we are receiving the response too late..ignore
    //*debug ("At %f in Mac %d, scan response arrives too late\n", NOW, addr());
    return;
  }


  if (frame->scan_duration != 0) {
    //scanning accepted
    //*debug ("At %f in Mac %d, scanning accepted (dur=%d it=%d)\n", NOW, addr(), frame->scan_duration,frame->scan_iteration );
    //allocate data for scanning
    //scan_info_ = (struct scanning_structure *) malloc (sizeof (struct scanning_structure));
    //store copy of frame
    
    scan_info_->rsp = (struct mac802_16_mob_scn_rsp_frame *) malloc (sizeof (struct mac802_16_mob_scn_rsp_frame));
    memcpy (scan_info_->rsp, frame, sizeof (struct mac802_16_mob_scn_rsp_frame));
    scan_info_->iteration = 0;
    scan_info_->count = frame->start_frame;
    scan_info_->substate = SCAN_PENDING;
    scan_info_->handoff_timeout = 0; 
    scan_info_->serving_bsid = getPeerNode_head()->getAddr();
    scan_info_->nb_rdv_timers = 0;

    //mark all neighbors as not detected
    for (int i = 0 ; i < nbr_db_->getNbNeighbor() ; i++) {
      nbr_db_->getNeighbors()[i]->setDetected(false);
    }

    //schedule timer for rdv time (for now just use full)
    //TBD: add rec_bs_index
    //debug ("\tstart scan in %d frames (%f)\n",frame->start_frame,NOW+frame->start_frame*getFrameDuration());
    for (int i = 0 ; i < scan_info_->rsp->n_recommended_bs_full ; i++) {
      if (scan_info_->rsp->rec_bs_full[i].scanning_type ==SCAN_ASSOC_LVL1 
	  || scan_info_->rsp->rec_bs_full[i].scanning_type==SCAN_ASSOC_LVL2) {
	//debug2 ("Creating timer for bs=%d at time %f\n", 
		//scan_info_->rsp->rec_bs_full[i].recommended_bs_id, 
		//NOW+getFrameDuration()*scan_info_->rsp->rec_bs_full[i].rdv_time);
	assert (nbr_db_->getNeighbor (scan_info_->rsp->rec_bs_full[i].recommended_bs_id));
	//get the channel
	int ch = getChannel (nbr_db_->getNeighbor (scan_info_->rsp->rec_bs_full[i].recommended_bs_id)->getDCD ()->frequency*1000);
	assert (ch!=-1);
	WimaxRdvTimer *timer = new WimaxRdvTimer (this, ch);
	scan_info_->rdv_timers[scan_info_->nb_rdv_timers++] = timer;
	timer->start(getFrameDuration()*scan_info_->rsp->rec_bs_full[i].rdv_time);
      }
    }

  } else {
    //*debug ("At %f in Mac %d, scanning denied\n", NOW, addr());
    //what do I do???
  }

  t44timer_->stop();
  nb_scan_req_ = 0;
}

/**
 * Send a MSHO-REQ message to the BS
 */
void Mac802_16SS::send_msho_req ()
{
	//*debug2("SS %d send a handover request..\n", addr());
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_msho_req_frame *req_frame;
  double rssi;

  PeerNode *peer = getPeerNode_head();

  int nbPref = 0;
  for (int i = 0 ; i < nbr_db_->getNbNeighbor() ; i++) {
    WimaxNeighborEntry *entry = nbr_db_->getNeighbors()[i];
    if (entry->isDetected()) {
      //*debug ("At %f in Mac %d Found new AP %d..need to send HO message\n",NOW, addr(), entry->getID());
      nbPref++;
    }  
  }

  if (nbPref==0)
    return; //no other BS found

  //create packet for request
  p = getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_msho_req_frame)+nbPref*sizeof (mac802_16_mob_msho_req_bs_index));
  req_frame = (mac802_16_mob_msho_req_frame*) p->accessdata();
  memset (req_frame, 0, sizeof (mac802_16_mob_msho_req_bs_index));
  req_frame->type = MAC_MOB_MSHO_REQ;
  
  req_frame->report_metric = 0x2; //include RSSI
  req_frame->n_new_bs_index = 0;
  req_frame->n_new_bs_full = nbPref;
  req_frame->n_current_bs = 1;
  rssi = getPeerNode_head()->getStatWatch()->average();
  //debug2 ("RSSI=%e, %d, peer bs=%d\n", rssi, (u_char)((rssi+103.75)/0.25), getPeerNode_head()->getAddr());
  req_frame->bs_current[0].temp_bsid = getPeerNode_head()->getAddr();
  req_frame->bs_current[0].bs_rssi_mean = (u_char)((rssi+103.75)/0.25);
  for (int i = 0, j=0; i < nbr_db_->getNbNeighbor() ; i++) {
    WimaxNeighborEntry *entry = nbr_db_->getNeighbors()[i];
    //TBD: there is an error measuring RSSI for current BS during scanning
    //anyway, we don't put it in the least, so it's ok for now
    if (entry->isDetected() && entry->getID()!= getPeerNode_head()->getAddr()) {
      req_frame->bs_full[j].neighbor_bs_index = entry->getID();
      rssi = entry->getState()->state_info->peer_list->lh_first->getStatWatch()->average();
      //debug2 ("RSSI=%e, %d, neighbor bs=%d\n", rssi, (u_char)((rssi+103.75)/0.25), req_frame->bs_full[j].neighbor_bs_index);
      req_frame->bs_full[j].bs_rssi_mean = (u_char)((rssi+103.75)/0.25);
      //the rest of req_frame->bs_full is unused for now..
      req_frame->bs_full[j].arrival_time_diff_ind = 0;
      j++;
    }
  }
  
  ch->size() += Mac802_16pkt::getMOB_MSHO_REQ_size(req_frame);
  wimaxHdr->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
  peer->getPrimary(OUT_CONNECTION)->enqueue (p);
}

/**
 * Process a BSHO-RSP message 
 * @param frame The handover response frame
 */
void Mac802_16SS::process_bsho_rsp (mac802_16_mob_bsho_rsp_frame *frame)
{
  //*debug ("At %f in Mac %d, received handover response\n", NOW, addr());
 
  //go and switch to the channel recommended by the BS
  int targetBS = frame->n_rec[0].neighbor_bsid;
  PeerNode *peer = getPeerNode_head();      

  if (peer->getAddr ()==targetBS) {
    //debug ("\tDecision to stay in current BS\n");
    return;
  }
  scan_info_->nbr = nbr_db_->getNeighbor (targetBS);

  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_ho_ind_frame *ind_frame;
  
  
  p = getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_ho_ind_frame));
  ind_frame = (mac802_16_mob_ho_ind_frame*) p->accessdata();
  ind_frame->type = MAC_MOB_HO_IND;
  
  ind_frame->mode = 0; //HO
  ind_frame->ho_ind_type = 0; //Serving BS release
  ind_frame->rng_param_valid_ind = 0;
  ind_frame->target_bsid = targetBS;
  
  ch->size() += Mac802_16pkt::getMOB_HO_IND_size(ind_frame);
  wimaxHdr->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
  peer->getPrimary(OUT_CONNECTION)->enqueue (p);
  
#ifdef USE_802_21
  send_link_handover_imminent (addr(), peer->getAddr(), targetBS);
  //debug ("At %f in Mac %d link handover imminent\n", NOW, addr());
#endif 
  
  //*debug ("\tHandover to BS %d\n", targetBS);
  scan_info_->handoff_timeout = 20;
  scan_info_->substate = HANDOVER_PENDING;

  Connection *head  = getCManager()->get_out_connection();
  while(head != NULL)
  {
  		//debug2("SS ARQ handling out cid %d\n", head->get_cid());
		if((head->getArqStatus() != NULL))
		{
			//debug2("SS delete ARQ timer CID is [%d]\n",head->get_cid());
			head->getArqStatus()->cancelTimer();
			//delete head->getArqStatus();
			//getCManager()->remove_connection (head);
			//delete head;
		}
		head = head->next_entry();
  }
  //setChannel (scan_info_->bs_infos[i].channel);
  //lost_synch ();  
}


/**
 * Process a NBR_ADV message 
 * @param frame The handover response frame
 */
void Mac802_16SS::process_nbr_adv (mac802_16_mob_nbr_adv_frame *frame)
{
  //*debug ("At %f in Mac %d, received neighbor advertisement\n", NOW, addr());

  //mac802_16_mob_nbr_adv_frame *copy;
  //copy  = (mac802_16_mob_nbr_adv_frame *) malloc (sizeof (mac802_16_mob_nbr_adv_frame));
  //memcpy (copy, frame, sizeof (mac802_16_mob_nbr_adv_frame));
  
  //all we need is to store the information. We will process that only
  //when we will look for another station
  for (int i = 0 ; i < frame->n_neighbors ; i++) {
    int nbrid = frame->nbr_info[i].nbr_bsid;
    mac802_16_nbr_adv_info *info = (mac802_16_nbr_adv_info *) malloc (sizeof(mac802_16_nbr_adv_info));
    WimaxNeighborEntry *entry = nbr_db_->getNeighbor (nbrid);
    if (entry==NULL){
      entry = new WimaxNeighborEntry (nbrid);
      nbr_db_->addNeighbor (entry);
    }
    memcpy(info, &(frame->nbr_info[i]), sizeof(mac802_16_nbr_adv_info));
    if (entry->getNbrAdvMessage ())
      free (entry->getNbrAdvMessage());
    entry->setNbrAdvMessage(info);
    if (info->dcd_included) {
      //set DCD 
      mac802_16_dcd_frame *tmp = (mac802_16_dcd_frame *)malloc (sizeof(mac802_16_dcd_frame));
      memcpy(tmp, &(info->dcd_settings), sizeof(mac802_16_dcd_frame));
      entry->setDCD(tmp);
    }
    else 
      entry->setDCD(NULL);
    if (info->ucd_included) {
      //set DCD 
      mac802_16_ucd_frame *tmp = (mac802_16_ucd_frame *)malloc (sizeof(mac802_16_ucd_frame));
      memcpy(tmp, &(info->ucd_settings), sizeof(mac802_16_ucd_frame));
      entry->setUCD(tmp);
#ifdef DEBUG_WIMAX
      //debug2 ("Dump information nbr in Mac %d for nbr %d %lx\n", addr(), nbrid, (long)tmp);
      int nb_prof = tmp->nb_prof;
      mac802_16_ucd_profile *profiles = tmp->profiles;
      for (int i = 0 ; i < nb_prof ; i++) {
	//debug2 ("\t Reading ul profile %i: f=%d, rate=%d, iuc=%d\n", i, 0, profiles[i].fec, profiles[i].uiuc);
      }
#endif
    }
    else
      entry->setUCD(NULL);
  }  

}

/**** Internal methods ****/

#ifdef USE_802_21

/*
 * Connect to the PoA
 */
void Mac802_16SS::link_connect(int poa)
{
  //*debug ("At %f in Mac %d, received link connect to BS %d\n", NOW, addr(), poa);
 
  set_mode (NORMAL_MODE);

  //go and switch to the channel recommended by the BS
  int targetBS = poa;
  PeerNode *peer = getPeerNode_head();      

  if (peer->getAddr ()==targetBS) {
    //debug ("\tDecision to stay in current BS\n");
    return;
  }
  scan_info_->nbr = nbr_db_->getNeighbor (targetBS);

  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_ho_ind_frame *ind_frame;
  
  
  p = getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_ho_ind_frame));
  ind_frame = (mac802_16_mob_ho_ind_frame*) p->accessdata();
  ind_frame->type = MAC_MOB_HO_IND;
  
  ind_frame->mode = 0; //HO
  ind_frame->ho_ind_type = 0; //Serving BS release
  ind_frame->rng_param_valid_ind = 0;
  ind_frame->target_bsid = targetBS;
  
  ch->size() += Mac802_16pkt::getMOB_HO_IND_size(ind_frame);
  wimaxHdr->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
  peer->getPrimary(OUT_CONNECTION)->enqueue (p);
  
#ifdef USE_802_21
  send_link_handover_imminent (addr(), peer->getAddr(), targetBS);
  //*debug ("At %f in Mac %d link handover imminent\n", NOW, addr());
  
#endif 
  
  //*debug ("\tHandover to BS %d\n", targetBS);
  scan_info_->handoff_timeout = 20;
  scan_info_->substate = HANDOVER_PENDING;
  //setChannel (scan_info_->bs_infos[i].channel);
  //lost_synch ();    
}


/*
 * Disconnect from the PoA
 */
void Mac802_16SS::link_disconnect ()
{
  //force losing synchronization
  lost_synch ();
  set_mode (POWER_DOWN); //not sure if we should turn it off
}


/*
 * Set the operation mode
 * @param mode The new operation mode
 * @return true if transaction succeded
 */
bool Mac802_16SS::set_mode (mih_operation_mode_t mode)
{
  switch (mode) {
  case NORMAL_MODE:
    if (op_mode_ != NORMAL_MODE) {
      getPhy()->node_on(); //turn on phy
      //*debug ("Turning on mac\n");
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
      //*debug ("Turning off mac\n");
    }
    op_mode_ = mode;
    return true;
    break;
  default:
    return false;
  }
}

#endif

/*
 * Scan chanels
 */
void Mac802_16SS::link_scan (void *req)
{
  if(!isScanRunning()){
    setScanFlag(true);  
    send_scan_request ();
  }
}

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_16SS::update_watch (StatWatch *watch, double value)
{
  char *name;

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
  threshold_action_t action = watch->update (value);

  if (action != NO_ACTION_TH) {
    link_parameter_type_s param;
    union param_value old_value, new_value;

    if (watch == &loss_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_FRAME_LOSS;
    } else if (watch == &delay_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_PACKET_DELAY;
    } else if (watch == &jitter_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_PACKET_JITTER;
    }
    old_value.data_d = watch->old_average();
    new_value.data_d = watch->average();

    send_link_parameters_report (addr(), getPeerNode_head()->getAddr(), param, old_value, new_value);      
  }
#else
  watch->update (value);
#endif

  if (watch == &loss_watch_) {
    name = "loss";
  } else if (watch == &delay_watch_) {
    name = "delay";
  } else if (watch == &jitter_watch_) {
    name = "jitter";
  } else {
    name = "other";
  }
  //*if (print_stats_)
    //*debug2 ("At %f in Mac %d, updating stats %s: %f\n", NOW, addr(), name, watch->average());
}

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_16SS::update_throughput (ThroughputWatch *watch, double size)
{
  char *name;

#ifdef USE_802_21 //Switch to activate when using 802.21 modules (external package)
  threshold_action_t action = watch->update (size, NOW);
  if (action != NO_ACTION_TH) {
    link_parameter_type_s param;
    union param_value old_value, new_value;
    if (watch == &rx_data_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_RX_DATA_THROUGHPUT;
    } else if (watch == &rx_traffic_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_RX_TRAFFIC_THROUGHPUT;
    } else if (watch == &tx_data_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_TX_DATA_THROUGHPUT;
    } else if (watch == &tx_traffic_watch_) {
      param.link_type = LINK_GENERIC;
      param.parameter_type = LINK_GEN_TX_TRAFFIC_THROUGHPUT;
    }
    old_value.data_d = watch->old_average();
    new_value.data_d = watch->average();

    send_link_parameters_report (addr(), getPeerNode_head()->getAddr(), param, old_value, new_value);      
  }
#else
  watch->update (size, NOW);
#endif 

  if (watch == &rx_data_watch_) {
    name = "rx_data";
    rx_data_timer_->resched (watch->get_timer_interval());
  } else if (watch == &rx_traffic_watch_) {
    rx_traffic_timer_->resched (watch->get_timer_interval());
    name = "rx_traffic";
  } else if (watch == &tx_data_watch_) {
    tx_data_timer_->resched (watch->get_timer_interval());
    name = "tx_data";
  } else if (watch == &tx_traffic_watch_) {
    tx_traffic_timer_->resched (watch->get_timer_interval());
    name = "tx_traffic";
  }

  //*if (print_stats_)
    //*debug2 ("At %f in Mac %d, updating stats %s: %f\n", NOW, addr(), name, watch->average());
}

/**
 * Start a new frame
 */
void Mac802_16SS::start_dlsubframe ()
{
  //*debug2 ("At %f in Mac %d SS scheduler dlsubframe expires %d\n", NOW, addr(), scan_info_->substate);
  r_delta_ss = 0;

  frame_number_++;

  switch (scan_info_->substate) {
  case SCAN_PENDING: 
    if (scan_info_->count == 0) {
      resume_scanning();
      return;
    } 
    scan_info_->count--;
    break;
  case HANDOVER_PENDING:
    if (scan_info_->handoff_timeout == 0) {
      assert (scan_info_->nbr);
#ifdef USE_802_21
      //debug ("At %f in Mac %d link handoff proceeding\n", NOW, addr());
      //send_link_handoff_proceeding (addr(), getPeerNode_head()->getAddr(), scan_info_->nbr->getID());
#endif 
      scan_info_->substate = HANDOVER;
      //restore previous state 
      //restore_state (scan_info_->nbr->getState()->state_info);
      setChannel (scan_info_->nbr->getState()->state_info->channel);
      lost_synch ();
      //add target as peer
      addPeerNode (new PeerNode(scan_info_->nbr->getID()));
      return;
    }
    scan_info_->handoff_timeout--;
    break;
  default:
    break;
  }
    
  //this is the begining of new frame
  map_->setStarttime (NOW);

  //start handler of dlsubframe
  map_->getDlSubframe()->getTimer()->sched (0);

  //reschedule for next frame
  dl_timer_->resched (getFrameDuration());
}

/**
 * Start a new frame
 */
void Mac802_16SS::start_ulsubframe ()
{
  //*debug ("At %f in Mac %d SS scheduler ulsubframe expires\n", NOW, addr());
  
  //change state of PHY: even though it should have been done before
  //there are some cases where it does not (during scanning)
  getPhy()->setMode (OFDM_SEND);

  scheduler_->schedule();

  //start handler for ulsubframe
  if (getMap()->getUlSubframe()->getNbPdu ()>0) {
    Burst *b = getMap()->getUlSubframe()->getPhyPdu (0)->getBurst (0);
    getMap()->getUlSubframe()->getTimer()->sched (b->getStarttime()*getPhy()->getSymbolTime());
  }//else there is no uplink phy pdu defined

  //reschedule for next frame
  ul_timer_->resched (getFrameDuration());     
 
}

/**
 * Called when lost synchronization
 */
void Mac802_16SS::lost_synch ()
{
#ifdef USE_802_21
  int poa = -1;
  bool disconnect = false;
#endif

  //reset timers
  if (t1timer_->busy()!=0)
    t1timer_->stop();
  if (t12timer_->busy()!=0)
    t12timer_->stop();
  if (t21timer_->busy()!=0)
    t21timer_->stop();
  if (lostDLMAPtimer_->busy()!=0)
    lostDLMAPtimer_->stop(); 
  if (lostULMAPtimer_->busy()!=0)
    lostULMAPtimer_->stop(); 
  if (t2timer_->busy()!=0)
    t2timer_->stop(); 
  if (t44timer_ && t44timer_->busy()!=0)
    t44timer_->stop();

  //we need to go to receiving mode
  //printf ("Set phy to recv %x\n", getPhy());
  getPhy()->setMode (OFDM_RECV);
  if (getMacState()==MAC802_16_CONNECTED) {
    //remove possible pending requests
    map_->getUlSubframe()->getBw_req()->removeRequests(); 

#ifdef USE_802_21
    poa = getPeerNode_head()->getAddr ();
    disconnect = true;
#endif
  }

  //remove information about peer node
  if (getPeerNode_head())
    removePeerNode (getPeerNode_head());

  //start waiting for DL synch
  setMacState (MAC802_16_WAIT_DL_SYNCH);
  t21timer_->start (macmib_.t21_timeout);
  if (dl_timer_->status()==TIMER_PENDING)
    dl_timer_->cancel();
  map_->getDlSubframe()->getTimer()->reset();
  if (ul_timer_->status()==TIMER_PENDING)
    ul_timer_->cancel();
  map_->getUlSubframe()->getTimer()->reset();

#ifdef USE_802_21
  if (disconnect) {
    //*debug ("At %f in Mac %d, send link down\n", NOW, addr());
    send_link_down (addr(), poa, LD_RC_FAIL_NORESOURCE);
  }
#endif

  if (scan_info_->substate == HANDOVER_PENDING || scan_info_->substate == SCAN_PENDING) {
    //we have lost synch before scanning/handover is complete
    for (int i=0 ; i < scan_info_->nb_rdv_timers ; i++) {
      //debug ("canceling rdv timer\n");
      if (scan_info_->rdv_timers[i]->busy()) {
	scan_info_->rdv_timers[i]->stop();
      }
      delete (scan_info_->rdv_timers[i]);
    }
    scan_info_->nb_rdv_timers = 0;
  }

  if (scan_info_->substate == HANDOVER_PENDING) {
    //*debug ("Lost synch during pending handover (%d)\n", scan_info_->handoff_timeout);
    //since we lost connection, let's execute handover immediately 
    scan_info_->substate = HANDOVER;
    setChannel (scan_info_->nbr->getState()->state_info->channel);
    //add target as peer
    addPeerNode (new PeerNode(scan_info_->nbr->getID()));
    return;
  } 
  if (scan_info_->substate == SCAN_PENDING) {
    debug ("Lost synch during pending scan (%d)\n", scan_info_->count);
    //we must cancel the scanning
    scan_info_->substate = NORMAL;
  }
}

/**
 * Start/Continue scanning
 */
void Mac802_16SS::resume_scanning ()
{
  //*if (scan_info_->iteration == 0) 
    //*debug ("At %f in Mac %d, starts scanning\n", NOW, addr());
  //*else 
    //*debug ("At %f in Mac %d, resume scanning\n", NOW, addr());
  
  scan_info_->substate = SCANNING;

  //backup current state
  scan_info_->normal_state.state_info = backup_state();
  if (t1timer_->busy())
    t1timer_->pause();
  scan_info_->normal_state.t1timer = t1timer_;
  if (t2timer_->busy())
    t2timer_->pause();
  scan_info_->normal_state.t2timer = t2timer_;
  if (t6timer_->busy())
    t6timer_->pause();
  scan_info_->normal_state.t6timer = t6timer_;
  if (t12timer_->busy())
    t12timer_->pause();
  scan_info_->normal_state.t12timer = t12timer_;
  if (t21timer_->busy())
    t21timer_->pause();
  scan_info_->normal_state.t21timer = t21timer_;
  if (lostDLMAPtimer_->busy())
    lostDLMAPtimer_->pause();
  scan_info_->normal_state.lostDLMAPtimer = lostDLMAPtimer_;
  if (lostULMAPtimer_->busy())
    lostULMAPtimer_->pause();
  scan_info_->normal_state.lostULMAPtimer = lostULMAPtimer_;
  scan_info_->normal_state.map = map_;

  if (scan_info_->iteration == 0) {
    //reset state
    t1timer_ = new WimaxT1Timer (this);
    t2timer_ = new WimaxT2Timer (this);
    t6timer_ = new WimaxT6Timer (this);
    t12timer_ = new WimaxT12Timer (this);
    t21timer_ = new WimaxT21Timer (this);
    lostDLMAPtimer_ = new WimaxLostDLMAPTimer (this);
    lostULMAPtimer_ = new WimaxLostULMAPTimer (this);
    
    map_ = new FrameMap (this);
    
    nextChannel();

    scan_info_->scn_timer_ = new WimaxScanIntervalTimer (this);

    //start waiting for DL synch
    setMacState (MAC802_16_WAIT_DL_SYNCH);
    t21timer_->start (macmib_.t21_timeout);
    if (dl_timer_->status()==TIMER_PENDING)
      dl_timer_->cancel();
    map_->getDlSubframe()->getTimer()->reset();
    if (ul_timer_->status()==TIMER_PENDING)
      ul_timer_->cancel();
    map_->getUlSubframe()->getTimer()->reset();
    

  }else{
    //restore where we left
    //restore previous timers
    restore_state(scan_info_->scan_state.state_info);
    t1timer_ = scan_info_->scan_state.t1timer;
    if (t1timer_->paused())
      t1timer_->resume();
    t2timer_ = scan_info_->scan_state.t2timer;
    if (t2timer_->paused())
      t2timer_->resume();
    t6timer_ = scan_info_->scan_state.t6timer;
    if (t6timer_->paused())
      t6timer_->resume();
    t12timer_ = scan_info_->scan_state.t12timer;
    if (t12timer_->paused())
      t12timer_->resume();
    t21timer_ = scan_info_->scan_state.t21timer;
    if (t21timer_->paused())
      t21timer_->resume();
    lostDLMAPtimer_ = scan_info_->scan_state.lostDLMAPtimer;
    if (lostDLMAPtimer_->paused())
      lostDLMAPtimer_->resume();
    lostULMAPtimer_ = scan_info_->scan_state.lostULMAPtimer;
    if (lostULMAPtimer_->paused())
      lostULMAPtimer_->resume();
    map_ = scan_info_->scan_state.map;
    
    getPhy()->setMode (OFDM_RECV);

    if (ul_timer_->status()==TIMER_PENDING)
      ul_timer_->cancel();
  }
  setNotify_upper (false);
  //printf ("Scan duration=%d, frameduration=%f\n", scan_info_->rsp->scan_duration, getFrameDuration());
  scan_info_->scn_timer_->start (scan_info_->rsp->scan_duration*getFrameDuration());
  scan_info_->iteration++;
  
}

/**
 * Pause scanning
 */
void Mac802_16SS::pause_scanning ()
{
  //*if (scan_info_->iteration < scan_info_->rsp->scan_iteration)
    //*debug ("At %f in Mac %d, pause scanning\n", NOW, addr());
  //*else 
    //*debug ("At %f in Mac %d, stop scanning\n", NOW, addr());

  //return to normal mode
  if (scan_info_->iteration < scan_info_->rsp->scan_iteration) {
    //backup current state
    scan_info_->scan_state.state_info = backup_state();
    if (t1timer_->busy())
      t1timer_->pause();
    scan_info_->scan_state.t1timer = t1timer_;
    if (t2timer_->busy())
      t2timer_->pause();
    scan_info_->scan_state.t2timer = t2timer_;
    if (t6timer_->busy())
      t6timer_->pause();
    scan_info_->scan_state.t6timer = t6timer_;
    if (t12timer_->busy())
      t12timer_->pause();
    scan_info_->scan_state.t12timer = t12timer_;
    if (t21timer_->busy())
      t21timer_->pause();
    scan_info_->scan_state.t21timer = t21timer_;
    if (lostDLMAPtimer_->busy())
      lostDLMAPtimer_->pause();
    scan_info_->scan_state.lostDLMAPtimer = lostDLMAPtimer_;
    if (lostULMAPtimer_->busy())
      lostULMAPtimer_->pause();
    scan_info_->scan_state.lostULMAPtimer = lostULMAPtimer_;
    scan_info_->scan_state.map = map_;

    scan_info_->count = scan_info_->rsp->interleaving_interval;

  } else {
    //else scanning is over, no need to save data
    //reset timers
    if (t1timer_->busy()!=0)
      t1timer_->stop();
    delete (t1timer_);
    if (t12timer_->busy()!=0)
      t12timer_->stop();
    delete (t12timer_);
    if (t21timer_->busy()!=0)
      t21timer_->stop();
    delete (t21timer_);
    if (lostDLMAPtimer_->busy()!=0)
      lostDLMAPtimer_->stop(); 
    delete (lostDLMAPtimer_);
    if (lostULMAPtimer_->busy()!=0)
      lostULMAPtimer_->stop(); 
    delete (lostULMAPtimer_);
    if (t2timer_->busy()!=0)
      t2timer_->stop(); 
    delete (t2timer_);
  }
  //restore previous timers
  restore_state(scan_info_->normal_state.state_info);
  t1timer_ = scan_info_->normal_state.t1timer;
  if (t1timer_->paused())
    t1timer_->resume();
  t2timer_ = scan_info_->normal_state.t2timer;
  if (t2timer_->paused())
    t2timer_->resume();
  t6timer_ = scan_info_->normal_state.t6timer;
  if (t6timer_->paused())
    t6timer_->resume();
  t12timer_ = scan_info_->normal_state.t12timer;
  if (t12timer_->paused())
    t12timer_->resume();
  t21timer_ = scan_info_->normal_state.t21timer;
  if (t21timer_->paused())
    t21timer_->resume();
  lostDLMAPtimer_ = scan_info_->normal_state.lostDLMAPtimer;
  if (lostDLMAPtimer_->paused())
    lostDLMAPtimer_->resume();
  lostULMAPtimer_ = scan_info_->normal_state.lostULMAPtimer;
  if (lostULMAPtimer_->paused())
    lostULMAPtimer_->resume();
  map_ = scan_info_->normal_state.map;

  setNotify_upper (true);
  dl_timer_->resched (0);

  if (scan_info_->iteration == scan_info_->rsp->scan_iteration) {
    scan_info_->substate = NORMAL;
    
    /** here we check if there is a better BS **/
#ifdef USE_802_21
    if(mih_){
      int nbDetected = 0;
      for (int i = 0 ; i < nbr_db_->getNbNeighbor() ; i++) {
	if (nbr_db_->getNeighbors()[i]->isDetected()) {
	  nbDetected++;
	}
      }
      int *listOfPoa = new int[nbDetected];
      int itr = 0;
      for (int i = 0 ; i < nbr_db_->getNbNeighbor() ; i++) {
	WimaxNeighborEntry *entry = nbr_db_->getNeighbors()[i];
	if (entry->isDetected()) {
	  listOfPoa[itr] = entry->getID();
	  itr++;
	}  
      }  
      send_scan_result (listOfPoa, itr*sizeof(int));	
    }else
#endif
      {
	send_msho_req();
      }

    setScanFlag(false);
    scan_info_->count--; //to avoid restarting scanning
  } else {
    scan_info_->substate = SCAN_PENDING;
  }
}

/**
 * Set the scan flag to true/false
 * param flag The value for the scan flag
 */
void Mac802_16SS::setScanFlag(bool flag)
{
  scan_flag_ = flag;
}

/**
 * return scan flag
 * @return the scan flag
 */
bool Mac802_16SS::isScanRunning()
{
  return scan_flag_;
}


/**
 * rpi - get initial random channel for propagation model
 */
int  Mac802_16SS::GetInitialChannel()
{
  int Rand_Num=0;
  Rand_Num = ((int)(rand() % 500)/*NUM_REALIZATIONS*/+ 1);
  return Rand_Num; 
}



void Mac802_16SS::addPowerinfo(hdr_mac802_16 *wimaxHdr,double power_per_subchannel, bool collision )
{

  int num_data_subcarrier = getPhy()->getNumSubcarrier (DL_);
  int num_subchannel = getPhy()->getNumsubchannels (DL_);
  int num_symbol_per_slot = getPhy()->getSlotLength (DL_); 

  if (collision == true)

    if(wimaxHdr->phy_info.num_OFDMSymbol % num_symbol_per_slot == 0)   // chk this condition , chk whether broacastcid reqd ir not. 
      // if((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) > 30)
      //if(wimaxHdr->phy_info.num_OFDMSymbol >= num_symbol_per_slot)
      {
	if(wimaxHdr->phy_info.num_OFDMSymbol > num_symbol_per_slot) 
	  {
            //numsymbols = (int)  ceil((((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) - 30) % 30));
            //numsymbols *= num_symbol_per_slot;  
	    // for the first num_symbol_per_slot symbols 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j<=num_subchannel ; j++ )
                (basepower_[i][j] = power_per_subchannel);
	    // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+num_symbol_per_slot ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot) ; i++)
	      for(int j = 1 ; j<=num_subchannel ; j++ )
                (basepower_[i][j] = power_per_subchannel);  

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i< wimaxHdr->phy_info.OFDMSymbol_offset + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = 1 ; j<=((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel) ; j++ )
		(basepower_[i][j] = power_per_subchannel); 
          }
	else 
          {

	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
              for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j< (wimaxHdr->phy_info.subchannel_offset) + wimaxHdr->phy_info.num_subchannels ; j++ )
                (basepower_[i][j] = power_per_subchannel); 	    

          }

      }

    else

      {
	//numsymbols = (int)  ceil((((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) - 30) % 30));
	//numsymbols *= num_symbol_per_slot;  
        // for the first num_symbol_per_slot symbols 
	if(wimaxHdr->phy_info.num_OFDMSymbol > 1) 
	  {
            for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
              for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j<=num_subchannel ; j++ )
		(basepower_[i][j] = power_per_subchannel); 
	    // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
            for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+1 ; i<  ((wimaxHdr->phy_info.OFDMSymbol_offset) + wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
	      for(int j = 1 ; j<=num_subchannel ; j++ )
		basepower_[i][j] = power_per_subchannel;

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-1 ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = 1 ; j<=((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel) ; j++ )
		(basepower_[i][j] = power_per_subchannel); 

          }
	else
          {
          
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j< wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels ; j++ )
		(basepower_[i][j] = power_per_subchannel); 	

          }
             
      }

  else
       
    if(wimaxHdr->phy_info.num_OFDMSymbol % num_symbol_per_slot == 0)   // chk this condition , chk whether broacastcid reqd ir not. 
      // if((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) > num_subchannel)
      //if(wimaxHdr->phy_info.num_OFDMSymbol >= num_symbol_per_slot)
      {
	if(wimaxHdr->phy_info.num_OFDMSymbol > num_symbol_per_slot) 
	  {
            //numsymbols = (int)  ceil((((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) - num_subchannel) % num_subchannel));
            //numsymbols *= num_symbol_per_slot;  
	    // for the first num_symbol_per_slot symbols 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
                (intpower_[i][j] = power_per_subchannel);
	    // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+num_symbol_per_slot ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot) ; i++)
	      for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
                (intpower_[i][j] = power_per_subchannel);  

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i< wimaxHdr->phy_info.OFDMSymbol_offset + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = 0 ; j<(((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel))*num_data_subcarrier ; j++ )
		(intpower_[i][j] = power_per_subchannel); 
          }
	else 
          {

	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
              for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier) + wimaxHdr->phy_info.num_subchannels*num_data_subcarrier) ; j++ )
                (intpower_[i][j] = power_per_subchannel); 	    

          }

      }

    else

      {
	//numsymbols = (int)  ceil((((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) - num_subchannel) % num_subchannel));
	//numsymbols *= num_symbol_per_slot;  
        // for the first num_symbol_per_slot symbols 
	if(wimaxHdr->phy_info.num_OFDMSymbol > 1) 
	  {
            for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
              for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
		(intpower_[i][j] = power_per_subchannel); 
	    // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
            for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+1 ; i<  ((wimaxHdr->phy_info.OFDMSymbol_offset) + wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
	      for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
		intpower_[i][j] = power_per_subchannel;

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-1 ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = 0 ; j<(((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel))*num_data_subcarrier ; j++ )
		(intpower_[i][j] = power_per_subchannel); 

          }
	else
          {
          
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier) + wimaxHdr->phy_info.num_subchannels*num_data_subcarrier) ; j++ )
		(intpower_[i][j] = power_per_subchannel); 	

          }
             
      }

}



bool Mac802_16SS::IsCollision (const hdr_mac802_16 *wimaxHdr,double power_subchannel)
{
  bool collision = FALSE; 
  int num_subchannel = getPhy()->getNumsubchannels (DL_); 
  int num_symbol_per_slot = getPhy()->getSlotLength (DL_); 

  if(wimaxHdr->phy_info.num_OFDMSymbol % num_symbol_per_slot == 0 )   // chk this condition , chk whether broacastcid reqd ir not. 
    // if((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) > num_subchannel)
    //if(wimaxHdr->phy_info.num_OFDMSymbol >= num_symbol_per_slot)
    {
      if(wimaxHdr->phy_info.num_OFDMSymbol > num_symbol_per_slot) 
	{
	  //numsymbols = (int)  ceil((((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) - num_subchannel) % num_subchannel));
	  //numsymbols *= num_symbol_per_slot;  
	  // for the first num_symbol_per_slot symbols 
          for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
            {
              for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j<=num_subchannel ; j++ )
		if(basepower_[i][j] != power_subchannel)
		  {
                    collision = TRUE;
		    //*debug2(" collision is true : i = %d j =%d basepower[i][j] = %.25f power_subchannel =%.25f \n" , i, j , basepower_[i][j], power_subchannel);			
		    return collision;
		  }
              //if(collision == TRUE) return collision;  
	    }
	  // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
          for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+num_symbol_per_slot ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot) ; i++)
            {
              for(int j = 1 ; j<=num_subchannel ; j++ )
		if(basepower_[i][j] != power_subchannel)
		  {
                    collision = TRUE;
		    //*debug2(" collision is true : i = %d j =%d basepower[i][j] = %.25f power_subchannel =%.25f \n" , i, j , basepower_[i][j], power_subchannel);		
		    return collision;
		  } 
	      //              if(collision == TRUE) return collision; 
            } 
	  // last num_symbol_per_slot 
          for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i<  wimaxHdr->phy_info.OFDMSymbol_offset + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)        
            {
	      for(int j = 1 ; j<=((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel) ; j++ )
		if(basepower_[i][j] != power_subchannel)
		  {
                    collision = TRUE;
		    //*debug2(" collision is true : i = %d j =%d basepower[i][j] = %.25f power_subchannel =%.25f \n" , i, j , basepower_[i][j], power_subchannel);		
		    return collision ;
		  }
	      //              if(collision == TRUE) return collision;    
            }
	}
      else 
	{

	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
            {
	      for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j< (wimaxHdr->phy_info.subchannel_offset) + wimaxHdr->phy_info.num_subchannels ; j++ )
                if(basepower_[i][j] != power_subchannel)
		  {
                    collision = TRUE;
		    //*debug2(" collision is true : i = %d j =%d basepower[i][j] = %.25f power_subchannel =%.25f \n" , i, j , basepower_[i][j], power_subchannel);		
		    return collision;
		  }    
	      //            if(collision == TRUE) return collision;  
	    }  

	}

    }

  else

    {
      //numsymbols = (int)  ceil((((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) - num_subchannel) % num_subchannel));
      //numsymbols *= num_symbol_per_slot;  
      // for the first num_symbol_per_slot symbols 
      if(wimaxHdr->phy_info.num_OFDMSymbol > 1) 
	{
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	    {
	      for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j<=num_subchannel ; j++ )
		if(basepower_[i][j] != power_subchannel)
		  {
                    collision = TRUE;
		    //*debug2(" collision is true : i = %d j =%d basepower[i][j] = %.25f power_subchannel =%.25f \n" , i, j , basepower_[i][j], power_subchannel);		
		    return collision;
		  } 
	      //	      if(collision == TRUE) return collision; 
	    }	
	  // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+1 ; i<  ((wimaxHdr->phy_info.OFDMSymbol_offset) + wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
	    {
	      for(int j = 1 ; j<=num_subchannel ; j++ )
		if(basepower_[i][j] != power_subchannel)
		  {
                    collision = TRUE;
		    //*debug2(" collision is true : i = %d j =%d basepower[i][j] = %.25f power_subchannel =%.25f \n" , i, j , basepower_[i][j], power_subchannel);		
		    return collision;
		  }  
	      //		 if(collision == TRUE) return collision; 
	    }
	  // last num_symbol_per_slot 
          for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-1 ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	    {
	      for(int j = 1 ; j<=((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel) ; j++ )
		if(basepower_[i][j] != power_subchannel)
		  {
                    collision = TRUE;
		    //*debug2(" collision is true : i = %d j =%d basepower[i][j] = %.25f power_subchannel =%.25f \n" , i, j , basepower_[i][j], power_subchannel);		
		    return collision;
		  }
	      //               if(collision == TRUE) return collision; 
            }
	}
      else
	{
          
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	    {
	      for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j< wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels ; j++ )
		if(basepower_[i][j] != power_subchannel)
		  {
                    collision = TRUE;
		    //*debug2(" collision is true : i = %d j =%d basepower[i][j] = %.25f power_subchannel =%.25f \n" , i, j , basepower_[i][j], power_subchannel);		
		    return collision;
		  } 	
	      //                if(collision == TRUE) return collision;  
	    }

	}
             
    }

  return collision; 

}

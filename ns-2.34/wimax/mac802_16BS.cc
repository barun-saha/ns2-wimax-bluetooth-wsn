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
#include "mac802_16BS.h"
#include "scheduling/bsscheduler.h"
#include "destclassifier.h"
#include "cmu-trace.h"
#include "globalparams_wimax.h"
#include <rtp.h>

/**
 * TCL Hooks for the simulator for wimax mac
 */
static class Mac802_16BSClass : public TclClass {
public:
  Mac802_16BSClass() : TclClass("Mac/802_16/BS") {}
  TclObject* create(int, const char*const*) {
    return (new Mac802_16BS());
    
  }
} class_mac802_16BS;

/**
 * Creates a Mac 802.16
 */
Mac802_16BS::Mac802_16BS() : Mac802_16 (), cl_head_(0), cl_tail_(0), ctrlagent_(0)
{
  type_ = STA_BS; //type of MAC. In this case it is for SS
  
  //Default configuration
  addClassifier (new DestClassifier ());
  scheduler_ = new BSScheduler();
  scheduler_->setMac (this); //register the mac

  init_default_connections ();

  LIST_INIT (&t17_head_);
  LIST_INIT (&scan_stations_);
  LIST_INIT (&fast_ranging_head_);
  bw_peer_ = NULL;
  bw_node_index_ = 0;
  sendDCD = false;
  dlccc_ = 0;
  sendUCD = false;
  ulccc_ = 0;
  reg_SS_number = 0;

  map_ = new FrameMap (this);

  //create timers for DL/UL boundaries
  dl_timer_ = new DlTimer (this);
  ul_timer_ = new UlTimer (this);
}

/*
 * Interface with the TCL script
 * @param argc The number of parameter
 * @param argv The list of parameters
 */
int Mac802_16BS::command(int argc, const char*const* argv)
{
  return Mac802_16::command(argc, argv);
}

/**
 * Initialize default connections
 */
void Mac802_16BS::init_default_connections ()
{
  Connection * con;

  //create initial ranging and padding connection
  con = new Connection (CONN_INIT_RANGING);
  //  con->initCDMA();
  //  con->setCDMA(0);
  connectionManager_->add_connection (con, IN_CONNECTION); //uplink
  con = new Connection (CONN_INIT_RANGING);
  //  con->initCDMA();
  //  con->setCDMA(0);
  connectionManager_->add_connection (con, OUT_CONNECTION); //downlink
  con = new Connection (CONN_PADDING);
  //  con->initCDMA();
  //  con->setCDMA(0);
  connectionManager_->add_connection (con, IN_CONNECTION);
  con = new Connection (CONN_PADDING);
  //  con->initCDMA();
  //  con->setCDMA(0);
  connectionManager_->add_connection (con, OUT_CONNECTION);

  //we need to create a Broadcast connection and AAS init ranging CIDs
  con = new Connection (CONN_BROADCAST);
  //  con->initCDMA();
  //  con->setCDMA(0);
  connectionManager_->add_connection (con, OUT_CONNECTION);
  con = new Connection (CONN_AAS_INIT_RANGING);
  //  con->initCDMA();
  //  con->setCDMA(0);
  connectionManager_->add_connection (con, IN_CONNECTION);
}

/**
 * Initialize the MAC
 */
void Mac802_16BS::init ()
{
  //schedule the first frame by using a random backoff to avoid
  //synchronization between BSs.
  
  double stime = getFrameDuration () + Random::uniform(0, getFrameDuration ());
  dl_timer_->sched (stime);

  //also start the DCD and UCD timer
  dcdtimer_ = new WimaxDCDTimer (this);
  ucdtimer_ = new WimaxUCDTimer (this);
  nbradvtimer_ = new WimaxMobNbrAdvTimer (this);
  dcdtimer_->start (macmib_.dcd_interval);
  ucdtimer_->start (macmib_.ucd_interval);
  nbradvtimer_->start (macmib_.nbr_adv_interval+stime);

  int nbPS = (int) floor((getFrameDuration()/getPhy()->getPS()));
  int nbPS_left = nbPS - phymib_.rtg - phymib_.ttg;
  int nbSymbols = (int) floor((getPhy()->getPS()*nbPS_left)/getPhy()->getSymbolTime());  // max num of OFDM symbols available per frame. 
  int nbSubcarrier = getPhy()->getFFT();
  int nbSubchannel = getPhy()->getNumsubchannels (UL_);

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
 * Set the control agent
 */
void Mac802_16BS::setCtrlAgent (WimaxCtrlAgent *agent)
{
  assert (agent);
  ctrlagent_ = agent;
}

/**** Packet processing methods ****/

/*
 * Process packets going out
 * @param p The packet to send out
 */
void Mac802_16BS::sendDown(Packet *p)
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
    //debug ("At %f in Mac %d drop packet because no classification were found\n", \
	   NOW, index_);
    drop(p, "CID");
    //Packet::free (p);
  } else {
    //enqueue the packet 
    Connection *connection = connectionManager_->get_connection (cid, OUT_CONNECTION);
    if (connection == NULL) {
      //debug ("Warning: At %f in Mac %d connection with cid = %d does not exist. Please check classifiers\n",\
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
					//debug2 ("At %f (BS) in Mac %d, SENDDOWN, Enqueue packet to cid :%d queue size :%d (max :%d)\n", NOW, index_, cid,connection->queueLength (), macmib_.queue_length);
					//Begin RPI
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
						connection ->enqueue (p->copy());
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
					connection ->enqueue (p);
					//debug2 ("ARQ BS Dividing Packet into Blocks Packet Size=%d, Number of packets = %d, ARQ_BLOCK_SIZE = %d,Last Packet Size= %d\n",packet_size,number_of_packs+1,arq_block_size_, HDR_CMN(p)->size()) ;
				}
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
void Mac802_16BS::transmit(Packet *p)
{
  /*  if (NOW < last_tx_time_+last_tx_duration_) {
  //still sending
  debug2 ("At %f (BS) in Mac %d is already transmitting (endTX=%.9f). Drop packet.\n", NOW, addr(),last_tx_time_+last_tx_duration_);
  Packet::free (p);
  return;
  }*/   // rpi - removed because it wa not allowing multiple packets to be transmitted at the same time. 

  struct hdr_cmn *ch = HDR_CMN(p);
  int if_cdma_packet = 0;
  
  //debug ("At %f (BS) in Mac %d, sending packet, TRANSMIT, (type :%s, size :%d, txtime :%.9f)\n", NOW, index_, packet_info.name(ch->ptype()), ch->size(), ch->txtime());
  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(p)->header.ht == 0) {
        // Barun if((mac802_16_dl_map_frame*) p->accessdata() != NULL)
          //debug (" generic mngt :%d\n", ((mac802_16_dl_map_frame*) p->accessdata())->type);
    } else {
      hdr_mac802_16 *wimaxHdr_tmp = HDR_MAC802_16(p);
      gen_mac_header_t header_tmp = wimaxHdr_tmp->header;

      cdma_req_header_t *req_tmp;
      req_tmp = (cdma_req_header_t *)&header_tmp;

      if (req_tmp->type == 0x3) {
        //debug (" cdma_bw_req code :%d, top :%d, ssid :%d\n", req_tmp->code, req_tmp->top, req_tmp->br);
	if_cdma_packet = 1;
      } else if (req_tmp->type == 0x2) {
        //debug (" cdma_inti_req code :%d, top :%d, ssid :%d\n", req_tmp->code, req_tmp->top, req_tmp->br);
	if_cdma_packet = 1;
      } else {
        //debug (" bwreq\n");
      }
    }
  } else {
    //debug10 (" unknown => %s\n",  packet_info.name(ch->ptype()));
  }

  //update stats for delay and jitter
  double delay = NOW-ch->timestamp();
  if (if_cdma_packet == 0) update_watch (&delay_watch_, delay);
  double jitter = fabs (delay - last_tx_delay_);
  if (if_cdma_packet == 0) update_watch (&jitter_watch_, jitter);
  last_tx_delay_ = delay;
  if (ch->ptype()!=PT_MAC) {
    update_throughput (&tx_data_watch_, 8*ch->size());
  } 
  if (if_cdma_packet == 0) update_throughput (&tx_traffic_watch_, 8*ch->size());
  
  last_tx_time_ = NOW;
  last_tx_duration_ = ch->txtime();
  //pass it down
  downtarget_->recv (p, (Handler*)NULL);
}

/*
 * Process incoming packets
 * @param p The incoming packet
 */
void Mac802_16BS::sendUp (Packet *p)
{
  struct hdr_cmn *ch = HDR_CMN(p);
  hdr_mac802_16 *wimaxHdr;

 
#ifdef DEBUG_WIMAX
  //debug10 ("At %f (BS) in Mac %d, SENDUP, receive first bit..over at :%f(txtime :%f) (type :%s, size :%d), cid :%d\n", NOW, index_, NOW+ch->txtime(),ch->txtime(), packet_info.name(ch->ptype()), ch->size(), HDR_MAC802_16(p)->header.cid);
  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(p)->header.ht == 0) {
        // Barun if((mac802_16_dl_map_frame*) p->accessdata() != NULL)
          //debug (" generic mngt :%d\n", ((mac802_16_dl_map_frame*) p->accessdata())->type);
    } else {
      hdr_mac802_16 *wimaxHdr_tmp = HDR_MAC802_16(p);
      gen_mac_header_t header_tmp = wimaxHdr_tmp->header;

      cdma_req_header_t *req_tmp;
      req_tmp = (cdma_req_header_t *)&header_tmp;
      // Commented by Barun : 23-Sep-2011
      /*
      if (req_tmp->type == 0x3) {
        debug (" cdma_bw_req code :%d, top :%d, ssid :%d\n", req_tmp->code, req_tmp->top, req_tmp->br);
      } else if (req_tmp->type == 0x2) {
        debug (" cdma_init_req code :%d, top :%d, ssid :%d\n", req_tmp->code, req_tmp->top, req_tmp->br);
      } else {
        debug (" bwreq\n");
      }
      */
    }
  } else {
    //debug10 (" unknown => %s\n",  packet_info.name(ch->ptype()));

  }
#endif

  //create a timer to wait for the end of reception
  //since the packets are received by burst, the beginning of the new packet 
  //is the same time as the end of this packet..we process this packet 1ns 
  //earlier to make room for the new packet.
  wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;
  int cid = header.cid;
  Connection *con = connectionManager_->get_connection (cid, IN_CONNECTION);
  gen_mac_header_t header_tmp = wimaxHdr->header;
  cdma_req_header_t *req_tmp;
  req_tmp = (cdma_req_header_t *)&header_tmp;

  if (req_tmp->type == 0x3) {
      //debug10 (" BS, SENDUP, received by pass cdma_bw_req code :%d, top :%d, ssid :%d\n", req_tmp->code, req_tmp->top, req_tmp->br);
      addPacket(p);
      return;
  } else if (req_tmp->type == 0x2) {
      //debug10 (" BS, SENDUP, received by pass cdma_init_req code :%d, top :%d, ssid :%d\n", req_tmp->code, req_tmp->top, req_tmp->br);
      addPacket(p);
      return;
  }

  if( (wimaxHdr->phy_info.OFDMSymbol_offset == 0 && wimaxHdr->phy_info.num_OFDMSymbol == 0) || wimaxHdr->header.cid == BROADCAST_CID)   // this kind of packets are treated diff(OFDM types) basically it a bw req packet.
    {
      if (con == NULL) {
	//This packet is not for us
	//debug2 ("At %f in Mac %d Connection null\n", NOW, index_);
	update_watch (&loss_watch_, 1);
 
	Packet::free(p);
	//debug2(" packet getting dropped header.ht :%d",HDR_MAC802_16(p)->header.ht);
	//pktRx_=NULL;
	return;
      } 
      
      if(contPktRxing_ == TRUE)
	{
	  // if there is a collision and the power of the currently recvd packet is less by factor 10 thn discard it. 
           
	  if (head_pkt_ !=NULL) {
            
	    if(head_pkt_->p->txinfo_.RxPr / p->txinfo_.RxPr >= p->txinfo_.CPThresh) {
	      //Chakchai  => disable collission
	      //	      Packet::free(p);
	    }
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
  
	  //           drop (p, "COL");
	  //update drop stat
	  //           debug2(" packet dropped due to collision"); 
	  //           update_watch (&loss_watch_, 1);
	  //           collision_ = TRUE;
	  //p = NULL;
	  //           return;
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
     // debug2 ("At %f in Mac %d Connection null -- packet getting dropped at SS not for us \n", NOW, index_);
      //wimaxHdr->phy_info.num_subchannels;
      //wimaxHdr->phy_info.subchannel_offset;
      //wimaxHdr->phy_info.num_OFDMSymbol;
      //wimaxHdr->phy_info.OFDMSymbol_offset; 
      update_watch (&loss_watch_, 1);
      //intpower_ +=  p->txinfo_.RxPr;
      //for (int i= 0; i++; i<1024)
      //intpower_[i] += p->txinfo_.RxPr_OFDMA[i]; 
    
      // remeber to take care of collision, tht is if a packet destined to this is already being recvd and another packets destined to this comes thn thr will be collision    

      Packet::free(p);  //removed jut to chk
      //pktRx_=NULL;
      return;
    }  // free and end if con == NULL 
    
    // collision check .  rpi

    bool collision = FALSE; 

    //collision = IsCollision ( wimaxHdr, 0.0);  removed replace it later

    if (collision == TRUE) {
      collision = FALSE; 
      hdr_cmn *hdr = HDR_CMN(p);
      hdr->error() = 1;   // we do not discard this packet but just mark it and r=transmit coz, this packet and the other collided packet might not have symbols and carrriers exactly same, but they can collide, we shld also keep track of this packets transmission, because other packets overlapping with this can collide. 
      
      // set to the symbol and corresponding subcarriers over which this packet was transmitted to -1, so tht for future packets we can check for collision and also after the other collided packet is received we can chk for collision.    
      //collision_ = true; 
      addPowerinfo(wimaxHdr, -1.0,true);  // enable , diabled just to compile // will be in the base power 
      
      // set end
      
    } else {
      addPowerinfo(wimaxHdr, BASE_POWER,true ); // enable , disabled just to compile 
    }
  
    addPowerinfo(wimaxHdr, 0.0,false);   // enable, disabled just to compile  // separated the variabled for interference and collision detection. 

    // collision check rpi 

    addPacket(p);    // adding the collided packet also , which will be dropped in the receive function, this will help for collision detection of packets which might over lap with this.

  } // ends else for bwreq  replace this } later when testing over 

}

/**
 * Process the fully received packet
 */
void Mac802_16BS::receive (Packet *pktRx_)
{
	assert (pktRx_);
	struct hdr_cmn *ch = HDR_CMN(pktRx_);
	hdr_mac802_16 *wimaxHdr;
	wimaxHdr = HDR_MAC802_16(pktRx_);

	//cdma_flag => 1 (bw-req), 2 (initial-ranging-req)
	int cdma_flag = 0;


  //debug10 ("At %f (BS) in Mac %d, RECEIVE, packet received (type :%s)\n", NOW, index_, packet_info.name(ch->ptype()));
  // Commented by Barun : 22-Sep-2011
  //debug10 (" phyinfo header - symbol offset :%d, numsymbol :%d\n", wimaxHdr->phy_info.OFDMSymbol_offset,wimaxHdr->phy_info.num_OFDMSymbol);
  if (ch->ptype()==PT_MAC) {
    if (HDR_MAC802_16(pktRx_)->header.ht == 0) {
        // Barun if((mac802_16_dl_map_frame*) pktRx_->accessdata() != NULL)
          //debug10 (" generic mngt :%d\n", ((mac802_16_dl_map_frame*) pktRx_->accessdata())->type);
    } else {
      hdr_mac802_16 *wimaxHdr_tmp = HDR_MAC802_16(pktRx_);
      gen_mac_header_t header_tmp = wimaxHdr_tmp->header;
      cdma_req_header_t *req_tmp;
      req_tmp = (cdma_req_header_t *)&header_tmp;

      //int cid = header_tmp.cid;
      int cid = req_tmp->cid;
//      debug2 ("CON1 :%d\n", cid);
      Connection *con_tmp = connectionManager_->get_connection (cid, IN_CONNECTION);
//      debug2 ("CON1 :%d\n", con_tmp->get_cid());

      if (req_tmp->type == 0x3 && con_tmp!=NULL) {
        //debug (" cdma_bw_req code :%d, top :%d, ssid :%d\n", req_tmp->code, req_tmp->top, req_tmp->br);
        cdma_flag = 1;
        //debug10 ("At %f Connection %d, old :%d, set to 1\n", NOW, con_tmp->get_cid(), con_tmp->getCDMA());

        con_tmp->setCDMA(1);
        con_tmp->setCDMA_code(req_tmp->code);
        con_tmp->setCDMA_top(req_tmp->top);
        Packet::free(pktRx_);
        return;
      } else if (req_tmp->type == 0x2 && con_tmp!=NULL) {
        //debug (" cdma_init_req code :%d, top :%d, ssid :%d\n", req_tmp->code, req_tmp->top, req_tmp->br);
        cdma_flag = 2;
        //debug10 ("At %f Connection %d, old :%d, set to 2\n", NOW, con_tmp->get_cid(), con_tmp->getCDMA());

        con_tmp->setCDMA(2);
        con_tmp->setCDMA_SSID_FLAG(req_tmp->br, 2);
        con_tmp->setCDMA_SSID_SSID(req_tmp->br, req_tmp->br);
        con_tmp->setCDMA_SSID_CODE(req_tmp->br, req_tmp->code);
        con_tmp->setCDMA_SSID_TOP(req_tmp->br, req_tmp->top);
        con_tmp->setCDMA_SSID_CID(req_tmp->br, con_tmp->get_cid());
        con_tmp->setCDMA_code(0);
        con_tmp->setCDMA_top(0);
        reg_SS_number++;
        Packet::free(pktRx_);
        return;
      } else {
        debug (" bwreq\n");
      }
    }

  } else {
    //debug10 (" unknown => %s\n",  packet_info.name(ch->ptype()));
  }

  // commenting here to chk without collision start 

  if( (wimaxHdr->phy_info.OFDMSymbol_offset == 0 && wimaxHdr->phy_info.num_OFDMSymbol == 0) || wimaxHdr->header.cid == BROADCAST_CID)   // this kind of packets are treated diff(OFDM types) basically it a bw req packet.
    {
      if (ch->error())
	{
          if (collision_) {
	    //debug2 ("\t drop new pktRx..collision\n");
	    drop (pktRx_, "COL");
	    collision_ = false;
	  } 
          else {
            //debug2("\t error in the packet, the Mac does not process\n");
      	    Packet::free(pktRx_);
	  }

	  //update drop stat
	  update_watch (&loss_watch_, 1);
	  contPktRxing_ = false;
	  pktRx_ = NULL;
    	  return;
	}          
      //else
      contPktRxing_ = false; 
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
      //debug2("error in the packet, the Mac does not process");
      //  addPowerinfo(wimaxHdr, 0.0,false);  // when we drop or discard a packet we have to reinitialise the intpower array to 0.0 , which shows tht there is no packet being recvd over the symbols and subcarriers.   // interference initialisation not reqd cause it is initialised whenever a packet is recvd
      Packet::free(pktRx_);
    
      //update drop stat
      update_watch (&loss_watch_, 1);
      pktRx_ = NULL;
      return;
    }

    // chk here for collision of the packet which was received before the packet that caused actual collision, so we have to chk whether collision had occur on this packet or not. , and the collsion was caused  

    bool collision = false; 

    //collision = IsCollision(wimaxHdr, BASE_POWER);  // removed -replace it later

    if (collision == true)
      {
	collision = false; 
	addPowerinfo(wimaxHdr, 0.0,true);  
	drop (pktRx_, "COL"); 
	//update drop stat
	update_watch (&loss_watch_, 1);
	pktRx_ = NULL;
	//debug2("Drop this packet because of collision.\n");
	return;
      }

    addPowerinfo(wimaxHdr, 0.0,true); 

    //commenting here to chk without collision  ends

    // chk collision ends

    //   SINR calcualtions

    //removed for testing without Rxinpwr in packet header

    int total_subcarriers=0; 
    //int num_data_subcarrier = getPhy()->getNumDataSubcarrier (UL_);         
    int num_data_subcarrier = getPhy()->getNumSubcarrier (UL_);         
    int num_subchannel      = getPhy()->getNumsubchannels (UL_); 
    int num_symbol_per_slot = getPhy()->getSlotLength (UL_); 

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
	    // for the first num_symbol_per_slot symbols 
	    if(wimaxHdr->phy_info.num_OFDMSymbol > 1) 
	      {
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
		  //   for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers += num_subchannel*num_data_subcarrier - (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ;
		  }
		// except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
		for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+1 ; i< (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
		  //for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
		  {
		    total_subcarriers +=  num_subchannel*num_data_subcarrier ;
		  }  

		// last num_symbol_per_slot 
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
    else
      total_subcarriers = wimaxHdr->phy_info.num_subchannels * num_data_subcarrier ;  

    // total subcarrier calculation ends 

    //*debug2(" total_subcarriers = %d \n", total_subcarriers); 

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

	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; 
		i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)

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
	      for(int j = 0 ; j<((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier; j++ )
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
	    SINR[counter] =  signalpower[k]/interferencepower[counter];
	    counter++;
	  }
      }
    //*debug2("in MAC BS, [%d] subcarriers signal power are calculated.\n ", counter);
   

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

    num_of_slots = (int) ceil((double)ch->size() / (double)phy->getSlotCapacity(wimaxHdr->phy_info.modulation_, UL_));

    max_block_size=phy->getMaxBlockSize(wimaxHdr->phy_info.modulation_);   // get max  block size in number of slots

    last_block_size = num_of_slots % max_block_size; 

    num_of_complete_block = (int) floor(num_of_slots / max_block_size);

    int ITU_PDP = getITU_PDP (); 

    bool pkt_error = FALSE; 

    //*debug2(" num of complete blocks = %d , ITU_PDP = %d, num of slots = %d, max block size = %d, last block size = %d \n" , num_of_complete_block, ITU_PDP, num_of_slots, max_block_size, last_block_size ); 

    //max_block_size = 2;     // testing for results. remove it.
    //last_block_size = 2;

    if (num_of_complete_block == 0) { 
      //num_of_complete_block =1;
      index =  phy->getMCSIndex( wimaxHdr->phy_info.modulation_ ,  last_block_size);
      
      beta = global->Beta[ITU_PDP][index]; 
      //debug2(" beta = %.2f = \n index = %d\n" , beta,index ); 

      num_subcarrier_block = num_symbol_per_slot * getPhy()->getNumSubcarrier (UL_) * last_block_size; 

      /* if(num_subcarrier_block > total_subcarriers) num_subcarrier_block = total_subcarriers; 
	 for( int i = 0; i< num_subcarrier_block ; i++) 
	 {    
	 debug2( " SINR (%d) = %g , eesm_sum = %g %g\n", i, SINR[i], eesm_sum, exp(-(SINR[i]/beta)));    
	 eesm_sum = eesm_sum + exp( -(SINR[i]/beta));
	 }*/

      //printf("\n n [%d] --- counter [%d]\n",n, counter);
      for(int i=0; i<counter; i++)
	{
	  eesm_sum = eesm_sum + exp( -(SINR[i]/beta));		
	}

	
      if (num_subcarrier_block == 0) {
	fprintf(stderr, "ERROR: num_subcarrier_block = 0\n");
	exit (1);
      }
      if (eesm_sum >= BASE_POWER) {
	SIR =  (-beta) * log(eesm_sum/counter);
	SIR=10*log10(SIR);
	BLER = global->TableLookup(index, SIR);
      } else {
	//eesm = 0 when SINR is large (MS close to BS) and exp returns 0
	BLER = 0;
      }
      //debug2(" BLER-BS one block = %.6f = \n" , BLER ); 

      int rand_num = ((rand() % 100) +1 ) ; 
      // debug2(" random num = %d = \n" , rand_num ); 
      rand1 = rand_num/100.00; 

     //if (!phymib_.disableInterference && BLER > rand1) 
      if (!phymib_.disableInterference && BLER > 0.96) 
	pkt_error = TRUE;
      
      // getBLER(wimaxHdr->phy_info.modulation_, SINR, last_block_size);
    }
    else {

      // First the complete blocks are checked fro error 

      index =  phy->getMCSIndex( wimaxHdr->phy_info.modulation_ ,  max_block_size);
      
      beta = global->Beta[ITU_PDP][index];  

      //debug2(" beta = %.2f index = %d \n" , beta,index ); 
 
      num_subcarrier_block = num_symbol_per_slot * getPhy()->getNumSubcarrier (UL_) * max_block_size; 
	

      if(num_subcarrier_block > total_subcarriers) num_subcarrier_block = total_subcarriers; 

      /* for( int i = 0; i< num_subcarrier_block ; i++)     
	 eesm_sum = eesm_sum + exp( -(SINR[i]/beta));*/

      for(int i=0; i<counter; i++)
	{
	  eesm_sum = eesm_sum + exp( -(SINR[i]/beta));		
	}


      if (num_subcarrier_block == 0) {
	fprintf(stderr, "ERROR: num_subcarrier_block = 0\n");
	exit (1);
      }
      if (eesm_sum >=BASE_POWER) {
	//SIR =  (-beta) * log( eesm_sum/(num_subcarrier_block) );
	SIR =  (-beta) * log(eesm_sum/counter);
	SIR=10*log10(SIR);
	//debug2(" SIR-BS = %.6f = \n" , SIR );
	BLER = global->TableLookup(index, SIR);
      } else {
	//eesm = 0 when SINR is large (MS close to BS) and exp returns 0
	BLER = 0;
      } 
      //debug2(" BLER-BS complete blocks = %.6f = \n" , BLER ); 
 
      int rand_num = ((rand() % 100) +1 ) ; 

      //debug2(" random num = %d = \n" , rand_num ); 
 
      rand1 = rand_num/100.00; 

       //if (!phymib_.disableInterference && BLER > rand1) 
      if (!phymib_.disableInterference && BLER > 0.96) 
	pkt_error = TRUE; 

      // Chk if thr is any last block, compute whether it is in error or not. 
      if(last_block_size > 0)
	{
	  eesm_sum=0;
	  index =  phy->getMCSIndex( wimaxHdr->phy_info.modulation_ ,  last_block_size);
      
	  beta = global->Beta[ITU_PDP][index];  
 
	  num_subcarrier_block = num_symbol_per_slot * getPhy()->getNumSubcarrier (UL_) * last_block_size; 

	  if(num_subcarrier_block > total_subcarriers) num_subcarrier_block = total_subcarriers; 


	  /*	  for( int i = 0; i< num_subcarrier_block ; i++)     
	    eesm_sum = eesm_sum + exp( -(SINR[i]/beta));*/

	  for(int i=0; i<counter; i++)
	    {
	      eesm_sum = eesm_sum + exp( -(SINR[i]/beta));		
	    }


	  if (num_subcarrier_block == 0) {
	    fprintf(stderr, "ERROR: num_subcarrier_block = 0\n");
	    exit (1);
	  }
	  if (eesm_sum >=BASE_POWER) {
	    //SIR =  (-beta) * log( eesm_sum/(num_subcarrier_block) );
	    SIR =  (-beta) * log(eesm_sum/counter);
	    SIR=10*log10(SIR);
	    //debug2(" SIR-BS = %.6f = \n" , SIR );
	    BLER = global->TableLookup(index, SIR);
	  } else {
	    //eesm = 0 when SINR is large (MS close to BS) and exp returns 0
	    BLER = 0;
	  } 

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


 
    //removed for testing without Rxinpwr in packet header  

    //BLER calcualtion ends



  } //bwreq packet treatment else ends here.  replace { when testing ends 


  addPowerinfo(wimaxHdr, 0.0,true); 
  // normal packet processing starts. 

  //debug2 ("normal packet processing\n");

  gen_mac_header_t header = wimaxHdr->header;
  int cid = header.cid;
  Connection *con = connectionManager_->get_connection (cid, IN_CONNECTION);
  if(con==NULL)
  	return;

  //update rx time of last packet received
  PeerNode *peer;
  if (type_ == STA_MN)
    peer = getPeerNode_head(); //MN only has one peer
  else
    peer = con->getPeerNode(); //BS can have multiple peers

  if (peer) {
    peer->setRxTime (NOW);
    
    //collect receive signal strength stats
    if (pktRx_->txinfo_.RxPr*1e3 == 0) {
      fprintf(stderr, "ERROR: pktRx_->txinfo_.RxPr*1e3 = 0\n");
    }
    peer->getStatWatch()->update(10*log10(pktRx_->txinfo_.RxPr*1e3));
    //debug ("At %f in Mac %d weighted RXThresh: %e rxp average %e\n", NOW, index_, macmib_.lgd_factor_*macmib_.RXThreshold_, pow(10,peer->getStatWatch()->average()/10)/1e3);
    double avg_w = pow(10,(peer->getStatWatch()->average()/10))/1e3;
    
    if ( avg_w < (macmib_.lgd_factor_*macmib_.RXThreshold_)) {
#ifdef USE_802_21
      double probability = ((macmib_.lgd_factor_*macmib_.RXThreshold_)-avg_w)/((macmib_.lgd_factor_*macmib_.RXThreshold_)-macmib_.RXThreshold_);
      Mac::send_link_going_down (peer->getAddr(), addr(), -1, (int)(100*probability), LGD_RC_LINK_PARAM_DEGRADING, eventId_++);
#endif
      if (peer->getPrimary(IN_CONNECTION)!=NULL) { //check if node registered
	peer->setGoingDown (true);      
      }
    }
    else {
      if (peer->isGoingDown()) {
#ifdef USE_802_21
	Mac::send_link_rollback (addr(), getPeerNode_head()->getAddr(), eventId_-1);
#endif
	peer->setGoingDown (false);
      }
    }
  }
 
  //this flag is used to update fragmentaion for piggybacking scenario
  int flag_pig = 0;

  //Process GM subheader (piggybacking)
  int bw_update_dueto_gm = 0;
  int more_bw = 0;
  int old_bw = 0;
  if (wimaxHdr->header.type_fbgm) {
    more_bw = wimaxHdr->grant_subheader.piggyback_req;
    old_bw = con->getBw();
    flag_pig = 2;
    //*debug10 ("BS: Piggybacking, At %f Connection %d, flag :%d, old_bw :%d, add_bw :%d\n", NOW, con->get_cid(), wimaxHdr->header.type_fbgm, con->getBw(), more_bw);
    if (more_bw > old_bw) {
      bw_update_dueto_gm = more_bw;
      con->setBw(bw_update_dueto_gm);
    }

    //*debug10 ("    Final bw_update con->getBw :%d\n",con->getBw());

  } else {
    bw_update_dueto_gm = 0;
  }
  //End piggybacking process

  //Begin RPI 
  // New function for reassembly will be implemented for ARQ, Fragmentation and Packing
  if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1 
      && con->isFragEnable () == true && con->isPackingEnable () == true 
      && HDR_CMN(pktRx_)->ptype()!=PT_MAC){
    process_mac_pdu_witharqfragpack(con, pktRx_);
    update_throughput (&rx_data_watch_, 8*ch->size()); 
    update_throughput (&rx_traffic_watch_, 8*ch->size());
    return;		
  }	  
  //End RPI 
  
  //process reassembly
  //debug2 ("Reading fragmentation flags: %d (%d) for CID %d\n", wimaxHdr->header.type_frag, wimaxHdr->header.type_frag & 0x1, cid); 

  if (wimaxHdr->header.type_frag) {
    bool drop_pkt = true;
    bool frag_error = false;
    //debug2 ("\tfrag type = %d (type noflag :%d, first :%d, cont :%d, last :%d)\n", wimaxHdr->frag_subheader.fc & 0x3, FRAG_NOFRAG, FRAG_FIRST,FRAG_CONT, FRAG_LAST);
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
      //debug2 ("\tReceived first fragment fsn :%d cid :%d", wimaxHdr->frag_subheader.fsn&0x7, cid);
      if (con->getFragmentationStatus()!=FRAG_NOFRAG) {
	//debug2 ("...Error: expecting CONT or LAST\n");
      } else {
	//debug2 ("...Ok\n");
      }

      if (wimaxHdr->header.type_fbgm) {
        con->updateFragmentation (FRAG_FIRST, 0, ch->size()-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE+flag_pig));
      } else {
        con->updateFragmentation (FRAG_FIRST, 0, ch->size()-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
      }

      break; 
    case FRAG_CONT: 
      //debug2 ("\tReceived cont fragment fsn :%d cid :%d", wimaxHdr->frag_subheader.fsn&0x7, cid);
      if ( (con->getFragmentationStatus()!=FRAG_FIRST
	    && con->getFragmentationStatus()!=FRAG_CONT)
	   || ((wimaxHdr->frag_subheader.fsn&0x7) != (con->getFragmentNumber ()+1)%8) ) {
	frag_error = true;
	//debug2 ("...Error: status :%d, fsn :%d, expected fsn :%d\n", 
		//con->getFragmentationStatus(), wimaxHdr->frag_subheader.fsn&0x7, (con->getFragmentNumber ()+1)%8);
	con->updateFragmentation (FRAG_NOFRAG, 0, 0); //reset
      } else {
	//*debug2 ("...Ok\n");
        if (wimaxHdr->header.type_fbgm) {
	  con->updateFragmentation (FRAG_CONT, wimaxHdr->frag_subheader.fsn&0x7, con->getFragmentBytes()+ch->size()-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE+flag_pig));	
	} else {
	  con->updateFragmentation (FRAG_CONT, wimaxHdr->frag_subheader.fsn&0x7, con->getFragmentBytes()+ch->size()-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));	
	}
      }
      break; 
    case FRAG_LAST: 
      //debug2 ("\tReceived last fragment fsn :%d cid :%d", wimaxHdr->frag_subheader.fsn&0x7, cid);
      if ( (con->getFragmentationStatus()==FRAG_FIRST || con->getFragmentationStatus()==FRAG_CONT)
	   && ((wimaxHdr->frag_subheader.fsn&0x7) == (con->getFragmentNumber ()+1)%8) ) {
	ch->size() += con->getFragmentBytes()-HDR_MAC802_16_FRAGSUB_SIZE;
	drop_pkt = false;
	//debug2 ("...Ok\n");
      } else {
	//debug2 ("...Error: status :%d (nofrag :%d, first :%d, cont :%d, last :%d), fsn :%d, expected fsn :%d\n", 
		//con->getFragmentationStatus(), FRAG_NOFRAG, FRAG_FIRST, FRAG_CONT, FRAG_LAST ,wimaxHdr->frag_subheader.fsn&0x7, (con->getFragmentNumber ()+1)%8);
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
	//debug10 ("Drop packets FRG error\n");
	drop (pktRx_, "FRG"); //fragmentation error
      } else {
	//silently discard this fragment.
	Packet::free(pktRx_);
      }
      pktRx_=NULL;
      return;
    } 
  }

  //Check if this is a bandwidth request
  hdr_mac802_16 *wimaxHdr_tmp = HDR_MAC802_16(pktRx_);
  gen_mac_header_t header_tmp = wimaxHdr_tmp->header;

  bw_req_header_t *req_tmp;
  req_tmp = (bw_req_header_t *)&header_tmp;

  if (header.ht == 0) {
    if ( (req_tmp->type == 0x3) || (req_tmp->type == 0x2) ) {
      //*debug10 ("Impossible; PANIC\n");

#ifndef BWREQ_PATCH
    } else {
      if (con->getBw() > 0) {
	//debug10 ("setBW left-over At %f Connection %d, old=%d, rcv=%d, left=%d\n", NOW, con->get_cid(), con->getBw(), ch->size(), con->getBw()-ch->size());
	if (con->getBw()-ch->size() >0) {
	  con->setBw(con->getBw()-ch->size());
	} else {
	  con->setBw(0);
	}
      }
#endif
    }
  }
  //end checking bw-req packet

  /* => Old
  //check if this is a bandwidth request
  if (header.ht == 0 && con->getBw() >0) {
  debug2 ("At %f Connection %d, old=%d, rcv=%d, left=%d\n", NOW, con->get_cid(), con->getBw(), ch->size(), con->getBw()-ch->size());
  con->setBw(con->getBw()-ch->size());
  }
  */

  //We check if it is a MAC packet or not
  if (HDR_CMN(pktRx_)->ptype()==PT_MAC) {
    process_mac_packet (con, pktRx_);
    update_throughput (&rx_traffic_watch_, 8*ch->size());
    mac_log(pktRx_);
    Packet::free(pktRx_);
  }
  else {
    update_throughput (&rx_data_watch_, 8*ch->size());    
    update_throughput (&rx_traffic_watch_, 8*ch->size());
    ch->size() -= HDR_MAC802_16_SIZE;
    uptarget_->recv(pktRx_, (Handler*) 0);
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
void Mac802_16BS::process_mac_pdu_witharqfragpack (Connection *con, Packet * pkt)
{
  Packet *mac_pdu, *p_current, *p, *mac_sdu, *p_previous, *p_temp;
  hdr_cmn *ch_pdu, *ch_current, *ch, *ch_sdu;
  hdr_mac802_16 *wimaxHdr_pdu, *wimaxHdr_current, *wimaxHdr, *wimaxHdr_sdu;
  double tmp;
  u_int8_t isInOrder;
  u_int32_t seqno;
  bool mac_sdu_gen = false;
  int pdu_size = 0;	
  
  //debug2("ARQ BS: Entering the process_mac_pdu_witharqfragpack function \n");	 
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
				if(connection->getArqStatus () && connection->getArqStatus ()->isArqEnabled () == 1)
				{
					//debug2("ARQ : BS Feedback in Data Payload Received: Has a feedback: Value of i:%d , Value of number of acks:%d \n" , i, wimaxHdr_pdu->num_of_acks);
					connection->getArqStatus ()->arqRecvFeedback(mac_pdu, i, connection);
				}
			}  
		}
		ch_pdu->size() -= (wimaxHdr_pdu->num_of_acks * HDR_MAC802_16_ARQFEEDBK_SIZE);	
	}
  		
	//Add loss on data connection
	tmp = Random::uniform(0, 1);
	if (tmp < data_loss_) 
	{
		//debug2 ("ARQ: BS process_mac_pdu_witharqfragpack %f Drop data loss-- packet size [%d] %f %f\n", NOW,ch_pdu->size(), tmp, data_loss_);
		update_watch(&loss_watch_,1); 
		drop (pkt, DROP_MAC_PACKET_ERROR);
		pktRx_ = NULL;    
		return;
	} 
	else 
	{
		//debug2 ("ARQ: BS process_mac_pdu_witharqfragpack %f No drop data loss %f %f\n", NOW, tmp, data_loss_);
	}
  
	// We need to change the size of the first packet to its original size as in transmission logic 
	ch_pdu->size() -= HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE; /*Size of generic Mac Header was set (Note one packet header and first ARQ Block is present)*/

	if(wimaxHdr_pdu->header.ci)
		ch_pdu->size() -= HDR_MAC802_16_CRC_SIZE;	

	// Initial Condition
	p_previous = mac_pdu;	
	p_temp = (Packet*) mac_pdu->accessdata();

 	/*Maybe there are a lot of ARQ blocks in this incoming packet, but they belong to different MAC SDU.
 	   We first update the size of first ARQ block. and use a loop to remove all the overheads.*/
 	//debug2("ARQ BS: before update, the ch_pdu size: %d \n",ch_pdu->size()); 
	while(p_temp)
	{
		// Update the Mac PDU Size
		//debug2("ARQ:BS p-previous :%p p-temp : %p \n", p_previous, p_temp);	
		//debug2 ("ARQ : BS type of ARQ to update size:  %d, %d\n ",HDR_MAC802_16(p_temp)->pack_subheader.fc, (((HDR_MAC802_16(p_temp)->pack_subheader.sn) - (HDR_MAC802_16(p_previous)->pack_subheader.sn)) &0x7FF));	

		if((HDR_MAC802_16(p_temp)->pack_subheader.fc == FRAG_FIRST) || ((((HDR_MAC802_16(p_temp)->pack_subheader.sn) - (HDR_MAC802_16(p_previous)->pack_subheader.sn)) &0x7FF) != 1) || (HDR_MAC802_16(p_temp)->pack_subheader.fc == FRAG_NOFRAG))
			ch_pdu->size() -= HDR_CMN(p_temp)->size()+HDR_MAC802_16_FRAGSUB_SIZE ;  	
		else
			ch_pdu->size() -= HDR_CMN(p_temp)->size();

		//Initial conditions
		p_previous = p_temp;
		p_temp = (Packet*) p_temp->accessdata();
	}	
	//debug2("ARQ BS: Finally the ch_pdu size: %d \n",ch_pdu->size()); 

	/*Distribute all the ARQ blocks in this MAC SDU to transmission queue and re-transmission queue.
	   according to if they are in-order or not.*/
	if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) 
	{
		isInOrder = 1;
		con->getArqStatus ()->arqReceive(mac_pdu,con,&isInOrder);
		//*debug2("Receive: ARQ BS :ORDER is:%d\n",isInOrder);
		if (isInOrder == 0) /*In middle part*/
		{
			// packet is out of order, so queue it in retransmission queue at receiver end since it is not being used
			debug2("Receive: ARQ BS : Out of order packet, buffering in retransmission queue\n");
			con->getArqStatus ()->arq_retrans_queue_->enque(mac_pdu->copy());
		}
		else if (isInOrder == 1)  /*in order*/
		{
			// packet is in order, so queue it in transmission queue at receiver end since it is not being used
			//*debug2("Receive: ARQ BS : In order packet, buffering in arq transmission queue\n");
			con->getArqStatus ()->arq_trans_queue_->enque(mac_pdu->copy());
		}
		else /*outside of ARQ window.*/
		{
			//*debug2("Receive: ARQ BS : Not buffering in arq trans/retrans queue\n");
		}	 
	}
	
   /*for the second and following ARQ blocks.  Distribution go ahead.*/
	p_current = (Packet*) mac_pdu->accessdata();
  
	while(p_current)
	{
		wimaxHdr_current= HDR_MAC802_16(p_current);
		ch_current = HDR_CMN(p_current);

		//Time to perform ARQ for the new packet
		if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) 
		{
			isInOrder = 1;
			con->getArqStatus ()->arqReceive(p_current,con,&isInOrder);
	               //*debug2("Receive: ARQ BS cid=%d:ORDER is:%d\n",con->get_cid(),isInOrder);
			if (isInOrder == 0) 
			{
				// packet is out of order, so queue it in retransmission queue at receiver end since it is not being used
                                debug2("Receive: ARQ BS cid=%d: Out of order packet, buffering in retransmission queue\n",con->get_cid());
				con->getArqStatus ()->arq_retrans_queue_->enque(p_current->copy());
			}
			else if(isInOrder == 1) 
			{
				// packet is in order, so queue it in transmission queue at receiver end since it is not being used
	                       //*debug2("Receive: ARQ BS cid=%d: In order packet, buffering in arq transmission queue\n",con->get_cid());
				con->getArqStatus ()->arq_trans_queue_->enque(p_current->copy());
			}
			else 
			{
				//*debug2("Receive: ARQ BS :  Not buffering in arq trans/retrans queue\n"); 	
			} 
		}
		p_current = (Packet*) p_current->accessdata();
	}
 
	// Here we will check if packets in retransmission queue can be fit into transmission queue now that a lost packet may have been retransmitted
	primary:
	con->getArqStatus ()->arq_retrans_queue_->resetIterator();
   
	while(con->getArqStatus ()->arq_retrans_queue_)
	{
		secondary:
		p = con->getArqStatus ()->arq_retrans_queue_->getNext ();

		/*There are no packets in ARQ re-transmission queue.*/
		if(!p)
		{
	               //*debug2("ARQ BS cid=%d: Retransmission queue is empty- so nothing to fit or packets are there but of no use\n",con->get_cid());
			break;
		}  		 

		//Time to perform ARQ for the new packet
		if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) 
		{
			isInOrder = 1;
			con->getArqStatus ()->arqReceiveBufferTransfer(p,con,&isInOrder);
	               //*debug2("ARQ BS cid=%d: isInOrder=%d\n", con->get_cid(), isInOrder);
			if(isInOrder == 2) /*Just outside of ARQ window. We simply remove it since it is of no use.*/
			{
				con->getArqStatus ()->arq_retrans_queue_->remove(p);
				//*debug2("ARQ BS remove out-order ARQ blocks.\n");
				Packet::free(p);
				goto secondary;		
			}
			else if (isInOrder == 0) 
			{
				/*we need to find the expected block. So the other will stay in re-transmission queue. Wait for the next search.*/
				goto secondary;
			}
			else  /*In order.*/
			{
				con->getArqStatus ()->arq_trans_queue_->enque(p->copy());
				con->getArqStatus ()->arq_retrans_queue_->remove(p);
				Packet::free(p);	

				while(con->getArqStatus ()->arq_retrans_queue_)
				{
					p = con->getArqStatus ()->arq_retrans_queue_->getNext ();

					if(!p)
					{
		                                //*debug2("ARQ BS cid=%d: Retransmission2 queue is empty- so nothing to fit or packets are there but of no use\n",con->get_cid());
						break;
					}   		
					
					//Time to perform ARQ for the new packet
					if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) 
					{
						isInOrder = 1;
						con->getArqStatus ()->arqReceiveBufferTransfer(p,con,&isInOrder);
		                                //*debug2("ARQ BS cid=%d: isInOrder2=%d\n", con->get_cid(), isInOrder);
						if(isInOrder == 2)
						{
							con->getArqStatus ()->arq_retrans_queue_->remove(p);
							Packet::free(p);
							goto primary; 		
						}	
						
						if (isInOrder == 0)
						{
							goto primary;
						}
						else /*in order.*/
						{
							con->getArqStatus ()->arq_trans_queue_->enque(p->copy());
							con->getArqStatus ()->arq_retrans_queue_->remove(p);
							Packet::free(p);	
						}
					}
				}
				goto primary;
				//break;	
			}
		}		    	
	}

   
	//The advantage of seperating the packets in transmission and retransmission queue is the all the packets in transmission queue will be in order.
	while(1) 
	{
		con->getArqStatus ()->arq_trans_queue_->resetIterator();
		p = con->getArqStatus ()->arq_trans_queue_->getNext ();

		if(!p) 
		{
			//*debug2("ARQ BS: Transmission Queue is Empty \n ");	
			break;
		}	
		wimaxHdr= HDR_MAC802_16(p);
		ch = HDR_CMN(p);

		// This will be first part of the MAC SDU
		if(wimaxHdr->pack_subheader.fc == FRAG_FIRST) 
		{
			mac_sdu = p->copy();
			wimaxHdr_sdu= HDR_MAC802_16(mac_sdu);
			ch_sdu = HDR_CMN(mac_sdu);
			seqno = wimaxHdr_sdu->pack_subheader.sn; 
			//*debug2("ARQ BS: This is the first packet in transmission queue CID:[%d] and FRAG_FIRST Fsn: %d Size :%d\n",
								//*wimaxHdr->header.cid, wimaxHdr_sdu->pack_subheader.sn, ch_sdu->size());

			while(1) 
			{
				p =  con->getArqStatus ()->arq_trans_queue_->getNext ();

				if(!p)
				{
					//*debug2("ARQ BS: Except for the first Block, all other ARQ Blocks for this MAC SDU are not in ARQ transmission queue. \n");
					break;
				}
				wimaxHdr= HDR_MAC802_16(p);
				ch = HDR_CMN(p);

				
				if((wimaxHdr->pack_subheader.fc == FRAG_CONT) && (((wimaxHdr->pack_subheader.sn - seqno) &0x7FF)==1)) 
				{
					//*debug2("ARQ:BS Generating the MAC SDU CID %d Current SN: %d Previous SN: %d Frag Type: %d Size: %d, %d \t Trans-Q length is [%d] Re-trans-Q length is %d \n ",
									//*con->get_cid(), wimaxHdr->pack_subheader.sn, seqno, wimaxHdr->pack_subheader.fc, ch->size(), ch_sdu->size(), con->getArqStatus ()->arq_trans_queue_->length(), con->getArqStatus ()->arq_retrans_queue_->length()); 	
					ch_sdu->size() += ch->size();
					seqno++;	
				}
				else if(wimaxHdr->pack_subheader.fc == FRAG_LAST && (((wimaxHdr->pack_subheader.sn - seqno) &0x7FF)==1)) 
				{
					//*debug2("ARQ: BS Generating the MAC SDU CID %d Current SN: %d Previous SN: %d Frag Type: %d Size: %d, %d \t Trans-Q length is [%d] Re-trans-Q length is %d \n ",
									//*con->get_cid(), wimaxHdr->pack_subheader.sn, seqno, wimaxHdr->pack_subheader.fc, ch->size(), ch_sdu->size(), con->getArqStatus ()->arq_trans_queue_->length(), con->getArqStatus ()->arq_retrans_queue_->length());
					ch_sdu->size() += ch->size();
					mac_sdu_gen = true; 
					break;		
				}
				else 
				{
					//*debug2("ARQ BS:This is a unsual case FSN: %d  and Frag_Type: %d \n",wimaxHdr->pack_subheader.sn, wimaxHdr->pack_subheader.fc);		 
					break;
				}		
			}
			
			if(mac_sdu_gen == true)
			{
				
				//*debug2("ARQ BS: Created the Mac SDU SIZE: %d\n ", ch_sdu->size());
				//update_throughput (&rx_data_watch_, 8*pdu_size);    
				//update_throughput (&rx_traffic_watch_, 8*pdu_size);
				debug2("send to upper layer, Mac SDU size [%d]  time[%f] for MAC [%d] Last block FSN is [%d]\n",ch_sdu->size(), NOW, addr(),seqno);
				ch_sdu->direction() = hdr_cmn::UP; 
				uptarget_->recv(mac_sdu, (Handler*) 0);
				mac_sdu_gen = false;
				// Go ahead and delete all the packets pertaining to this MAC SDU from the ARQ transmission queue
				do 
				{
					p_temp =con->getArqStatus ()->arq_trans_queue_->deque();
					Packet::free(p_temp);
	                                 //*debug2("ARQ BS cid=%d: To keep a count on how many times Dequeue function is called \n",con->get_cid());
					p = con->getArqStatus ()->arq_trans_queue_->head(); /*Enqueue new block at the tail. Need to move the first ARQ block to continue.*/
					if(!p)
					{
						goto end;
					}
					wimaxHdr= HDR_MAC802_16(p);
					ch = HDR_CMN(p);
					//*debug2("ARQ BS: Next Packet to Dequeue FSN: %d FRAG_STATUS: %d \n",wimaxHdr->pack_subheader.sn, wimaxHdr->pack_subheader.fc);
				} while(wimaxHdr->pack_subheader.fc != FRAG_FIRST);
			//*debug2("ARQ:BS Generating the MAC SDU CID %d Trans-Q length is [%d] Re-trans-Q length is [%d] \n ",
							 //*con->get_cid() ,con->getArqStatus ()->arq_trans_queue_->length(), con->getArqStatus ()->arq_retrans_queue_->length()); 	

			}
			else  // Either no packets in trans queue or all the mac sdu that could be generated are sent above
			{
				//*debug2("ARQ BS: Either no packets in trans queue or all the mac sdu that could be generated are sent above \n");
				goto end; 
			}
		}
		else if(wimaxHdr->pack_subheader.fc == FRAG_NOFRAG) // This will be the only part of the MAC SDU
		{
			mac_sdu = p->copy();
			wimaxHdr_sdu= HDR_MAC802_16(mac_sdu);
			ch_sdu = HDR_CMN(mac_sdu);
			seqno = wimaxHdr_sdu->pack_subheader.sn; 
                       //*debug2("ARQ BS cid=%d: This is the only fragment  of MAC SDU FRAG_NOFRAG Fsn: %d Size :%d\n",con->get_cid(),wimaxHdr_sdu->pack_subheader.sn, ch_sdu->size());
                         //*debug2("ARQ BS cid=%d: Created the Mac SDU SIZE: %d\n ",con->get_cid(), ch_sdu->size());
			
			//update_throughput (&rx_data_watch_, 8*pdu_size);    
			//update_throughput (&rx_traffic_watch_, 8*pdu_size);
                          ch_sdu->direction() == hdr_cmn::UP;        
			uptarget_->recv(mac_sdu, (Handler*) 0);
			mac_sdu_gen = false;
			// Go ahead and delete all the packets pertaining to this MAC SDU from the transmission queue
			p_temp =con->getArqStatus ()->arq_trans_queue_->deque();
			Packet::free(p_temp);
                       //*debug2("ARQ BS cid=%d: To keep a count on how many times Dequeue function is called \n",con->get_cid());
		}  	 
    else {
      //*debug2("ARQ BS  cid=%d: Something really wrong with the implementation\n",con->get_cid());
      abort();
    }			 	
  }

	end:
#ifdef WIMAX_DEBUG
	//*printf ("ARQ recv cid=%d: TxQueue=%d\n",con->get_cid(), con->getArqStatus ()->arq_trans_queue_->length());
	con->getArqStatus ()->arq_trans_queue_->resetIterator();
	p = con->getArqStatus ()->arq_trans_queue_->getNext ();
	while (p) 
	{
		wimaxHdr= HDR_MAC802_16(p);
		//*printf ("\tARQ cid=%d Block sn=%d\n",con->get_cid(), wimaxHdr->pack_subheader.sn);
		p = con->getArqStatus ()->arq_trans_queue_->getNext ();
	}
	//*printf ("ARQ cid=%d recv: ReTxQueue=%d\n",con->get_cid(), con->getArqStatus ()->arq_retrans_queue_->length());
	con->getArqStatus ()->arq_retrans_queue_->resetIterator();
	p = con->getArqStatus ()->arq_retrans_queue_->getNext ();
	
	while (p) 
	{
		wimaxHdr= HDR_MAC802_16(p);
		//*printf ("\tARQ cid=%d Block sn=%d\n",con->get_cid(), wimaxHdr->pack_subheader.sn);
		p = con->getArqStatus ()->arq_retrans_queue_->getNext ();
	}
#endif

	Packet::free(pkt);	
	update_watch(&loss_watch_,0);
	pktRx_=NULL; 
}
//End RPI

/**
 * Process a packet received by the Mac. Only scheduling related packets should be sent here (BW request, UL_MAP...)
 * @param con The connection by which it arrived
 * @param p The packet to process
 */
void Mac802_16BS::process_mac_packet (Connection *con, Packet * p)
{
  //debug2 ("Mac802_16BS received packet to process\n");

  assert (HDR_CMN(p)->ptype()==PT_MAC);
  //*debug10 ("Mac802_16BS received packet to process\n");
  
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;

  bw_req_header_t *req;
  req = (bw_req_header_t *)&header;

  //check if this is a bandwidth request
  
  if (header.ht == 1) {
    if ( (req->type == 0x3) || (req->type == 0x2) ) {
      process_cdma_req (p);
      return;
    } else {
      process_bw_req (p);
      return;
    }
  }


  /*
  //check if this is a bandwidth request
  if (header.ht == 1) {
  process_bw_req (p);
  return;
  }
  */

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
		    //*debug2("ARQ:BS Feedback Payload Received: Has a feedback: Value of i:%d , Value of number of acks:%d \n" , i, wimaxHdr->num_of_acks);
		    connection->getArqStatus ()->arqRecvFeedback(p, i, connection);
		  }
		}  
	    }
	  Packet::free(p);
	  return ;
	}
    }
  //End RPI

  //we cast to this frame because all management frame start with
  //a type 
  mac802_16_dl_map_frame *frame = (mac802_16_dl_map_frame*) p->accessdata();
  
  switch (frame->type) {
  case MAC_RNG_REQ: 
    process_ranging_req (p);
    break;
  case MAC_REG_REQ:     
    process_reg_req (p);
    break;
  case MAC_MOB_SCN_REQ:
    if (ctrlagent_) 
      ctrlagent_->process_scan_request (p);
    else
      fprintf (stderr, "Warning: no controler to handle scan request in BS %d\n", addr());
    break;
  case MAC_MOB_MSHO_REQ:
    process_msho_req (p);
    break;
  case MAC_MOB_HO_IND:
    process_ho_ind (p);
    break;
  case MAC_DSA_REQ: 
  case MAC_DSA_RSP: 
  case MAC_DSA_ACK: 
    serviceFlowHandler_->process (p); // rpi changed pktRx_ to p , cause pktRx_ is not in scope. 
    break;
  default:
    //*debug (" unknown packet in BS\n");
    ;
  }

  //Packet::free (p);
}

/**
 * Process a RNG-REQ message
 * @param p The packet containing the ranging request information
 */
void Mac802_16BS::process_ranging_req (Packet *p)
{
  UlSubFrame *ulsubframe = getMap()->getUlSubframe();
  mac802_16_rng_req_frame *req = (mac802_16_rng_req_frame *) p->accessdata();

  //*debug ("BS received ranging request from :%d\n", HDR_MAC802_16(p)->header.cid);

  if (HDR_MAC802_16(p)->header.cid != INITIAL_RANGING_CID) {
    //process request for DIUC
  } else {
    //here we can make decision to accept the SS or not.
    //for now, accept everybody
    //check if CID already assigned for the SS
    PeerNode *peer = getPeerNode (req->ss_mac_address);
    if (peer==NULL) {
      //debug ("New peer node requesting ranging ssid :%d\n",req->ss_mac_address);
      //Assign Management CIDs
      Connection *basic = new Connection (CONN_BASIC);
      Connection *upbasic = new Connection (CONN_BASIC, basic->get_cid());
      Connection *primary = new Connection (CONN_PRIMARY);
      Connection *upprimary = new Connection (CONN_PRIMARY, primary->get_cid());
      
      //Create Peer information
      peer = new PeerNode (req->ss_mac_address);
      peer->setBasic (upbasic, basic);
      peer->setPrimary (upprimary, primary);
      addPeerNode (peer);
      getCManager()->add_connection (upbasic, IN_CONNECTION);
      getCManager()->add_connection (basic, OUT_CONNECTION);
      getCManager()->add_connection (upprimary, IN_CONNECTION);
      getCManager()->add_connection (primary, OUT_CONNECTION);
      peer->setDIUC (req->req_dl_burst_profile);

	  /*Xingting changes it. Old code does not work in case of CDMA ranging.*/	  
      peer->setchannel(HDR_MAC802_16(p)->phy_info.channel_index);
      //peer->setchannel(req->channel_num);
      //schedule timer in case the node never register
      addtimer17 (req->ss_mac_address);

      //create packet for answers
      Packet *rep = getPacket ();
      struct hdr_cmn *ch = HDR_CMN(rep);
      rep->allocdata (sizeof (struct mac802_16_rng_rsp_frame));
      mac802_16_rng_rsp_frame *frame = (mac802_16_rng_rsp_frame*) rep->accessdata();
      frame->type = MAC_RNG_RSP;
      frame->uc_id = ulsubframe->getChannelID();
      frame->rng_status = RNG_SUCCESS;
      frame->ss_mac_address = req->ss_mac_address;
      frame->basic_cid = basic->get_cid();
      frame->primary_cid = primary->get_cid();
      ch->size() = RNG_RSP_SIZE;

      //enqueue packet
      getCManager()->get_connection (BROADCAST_CID, OUT_CONNECTION)->enqueue (rep);
      //*debug ("BS created ranging response MAC_RNG_RSP to SSID :%d, CID :%d, primary :%d, size :%d\n", req->ss_mac_address, HDR_MAC802_16(p)->header.cid, primary->get_cid(), ch->size());

      if (cl_head_==NULL) {
	cl_head_ = (new_client_t*)malloc (sizeof (new_client_t));
	cl_tail_ = cl_head_;
      } else {
	cl_tail_->next = (new_client_t*)malloc (sizeof (new_client_t));
	cl_tail_=cl_tail_->next;
      }
      cl_tail_->cid = primary->get_cid();
      cl_tail_->next = NULL;

#ifdef USE_802_21
      send_link_detected (addr(), peer->getAddr(), 1);
#endif

    } else {
      //*debug ("BS received ranging for known station (%d) and create MAC_RNG_RSP\n", req->ss_mac_address);
      //reset invited ranging retries for SS
      //create packet for answers
      Connection *basic = peer->getBasic(IN_CONNECTION);
      Connection *primary = peer->getPrimary(IN_CONNECTION);
      Packet *rep = getPacket ();
      struct hdr_cmn *ch = HDR_CMN(rep);
      rep->allocdata (sizeof (struct mac802_16_rng_rsp_frame));
      mac802_16_rng_rsp_frame *frame = (mac802_16_rng_rsp_frame*) rep->accessdata();
      frame->type = MAC_RNG_RSP;
      frame->uc_id = ulsubframe->getChannelID();
      frame->rng_status = RNG_SUCCESS;
      frame->ss_mac_address = req->ss_mac_address;
      frame->basic_cid = basic->get_cid();
      frame->primary_cid = primary->get_cid();
      ch->size() = RNG_RSP_SIZE;
      //enqueue packet
      getCManager()->get_connection (BROADCAST_CID, OUT_CONNECTION)->enqueue (rep);
    }
  }
}

/**
 * Process bandwidth request
 * @param p The request
 */
void Mac802_16BS::process_bw_req (Packet *p)
{ 
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;

  bw_req_header_t *req;
  req = (bw_req_header_t *)&header;

  //*debug ("received bandwidth request of %d bytes from %d\n", req->br, req->cid); 
  
  //retrieve the CID and update bandwidth request information
  Connection *c =  getCManager()->get_connection (req->cid, IN_CONNECTION);
  assert (c);
  if ( (req->type & 0x7) == 0x001) {
    //aggregate
    c->setBw (req->br & 0x7FFFF); //masks 19 bits
    //*debug10 ("BS (setBW) Aggregate request on connection %d of %d\n", c->get_cid(), c->getBw());
  } else if ( (req->type & 0x7) == 0x000) {
    //incremental
    c->setBw (c->getBw() + (req->br & 0x7FFFF));
    //*debug10 ("BS (setBW) Incremental request on connection %d of %d\n", c->get_cid(), c->getBw());

  } 
}

/**
 * Process cdma bandwidth request
 * @param p The request
 */
void Mac802_16BS::process_cdma_req (Packet *p)
{
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;

  cdma_req_header_t *req;
  req = (cdma_req_header_t *)&header;

  //*debug ("BS (process_cdma_req: Impossible) received cdma request code :%d from :%d\n", req->code, req->cid);

  //retrieve the CID and update bandwidth request information
  Connection *c =  getCManager()->get_connection (req->cid, IN_CONNECTION);
  assert (c);
  //*debug10 ("BS (setCDMA), code :%d, top :%d, ssid :%d\n", req->code, req->top, req->br);

  if (req->type == 0x3) {
    c->setCDMA (1);
    c->setCDMA_code (req->code);
    c->setCDMA_top (req->top);
  } else {
    c->setCDMA(2);
    c->setCDMA_SSID_FLAG(req->br, 2);
    c->setCDMA_SSID_CODE(req->br, req->code);
    c->setCDMA_SSID_TOP(req->br, req->top);
    c->setCDMA_SSID_CID(req->br, c->get_cid());
    c->setCDMA_code(0);
    c->setCDMA_top(0);
  }

}

/**
 * Process registration request
 * @param p The request
 */
void Mac802_16BS::process_reg_req (Packet *req)
{ 
  hdr_mac802_16 *wimaxHdr_req = HDR_MAC802_16(req);
  gen_mac_header_t header_req = wimaxHdr_req->header;
  
  //*debug ("BS received registration request from :%d\n", header_req.cid);

  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_reg_rsp_frame *reg_frame;
  PeerNode *peer;

  //create packet for request
  p = getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_reg_rsp_frame));
  reg_frame = (mac802_16_reg_rsp_frame*) p->accessdata();
  reg_frame->type = MAC_REG_RSP;
  reg_frame->response = 0; //OK
  peer = getCManager()->get_connection (header_req.cid, IN_CONNECTION)->getPeerNode();
  Connection *secondary = peer->getSecondary (OUT_CONNECTION);
  if (secondary==NULL) {
    //first time 
    secondary = new Connection (CONN_SECONDARY);
    Connection *upsecondary = new Connection (CONN_SECONDARY, secondary->get_cid());
    getCManager()->add_connection (upsecondary, IN_CONNECTION);
    getCManager()->add_connection (secondary, OUT_CONNECTION);
    peer->setSecondary (upsecondary, secondary);
  }
  reg_frame->sec_mngmt_cid = secondary->get_cid();
  wimaxHdr->header.cid = header_req.cid;
  ch->size() = REG_RSP_SIZE;
  //*debug ("BS created registration response MAC_REG_RSP to :%d, secondary :%d, size :%d\n", header_req.cid, secondary->get_cid(), ch->size());
  
  //enqueue packet..must be replaced with second line later
  //getCManager()->get_connection (BROADCAST_CID, OUT_CONNECTION)->enqueue (p);
  peer->getPrimary(OUT_CONNECTION)->enqueue (p);

  //clear t17 timer for this node
  removetimer17 (peer->getAddr());

#ifdef USE_802_21
  //*debug ("At %f in Mac %d, send link up\n", NOW, addr());
  send_link_up (peer->getAddr(),addr(), -1);
#endif
}

/**
 * Send a neighbor advertisement message
 */
void Mac802_16BS::send_nbr_adv ()
{
  //*debug ("At %f in BS %d send_nbr_adv (nb_neighbor=%d)\n", NOW, addr(), nbr_db_->getNbNeighbor());
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_nbr_adv_frame *frame;
  //PeerNode *peer;

  //create packet for request
  p = getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_nbr_adv_frame));
  frame = (mac802_16_mob_nbr_adv_frame*) p->accessdata();
  frame->type = MAC_MOB_NBR_ADV;
  frame->n_neighbors = nbr_db_->getNbNeighbor();
  frame->skip_opt_field = 0;
  for (int i = 0 ; i < frame->n_neighbors ; i++) {
    frame->nbr_info[i].phy_profile_id.FAindex = 0;
    frame->nbr_info[i].phy_profile_id.bs_eirp = 0;
    frame->nbr_info[i].nbr_bsid= nbr_db_->getNeighbors()[i]->getID();
    frame->nbr_info[i].dcd_included = true;
    memcpy (&(frame->nbr_info[i].dcd_settings), nbr_db_->getNeighbors ()[i]->getDCD(), sizeof(mac802_16_dcd_frame));
    frame->nbr_info[i].ucd_included = true;
    memcpy (&(frame->nbr_info[i].ucd_settings), nbr_db_->getNeighbors ()[i]->getUCD(), sizeof(mac802_16_ucd_frame));
    frame->nbr_info[i].phy_included = false;
  }
  ch->size() = Mac802_16pkt::getMOB_NBR_ADV_size(frame);
  getCManager()->get_connection (BROADCAST_CID, OUT_CONNECTION)->enqueue (p);
  
}

/**
 * Process handover request
 * @param p The request
 */
void Mac802_16BS::process_msho_req (Packet *req)
{
  hdr_mac802_16 *wimaxHdr_req = HDR_MAC802_16(req);
  gen_mac_header_t header_req = wimaxHdr_req->header;
  mac802_16_mob_msho_req_frame *req_frame = 
    (mac802_16_mob_msho_req_frame*) req->accessdata();
  
  //*debug ("At %f in Mac %d received handover request from %d\n", NOW, addr(), header_req.cid);

  //check the BS that has stronger power
  int maxIndex = 0;
  int maxRssi = 0; //max value
  //*debug2("In the MSHO Request message, there are %d new BS full entities.\n", req_frame->n_new_bs_full);
  for (int i = 0; i < req_frame->n_new_bs_full ; i++) {
    if (req_frame->bs_full[i].bs_rssi_mean >= maxRssi) {
      maxIndex = i;
      maxRssi = req_frame->bs_full[i].bs_rssi_mean;
    }
  }
  //reply with one recommended BS
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_mob_bsho_rsp_frame *rsp_frame;

  send_nbr_adv (); //to force update with latest information

  //create packet for request
  p = getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_bsho_rsp_frame)+sizeof (mac802_16_mob_bsho_rsp_rec));
  rsp_frame = (mac802_16_mob_bsho_rsp_frame*) p->accessdata();
  rsp_frame->type = MAC_MOB_BSHO_RSP;
  
  rsp_frame->mode = 0; //HO request
  rsp_frame->ho_operation_mode = 1; //mandatory handover response
  rsp_frame->n_recommended = 1;
  rsp_frame->resource_retain_flag = 0; //release connection information
  rsp_frame->n_rec[0].neighbor_bsid = req_frame->bs_full[maxIndex].neighbor_bs_index;
  rsp_frame->n_rec[0].ho_process_optimization=0; //no optimization

  ch->size() += Mac802_16pkt::getMOB_BSHO_RSP_size(rsp_frame);
  wimaxHdr->header.cid = header_req.cid;
  getCManager()->get_connection (header_req.cid, OUT_CONNECTION)->enqueue (p);
}
 
/**
 * Process handover indication
 * @param p The indication
 */
void Mac802_16BS::process_ho_ind (Packet *p)
{
  hdr_mac802_16 *wimaxHdr_req = HDR_MAC802_16(p);
  gen_mac_header_t header_req = wimaxHdr_req->header;
  //mac802_16_mob_ho_ind_frame *req_frame = 
  //  (mac802_16_mob_ho_ind_frame*) p->accessdata();
  
  //*debug ("At %f in Mac %d received handover indication from %d\n", NOW, addr(), header_req.cid);
  //*debug2("xingting is trying to remove all the connections to SS.\n");

  Connection *head  = getCManager()->get_out_connection();
  while(head != NULL)
  {
  		//*debug2("HO in out cid %d\n",head->get_cid());
		if( (head->getArqStatus() != NULL))
		{
			//*debug2("BS %d delete ARQ timer CID is [%d]\n",addr(),head->get_cid());
				head->getArqStatus()->cancelTimer();
			//delete head->getArqStatus();
			//getCManager()->remove_connection (head);
			//delete head;
		}
		head = head->next_entry();
  }
}
 
/**
 * Send a scan response to the MN
 * @param rsp The response from the control
 */
void Mac802_16BS::send_scan_response (mac802_16_mob_scn_rsp_frame *rsp, int cid)
{
  //create packet for request
  Packet *p = getPacket ();
  struct hdr_cmn *ch = HDR_CMN(p);
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_mob_scn_rsp_frame));
  mac802_16_mob_scn_rsp_frame* rsp_frame = (mac802_16_mob_scn_rsp_frame*) p->accessdata();
  memcpy (rsp_frame, rsp, sizeof (mac802_16_mob_scn_rsp_frame));
  rsp_frame->type = MAC_MOB_SCN_RSP;
  
  wimaxHdr->header.cid = cid;
  ch->size() += Mac802_16pkt::getMOB_SCN_RSP_size(rsp_frame);
  
  //add scanning station to the list
  PeerNode *peer = getCManager()->get_connection (cid, false)->getPeerNode();

  /* The request is received in frame i, the reply is sent in frame i+1
   * so the frame at which the scanning start is start_frame+2
   */
  ScanningStation *sta = new ScanningStation (peer->getAddr(), rsp_frame->scan_duration & 0xFF, 
					      rsp_frame->start_frame+frame_number_+2, 
					      rsp_frame->interleaving_interval & 0xFF,
					      rsp_frame->scan_iteration & 0xFF);
  sta->insert_entry_head (&scan_stations_);

  //enqueue packet
  getCManager()->get_connection (cid, OUT_CONNECTION)->enqueue (p);
}

/**** Internal methods ****/

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_16BS::update_watch (StatWatch *watch, double value)
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

    send_link_parameters_report (addr(), addr(), param, old_value, new_value);      
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
    //*debug2 ("At %f in Mac %d, updating watch stats %s: %f\n", NOW, addr(), name, watch->average());
}

/**
 * Update the given timer and check if thresholds are crossed
 * @param watch the stat watch to update
 * @param value the stat value
 */
void Mac802_16BS::update_throughput (ThroughputWatch *watch, double size)
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
    
    send_link_parameters_report (addr(), addr(), param, old_value, new_value);
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
    //*debug2 ("At %f in Mac %d, updating throughput stats %s: %f\n", NOW, addr(), name, watch->average());
}


/**
 * Called when a timer expires
 * @param The timer ID
 */
void Mac802_16BS::expire (timer_id id)
{
  switch (id) {
  case WimaxDCDTimerID:
    sendDCD = true;
    //*debug ("At %f in Mac %d DCDtimer expired\n", NOW, addr());
    dcdtimer_->start (macmib_.dcd_interval);
    break;
  case WimaxUCDTimerID:
    sendUCD = true;
    //*debug ("At %f in Mac %d UCDtimer expired\n", NOW, addr());
    ucdtimer_->start (macmib_.ucd_interval);
    break;
  case WimaxMobNbrAdvTimerID:
    send_nbr_adv();
    nbradvtimer_->start (macmib_.nbr_adv_interval);
    break;
  default:
    ;//*debug ("Warning: unknown timer expired in Mac802_16BS\n");
  }
}

/**
 * Start a new frame
 */
void Mac802_16BS::start_ulsubframe ()
{
  //*debug ("At %f in Mac %d BS scheduler ulsubframe expires\n", NOW, addr());

  //change PHY state
  getPhy()->setMode (OFDM_RECV);  

  //start handler of ulsubframe
  getMap()->getUlSubframe()->getTimer()->sched (0);

  //reschedule for next frame
  ul_timer_->resched (getFrameDuration());
}

/**
 * Start a new frame
 */
void Mac802_16BS::start_dlsubframe ()
{
  //*debug ("At %f in Mac %d BS scheduler dlsubframe expires (frame=%d), r_ss :%lf, r_bs :%lf\n", NOW, addr(), frame_number_++, r_delta_ss, r_delta_bs);
  r_delta_bs = 0;

  assert (map_);
  map_->setStarttime(NOW);
  ((BSScheduler*)scheduler_)->schedule();

  //update some information  
  if (sendDCD || map_->getDlSubframe()->getCCC()!= dlccc_) {
    sendDCD = false;
    dlccc_ = map_->getDlSubframe()->getCCC();
    //reschedule timer
    dcdtimer_->stop();
    dcdtimer_->start (macmib_.dcd_interval);
  }

  if (sendUCD || map_->getUlSubframe()->getCCC()!= ulccc_) {
    sendUCD = false;
    ulccc_ = map_->getUlSubframe()->getCCC();
    //reschedule timer
    ucdtimer_->stop();
    ucdtimer_->start (macmib_.ucd_interval);
  }

  //change PHY state
  getPhy()->setMode (OFDM_SEND);

  //start handler of dlsubframe
  getMap()->getDlSubframe()->getTimer()->sched (0);

  //reschedule for next time (frame duration)
  dl_timer_->resched (getFrameDuration());  
  ul_timer_->resched (map_->getUlSubframe()->getStarttime()*getPhy()->getPS());
}

/** Add a new Fast Ranging allocation
 * @param time The time when to allocate data
 * @param macAddr The MN address
 */
void Mac802_16BS::addNewFastRanging (double time, int macAddr)
{
  //compute the frame where the allocation will be located
  int frame = int ((time-NOW)/getFrameDuration()) +2 ;
  frame += getFrameNumber();
  //debug2 ("Added fast RA for frame %d (current=%d) for time (%f)\n", 
  //	  frame, getFrameNumber(), time);
  FastRangingInfo *info= new FastRangingInfo (frame, macAddr);
  info->insert_entry(&fast_ranging_head_);
}

/**
 * Add a new timer17 in the list. It also performs cleaning of the list
 * @param index The client address
 */
void Mac802_16BS::addtimer17 (int index)
{
  //clean expired timers
  T17Element *entry;
  for (entry = t17_head_.lh_first; entry ; ) {
    if (!entry->busy ()) {
      T17Element *tmp = entry;
      entry = entry->next_entry();
      tmp->remove_entry();
      delete (tmp);
    } else {
      entry = entry->next_entry();
    }
  }

  entry = new T17Element (this, index);
  entry->insert_entry (&t17_head_);
}

/**
 * Cancel and remove the timer17 associated with the node
 * @param index The client address
 */
void Mac802_16BS::removetimer17 (int index)
{
  //clean expired timers
  T17Element *entry;
  for (entry = t17_head_.lh_first; entry ; entry = entry->next_entry()) {
    if (entry->index ()==index) {
      entry->cancel();
      entry->remove_entry();
      delete (entry);
      break;
    }
  }
}

/** 
 * Finds out if the given station is currently scanning
 * @param nodeid The MS id
 */
bool Mac802_16BS::isPeerScanning (int nodeid)
{
  ScanningStation *sta;
  for (sta = scan_stations_.lh_first; sta ; sta = sta->next_entry()) {
    if (sta->getNodeId()==nodeid && sta->isScanning(frame_number_)) {
      //debug2 ("station %d scanning\n", nodeid);
      return true;
    }
  }
  return false;
}


void Mac802_16BS::addPowerinfo(hdr_mac802_16 *wimaxHdr,double power_per_subchannel, bool collision )
{
 
  int num_data_subcarrier = getPhy()->getNumSubcarrier(UL_); 
  int num_subchannel = getPhy()->getNumsubchannels (UL_);
  int num_symbol_per_slot = getPhy()->getSlotLength (UL_); 

  if (collision == true) {

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
	      for(int j = 1 ; j<=num_subchannel; j++ )
                (basepower_[i][j] = power_per_subchannel);  

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i< wimaxHdr->phy_info.OFDMSymbol_offset + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = 1 ; j<=((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset-1)) )%num_subchannel) ; j++ )
		(basepower_[i][j] = power_per_subchannel); 
          }
	else 
          {
	    int i = wimaxHdr->phy_info.OFDMSymbol_offset;
	    int j = wimaxHdr->phy_info.subchannel_offset;
	    for (int k=0 ; k < wimaxHdr->phy_info.num_subchannels ; k++) {
	      //printf ("Assigning basepower_ [%d][%d]\n", i, j);
	      assert (i<(int) floor((getPhy()->getPS()*(int) floor((getFrameDuration()/getPhy()->getPS())))/getPhy()->getSymbolTime()));
	      assert (j < getPhy()->getNumsubchannels (UL_));
	      basepower_[i][j] = power_per_subchannel; 	    	      
	      j++;
	      if (j == getPhy()->getNumsubchannels (UL_)) {
		j = 0 ; 
		i++;
	      }
	    }
	    /*
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j< (wimaxHdr->phy_info.subchannel_offset) + wimaxHdr->phy_info.num_subchannels ; j++ ) {
	      printf ("Assigning basepower_ [%d][%d]\n", i, j);
	      assert (i<(int) floor((getPhy()->getPS()*(int) floor((getFrameDuration()/getPhy()->getPS())))/getPhy()->getSymbolTime()));
	      assert (j < getPhy()->getNumsubchannels (UL_));
	      (basepower_[i][j] = power_per_subchannel); 	    
	      }
	    */
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
	      for(int j = 1 ; j<=((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset-1)) )%num_subchannel) ; j++ )
		(basepower_[i][j] = power_per_subchannel); 

          }
	else
          {
          
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1)/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = (wimaxHdr->phy_info.subchannel_offset) ; j< wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels ; j++ )
		(basepower_[i][j] = power_per_subchannel); 	

          }
             
      }
  } else {

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
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
                (intpower_[i][j] = power_per_subchannel);
	    // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+num_symbol_per_slot ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot) ; i++)
	      for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
                (intpower_[i][j] = power_per_subchannel);  

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i< wimaxHdr->phy_info.OFDMSymbol_offset + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = 0 ; j<((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier ; j++ )
		(intpower_[i][j] = power_per_subchannel); 
          }
	else 
          {
	    int i = wimaxHdr->phy_info.OFDMSymbol_offset ;
	    int j = wimaxHdr->phy_info.subchannel_offset*num_data_subcarrier ;
	    for (int k = 0 ; k < wimaxHdr->phy_info.num_subchannels*num_data_subcarrier ; k++) {
	      //printf ("Assigning intpower_ [%d][%d]\n", i, j);
	      assert (i<(int) floor((getPhy()->getPS()*(int) floor((getFrameDuration()/getPhy()->getPS())))/getPhy()->getSymbolTime()));
	      assert (j < getPhy()->getFFT());
	      intpower_[i][j] = power_per_subchannel;		
	      if (j == getPhy()->getFFT()) {
		j = 0 ; 
		i++;
	      }	      
	    }
	    /*
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
              for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier) + wimaxHdr->phy_info.num_subchannels*num_data_subcarrier) ; j++ ) {
	      printf ("Assigning intpower_ [%d][%d]\n", i, j);
	      assert (i<(int) floor((getPhy()->getPS()*(int) floor((getFrameDuration()/getPhy()->getPS())))/getPhy()->getSymbolTime()));
	      assert (j < getPhy()->getFFT());
	      (intpower_[i][j] = power_per_subchannel); 	    
	      }
	    */
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
              for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
		(intpower_[i][j] = power_per_subchannel); 
	    // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
          
            for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+1 ; i<  ((wimaxHdr->phy_info.OFDMSymbol_offset) + wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
	      for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
		intpower_[i][j] = power_per_subchannel;

	    // last num_symbol_per_slot 
	    for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-1 ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + wimaxHdr->phy_info.num_OFDMSymbol/*wimaxHdr->phy_info.num_OFDMSymbol)*/ ; i++)
	      for(int j = 0 ; j<((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier ; j++ )
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
   
}





bool Mac802_16BS::IsCollision (const hdr_mac802_16 *wimaxHdr,double power_subchannel)

{

  bool collision = FALSE; 
  int num_subchannel = getPhy()->getNumsubchannels (UL_);
  int num_symbol_per_slot = getPhy()->getSlotLength (UL_); 
   
  if(wimaxHdr->phy_info.num_OFDMSymbol % num_symbol_per_slot == 0 )   // chk this condition , chk whether broacastcid reqd ir not. 
    // if((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) > 30)
    //if(wimaxHdr->phy_info.num_OFDMSymbol >= num_symbol_per_slot)
    {
      if(wimaxHdr->phy_info.num_OFDMSymbol > num_symbol_per_slot) 
	{
	  //numsymbols = (int)  ceil((((wimaxHdr->phy_info.subchannel_offset + wimaxHdr->phy_info.num_subchannels -1) - 30) % 30));
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
	      //              if(collision == TRUE) return collision;  
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
	      //              if(collision == TRUE) return collision;  
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



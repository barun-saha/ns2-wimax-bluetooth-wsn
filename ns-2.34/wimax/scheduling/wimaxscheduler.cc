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
 * @author  Richard Rouil
 * @modified by Chakchai So-In
 * @modified by Xingting Guo
 */

#include "wimaxscheduler.h"
#define HDR_PIG 2
#define BW_REQ_TIMEOUT 2
#define INIT_REQ_TIMEOUT 2
#define UL_HORIZONTAL 1
//#define IF_PIG

/*
 * Create a scheduler
 * @param mac The Mac where it is located
 */
WimaxScheduler::WimaxScheduler ()
{
  bind ("dlratio_", &dlratio_);
}

/*
 * Set the mac
 * @param mac The Mac where it is located
 */
void WimaxScheduler::setMac (Mac802_16 *mac)
{
  assert (mac!=NULL);

  mac_ = mac;
}

/**
 * Initialize the scheduler.
 */
void WimaxScheduler::init()
{
  // initializing dl and ul duration  --rpi
  OFDMAPhy *phy = mac_->getPhy();

  int nbPS = (int) floor((mac_->getFrameDuration()/phy->getPS()));
#ifdef DEBUG_WIMAX
  assert (nbPS*phy->getPS()<=mac_->getFrameDuration()); //check for rounding errors
#endif
  int nbPS_left = nbPS - mac_->phymib_.rtg - mac_->phymib_.ttg;
  int nbSymbols = (int) floor((phy->getPS()*nbPS_left)/phy->getSymbolTime());  // max num of OFDM symbols available per frame. 
  assert (nbSymbols*phy->getSymbolTime()+(mac_->phymib_.rtg + mac_->phymib_.ttg)*phy->getPS() < mac_->getFrameDuration());
  int maxdlduration = (int) (nbSymbols / (1.0/dlratio_)); //number of symbols for downlink
  int maxulduration = nbSymbols - maxdlduration;            //number of symbols for uplink

  mac_->setMaxDlduration (maxdlduration);
  mac_->setMaxUlduration (maxulduration);
  
  //debug2(" in wimax scheduler maxdlduration = %d maxulduration =%d mac =%d \n", maxdlduration, maxulduration, mac_->addr());
}

/**
 * This function is used to schedule bursts/packets
 */
void WimaxScheduler::schedule ()
{
  //defined by subclasses
}


/**
 * Transfer the packets from the given connection to the given burst
 * @param con The connection
 * @param b The burst
 * @param b_data Amount of data in the burst
 * @return the new burst occupation
 */
int WimaxScheduler::transfer_packets (Connection *c, Burst *b, int b_data, int subchannel_offset, int symbol_offset)
{
  Packet *p;
  hdr_cmn* ch;
  hdr_mac802_16 *wimaxHdr;
  double txtime, txtime2, txtime3;
  int txtime_s;
  bool pkt_transfered = false;
  OFDMAPhy *phy = mac_->getPhy();
  //int offset = b->getStarttime( );
  PeerNode *peer;

  //  debug2 (" entering transfer packets");// , bdata = %d  mac_ = %d \n ",b_data, mac_);
  //debug10 ("\tEntered transfer_packets MacADDR :%d, with bdata  :%d\n ", mac_->addr(), b_data);
  peer = mac_->getPeerNode_head();

  p = c->get_queue()->head();

  int max_data;
  if (mac_->getNodeType()==STA_BS) 
    { 
      max_data = phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding())-b_data;

      //debug10 ("\tBS_transfer_packets, CID :%d with bdata :%d, but MaxData (MaxSize-bdata) :%d, MaxSize :%d\n", c->get_cid(), b_data, max_data, phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding()) );
      //debug10 ("\t                     Bduration :%d, DIUC :%d\n",b->getDuration(), b->getIUC());

    } 
  else 
    {
      max_data = phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding())-b_data;

      //debug10 ("\tSS_transfer_packets, CID :%d with bdata :%d, but MaxData (MaxSize-bdata) :%d, MaxSize :%d\n", c->get_cid(), b_data, max_data, phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding()) );
      //debug10 ("\t                     Bduration :%d, UIUC :%d\n",b->getDuration(), b->getIUC());
    }

  if (max_data < HDR_MAC802_16_SIZE ||
      (c->getFragmentationStatus()!=FRAG_NOFRAG && max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE))
    return b_data; //not even space for header

  while (p) {
    ch = HDR_CMN(p);
    wimaxHdr = HDR_MAC802_16(p);

    if (mac_->getNodeType()==STA_BS) max_data = phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding())-b_data;
    else max_data = phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding())-b_data;

    //debug2 ("\tIn Loop MacADDR :%d, MaxDATA :%d (burst duration :%d, b_data :%d)\n", mac_->addr(), max_data, b->getDuration(), b_data);
    if (max_data < HDR_MAC802_16_SIZE ||
        (c->getFragmentationStatus()!=FRAG_NOFRAG && max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE))
      return b_data; //not even space for header

    if (c->getFragmentationStatus()!=FRAG_NOFRAG) {
      if (max_data >= ch->size()-c->getFragmentBytes()+HDR_MAC802_16_FRAGSUB_SIZE) {
        //add fragmentation header
        wimaxHdr->header.type_frag = true;
        //no need to fragment again
        wimaxHdr->frag_subheader.fc = FRAG_LAST;
        wimaxHdr->frag_subheader.fsn = c->getFragmentNumber ();
        c->dequeue();  /*remove packet from queue */

	ch->size() = ch->size()-c->getFragmentBytes()+HDR_MAC802_16_FRAGSUB_SIZE; //new packet size
        //update fragmentation
        //debug2 ("\tEnd of fragmentation (FRAG :%x), FSN :%d (max_data :%d, bytes to send :%d\n", FRAG_LAST, wimaxHdr->frag_subheader.fsn, max_data, ch->size());

        c->updateFragmentation (FRAG_NOFRAG, 0, 0);
      } else {
	//need to fragment the packet again
        p = p->copy(); //copy packet to send
        ch = HDR_CMN(p);
        wimaxHdr = HDR_MAC802_16(p);
        //add fragmentation header
        wimaxHdr->header.type_frag = true;
        wimaxHdr->frag_subheader.fc = FRAG_CONT;
        wimaxHdr->frag_subheader.fsn = c->getFragmentNumber ();
        ch->size() = max_data; //new packet size
        //update fragmentation
        c->updateFragmentation (FRAG_CONT, (c->getFragmentNumber ()+1)%8, c->getFragmentBytes()+max_data-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
        //debug2 ("\tContinue fragmentation (FRAG :%x), FSN :%d\n", FRAG_CONT, wimaxHdr->frag_subheader.fsn);
      }
    } 
    else 
      {
	if (max_data < ch->size() && c->isFragEnable()) {
	  //need to fragment the packet for the first time
	  p = p->copy(); //copy packet to send
	  ch = HDR_CMN(p);
	  wimaxHdr = HDR_MAC802_16(p);
	  //add fragmentation header
	  wimaxHdr->header.type_frag = true;
	  wimaxHdr->frag_subheader.fc = FRAG_FIRST;
	  wimaxHdr->frag_subheader.fsn = c->getFragmentNumber ();
	  ch->size() = max_data; //new packet size
	  //update fragmentation
	  c->updateFragmentation (FRAG_FIRST, 1, c->getFragmentBytes()+max_data-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
	  //debug2 ("\tFirst fragmentation (FRAG :%x), FSN :%d\n", FRAG_FIRST, c->getFragmentNumber());

	} else if (max_data < ch->size() && !c->isFragEnable()) {
	  //the connection does not support fragmentation
	  //can't move packets anymore
	  return b_data;
	} else {
	  //no fragmentation necessary
	  c->dequeue();
	}
      }


    if (mac_->getNodeType()==STA_BS) 
      {
	txtime = phy->getTrxTime (ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
	txtime2 = phy->getTrxSymbolTime (b_data+ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
	txtime3 = phy->getTrxSymbolTime (ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
      }
    else 
      {
	txtime = phy->getTrxTime (ch->size(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding());
	txtime2 = phy->getTrxSymbolTime (b_data+ch->size(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding());
	txtime3 = phy->getTrxSymbolTime (ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
      }
    ch->txtime() = txtime3;
    txtime_s = (int)round (txtime3/phy->getSymbolTime ()); //in units of symbol
    //debug2 ("symbtime=%f\n", phy->getSymbolTime ());
    //*debug2 ("\tCheck packet to send size :%d txtime :%f(%d) duration :%d(%f)\n", ch->size(),txtime, txtime_s, b->getDuration(), b->getDuration()*phy->getSymbolTime ());
    assert ( txtime2 <= b->getDuration()*phy->getSymbolTime () );
    //*debug2 ("\tTransfer to burst (txtime :%f, txtime2 :%f, bduration :%f, txtime3 :%f)\n", txtime,txtime2,b->getDuration()*phy->getSymbolTime (),txtime3);


    int total_DL_num_subchannel = phy->getNumsubchannels(DL_);
    Ofdm_mod_rate dlul_map_mod = mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding();
    int num_of_slots = (int) ceil(ch->size()/phy->getSlotCapacity(dlul_map_mod,DL_)); //get the slots	needs.
    int num_of_subchannel = num_of_slots*2;
    int num_of_symbol = (int) ceil((double)(subchannel_offset + num_of_subchannel)/total_DL_num_subchannel);		


    wimaxHdr = HDR_MAC802_16(p);
    if(wimaxHdr)
      {
	//wimaxHdr->phy_info.num_subchannels = b->getnumSubchannels();
	//wimaxHdr->phy_info.subchannel_offset = b->getSubchannelOffset ();
	//wimaxHdr->phy_info.num_OFDMSymbol = txtime_s;
	//wimaxHdr->phy_info.OFDMSymbol_offset = symbol_offset;//(int)round ((phy->getTrxSymbolTime (b_data, mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding()))/phy->getSymbolTime ());//offset;

	wimaxHdr->phy_info.num_subchannels = num_of_subchannel;
	wimaxHdr->phy_info.subchannel_offset = subchannel_offset;
	wimaxHdr->phy_info.num_OFDMSymbol = num_of_symbol;
	wimaxHdr->phy_info.OFDMSymbol_offset = symbol_offset;//(int)round ((phy->getTrxSymbolTime (b_data, mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding()))/phy->getSymbolTime ());//offset;


	// if(c->getPeerNode())
	//wimaxHdr->phy_info.channel_index = c->getPeerNode()->getchannel();
	wimaxHdr->phy_info.channel_index = 1; 
	if (mac_->getNodeType()==STA_BS)
	  wimaxHdr->phy_info.direction = 0;
	else 
	  wimaxHdr->phy_info.direction = 1;
      } 
		
    symbol_offset += num_of_symbol-1;
    //symbol_offset+= txtime_s;

    //p = c->dequeue();   //dequeue connection queue
    //debug10("enqueue into the burst for cid [%d].\n",c->get_cid());
    //*debug10("New packet in [%d] CID burst ||| sym_#[%d]\t sym_off[%d]\t subch_#[%d]\t subch_off[%d]\n",
	    //*c->get_cid(), wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.OFDMSymbol_offset,wimaxHdr->phy_info.num_subchannels,
	    //*wimaxHdr->phy_info.subchannel_offset);
    b->enqueue(p);         //enqueue into burst
    b_data += ch->size(); //increment amount of data enqueued
    if (!pkt_transfered && mac_->getNodeType()!=STA_BS){ //if we transfert at least one packet, remove bw request
      pkt_transfered = true;
      mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());
    }
    p = c->get_queue()->head(); //get new head
  }
  return b_data;

} 

/**
 * Transfer the packets from the given connection to the given burst
 * @param con The connection
 * @param b The burst
 * @param b_data Amount of data in the burst
 * @return the new burst occupation
 */
int WimaxScheduler::transfer_packets1 (Connection *c, Burst *b, int b_data)
{
  Packet *p;
  hdr_cmn* ch;
  hdr_mac802_16 *wimaxHdr;
  double txtime;
  int txtime_s;
  bool pkt_transfered = false;
  OFDMAPhy *phy = mac_->getPhy();
  int offset = b->getStarttime( );
  PeerNode *peer;
  int numsubchannels,subchannel_offset; 
  int initial_offset = b->getStarttime( );
  int initial_subchannel_offset = b->getSubchannelOffset ();
  int bw_req_packet = GENERIC_HEADER_SIZE;
  int flag_bw_req = 0;
  int flag_pig = 0;
  int old_subchannel_offset = 0;

  int num_symbol_per_slot;

  subchannel_offset = initial_subchannel_offset;
  old_subchannel_offset = subchannel_offset;

  if (mac_->getNodeType()==STA_BS) {
    num_symbol_per_slot = phy->getSlotLength (DL_); 
  } else {
    num_symbol_per_slot = phy->getSlotLength (UL_); 
  }

  //debug10 ("\tEntered transfer_packets1 for MacADDR :%d, for connection :%d with bdata  :%d, #subchannels :%d, #symbol :%d, IUC :%d\n", mac_->addr(), c->get_cid(), b_data, b->getnumSubchannels(), b->getDuration(), b->getIUC());

//Start sending registration message
  //*printf ("TOTO: check init ranging %d \n", c->getINIT_REQ_QUEUE(mac_->addr()));
  if ( (c->get_cid() == 0) && (c->getINIT_REQ_QUEUE(mac_->addr()) == 0) && (mac_->getNodeType()!=STA_BS) ) {

    Packet *p1= mac_->getPacket();
    hdr_cmn* ch = HDR_CMN(p1);
    hdr_mac802_16 *wimaxHdr;

    wimaxHdr = HDR_MAC802_16(p1);
    wimaxHdr->header.cid = INITIAL_RANGING_CID;

    int max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_) - b_data;    

    p1->allocdata (sizeof (struct mac802_16_rng_req_frame));
    mac802_16_rng_req_frame *frame = (mac802_16_rng_req_frame*) p1->accessdata();
    frame->type = MAC_RNG_REQ;
    frame->ss_mac_address = mac_->addr();

    wimaxHdr->header.type_frag = 0;
    wimaxHdr->header.type_fbgm = 0;
    //frame->req_dl_burst_profile = DIUC_PROFILE_7 & 0xF; //we use lower bits only
    frame->req_dl_burst_profile = mac_->get_diuc() & 0xF; //we use lower bits only

    ch->size() = RNG_REQ_SIZE;

    //*debug10 ("\t   Sending INIT-RNG-MSG-REQ, cid :%d, ssid :%d, MaxSize :%d, msg-size :%d\n", c->get_cid(), mac_->addr(), max_data, ch->size());

    int assigned_subchannels;
    int symbol_offset;

    assigned_subchannels = phy->getNumSubchannels(b_data, mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);
    numsubchannels = phy->getNumSubchannels(ch->size(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);

    Ofdm_mod_rate dlul_map_mod = mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding();
    int num_of_slots = (int) ceil((double)ch->size()/phy->getSlotCapacity(dlul_map_mod,UL_)); //get the slots needs.
    symbol_offset = b->getStarttime() + num_symbol_per_slot * ((b->getSubchannelOffset()+assigned_subchannels)/ phy->getNumsubchannels(UL_));
    txtime_s = (int)(ceil((double)(numsubchannels + subchannel_offset) / (double)phy->getNumsubchannels(UL_)))*num_symbol_per_slot;
    offset = b->getStarttime( )+ txtime_s-num_symbol_per_slot;
    subchannel_offset = (int) ceil((numsubchannels + subchannel_offset) % (phy->getNumsubchannels(UL_)));

    ch->txtime() = txtime_s*phy->getSymbolTime ();

    //*debug10 ("\t   Initial_Ranging_Msg, In station %d, Check packet to send: con id in BURST :%d, con id in connection :%d, size :%d, txtime :%f(%d), duration :%d(%f), numsubchnn :%d, subchnoff :%d\n",mac_->getNodeType(), c->get_cid(),b->getCid(), ch->size(),txtime, txtime_s, b->getDuration(), b->getDuration()*phy->getSymbolTime (),b->getnumSubchannels(),b->getSubchannelOffset ());

    wimaxHdr->phy_info.num_subchannels = numsubchannels;
    wimaxHdr->phy_info.subchannel_offset = initial_subchannel_offset;//subchannel_offset;
    wimaxHdr->phy_info.num_OFDMSymbol = txtime_s;
    wimaxHdr->phy_info.OFDMSymbol_offset = symbol_offset; //initial_offset;
    wimaxHdr->phy_info.channel_index = (mac_->getPeerNode_head())->getchannel();
    wimaxHdr->phy_info.direction = 1;

    //*debug10 ("\t   Initial_Ranging_Msg, In station %d, Packet phy header: numsymbols :%d, symboloffset :%d, subchanneloffset :%d, numsubchannel :%d, Channel_num :%d, direction :%d\n", mac_->getNodeType(), txtime_s,initial_offset, initial_subchannel_offset,numsubchannels,wimaxHdr->phy_info.channel_index,wimaxHdr->phy_info.direction);

    initial_offset = offset;
    initial_subchannel_offset = subchannel_offset;

    b->enqueue(p1);      //enqueue into burst
    b_data = b_data+ch->size();
//    c->setINIT_REQ_QUEUE(mac_->addr(), INIT_REQ_TIMEOUT);
    int t6time = (ceil)((double)(mac_->macmib_.t6_timeout)/(double)(mac_->macmib_.frame_duration));
    subchannel_offset = old_subchannel_offset;

    if ( mac_->getMap()->getUlSubframe()->getRanging()->getRequest_mac (mac_->addr()) != NULL ) {
       mac_->getMap()->getUlSubframe()->getRanging()->setTIMEOUT (mac_->addr(), t6time);
       //*debug10("\t   Reset CDMA_INIT_REQ timer to #frame :%d (%f sec) since send INIT-RNG-MSGQ out for cid :%d, ssid :%d\n", t6time, mac_->macmib_.t6_timeout, c->get_cid(), mac_->addr());
       c->setINIT_REQ_QUEUE(mac_->addr(), t6time);
    }

    if (mac_->getNodeType()!=STA_BS) {
       if ( mac_->getMap()->getUlSubframe()->getRanging()->getRequest_mac (mac_->addr()) != NULL ) {
          Packet *p_tmp = NULL;
          FrameMap *map1 = mac_->getMap();
          Burst *b1;

          if ((p_tmp = mac_->getMap()->getUlSubframe()->getRanging()->getPacket_P_mac(mac_->addr())) != NULL) {
             int flag_transmit = mac_->getMap()->getUlSubframe()->getRanging()->getFLAGNOWTRANSMIT(mac_->addr());

             //debug10 ("Search flag_transmit :%d; remove cdma_bw_req if flag == 1, nbpdu :%d\n", flag_transmit, map1->getUlSubframe()->getNbPdu ());
             if (flag_transmit == 1) {
                int break_frame = 0;
                for (int index = 0 ; index < map1->getUlSubframe()->getNbPdu (); index++) {
                    //debug10 ("Search index :%d for CDMA enqueue :%d\n", index, mac_->addr());
                    b1 = map1->getUlSubframe()->getPhyPdu (index)->getBurst (0);

                    if (b1->getIUC()==UIUC_INITIAL_RANGING) {
                        int $index_p_burst = 0;
                        int q_len = map1->getUlSubframe()->getPhyPdu(index)->getBurst(0)->getQueueLength_packets();

                        while(q_len>0) {
                           Packet *p_tmp_b = map1->getUlSubframe()->getPhyPdu(index)->getBurst(0)->lookup($index_p_burst);
                           q_len--;
                           $index_p_burst++;

                           if (p_tmp_b) {
                                hdr_cmn* ch_tmp = HDR_CMN(p_tmp_b);
                                hdr_mac802_16 *wimaxHdr_send;
                                wimaxHdr_send = HDR_MAC802_16(p_tmp_b);
                                cdma_req_header_t *header_s = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp_b)->header);
                                if (mac_->addr() == header_s->br) {
                                   //debug10 ("\tFound enqueued cdma_init_req in BURST (remove it) CID :%d, ADDR :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d\n", header_s->cid, header_s->br, wimaxHdr_send->phy_info.num_OFDMSymbol, wimaxHdr_send->phy_info.OFDMSymbol_offset, wimaxHdr_send->phy_info.num_subchannels, wimaxHdr_send->phy_info.subchannel_offset, header_s->code);
                                   b1->remove(p_tmp_b);
                                   mac_->getMap()->getUlSubframe()->getRanging()->setFLAGNOWTRANSMIT(mac_->addr(), 0);
                                   break_frame = 1;
                                   break;
                                }
                            }
                        }
                    }
                    if (break_frame == 1) break;
                }
             }
          }

       }

    }//end SS


    return b_data;
  }
//end sending registration message

  peer = mac_->getPeerNode_head();
  p = c->get_queue()->head();

  int max_data;

  if (mac_->getNodeType()==STA_BS) {
    max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_) - b_data;    

    //debug10 ("\tBS_transfer1.1, CID :%d with bdata :%d, but MaxData (MaxSize-bdata) :%d, MaxSize :%d, q->bytes :%d\n", c->get_cid(), b_data, max_data, phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_), c->queueByteLength() );
    //debug10 ("\t   Bduration :%d, Biuc :%d, B#subchannel :%d\n",b->getDuration(), b->getIUC(), b->getnumSubchannels());

  } else {
    max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_) - b_data;    

    //debug10 ("\tSS_transfer1.1, CID :%d with bdata :%d, but MaxData (MaxSize-bdata) :%d, MaxSize :%d, bw-header-size :%d, q->bytes :%d\n", c->get_cid(), b_data, max_data, phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_), bw_req_packet, c->queueByteLength() );
    //debug10 ("\t   Bduration :%d, Biuc :%d, B#subchannel :%d\n",b->getDuration(), b->getIUC(), b->getnumSubchannels());

    if ( (max_data >= HDR_MAC802_16_SIZE) && (mac_->getMap()->getUlSubframe()->getBw_req()->getRequest (c->get_cid())!=NULL) && (c->queueByteLength() > 0) ) {

    int slot_br = 0;
    Connection *c_tmp;
    c_tmp = c;

    int i_packet = 0;
    int  already_frag = 0;
    int real_bytes = 0;

    Packet *np;
    //debug10 ("Retrive connection :%d, qlen :%d\n", c_tmp->get_cid(), c_tmp->queueLength());
    for (int j_p = 0; j_p<c_tmp->queueLength(); j_p++) {
        if ( (np = c_tmp->queueLookup(i_packet)) != NULL ) {
            int p_size = hdr_cmn::access(np)->size();
//            debug10 ("CON CID :%d, packet-id :%d, q->byte :%d, q->len :%d, packet_size :%d, frag_no :%d, frag_byte :%d, frag_stat :%d, real_bytes :%d\n", c_tmp->get_cid(), i_packet, c_tmp->queueByteLength(), c_tmp->queueLength(), p_size, c_tmp->getFragmentNumber(), c_tmp->getFragmentBytes(), (int)c_tmp->getFragmentationStatus(), real_bytes );            
	    i_packet++;

            if ( (c_tmp->getFragmentBytes()>0) && (already_frag == 0) ){
               p_size = p_size - c_tmp->getFragmentBytes() + 4;
               already_frag = 1;
            }

            Ofdm_mod_rate dlul_map_mod = mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding();
            int num_of_slots = (int) ceil((double)p_size/(double)phy->getSlotCapacity(dlul_map_mod,UL_));

            real_bytes = real_bytes + (int) ceil((double)num_of_slots*(double)(phy->getSlotCapacity(dlul_map_mod,UL_)));
        }
   }

      if (c_tmp->getBW_REQ_QUEUE() == 0) flag_bw_req = 0;
      if (max_data >= real_bytes) {
         flag_bw_req = 0;
      } else {

#ifndef IF_PIG
        flag_bw_req = 6;
        //*debug10 ("\t   CheckBW-REQ (cdma_bw_req exist), generate BW-REQ, maxdata :%d, bw-req-size :%d, queue :%d\n",max_data, bw_req_packet, c->queueByteLength());
#endif

#ifdef IF_PIG
        if (max_data <= 10) {
           flag_bw_req = 6;
           //*debug10 ("\t   CheckBW-REQ (cdma_bw_req exist), generate BW-REQ, maxdata :%d, queue :%d\n",max_data, c->queueByteLength());
        } else {
           //*debug10 ("\t   CheckBW-REQ (cdma_bw_req exist), generate Piggybacking, maxdata :%d, queue :%d\n",max_data, c->queueByteLength());
           flag_pig = 2;
           flag_bw_req = 0;
        }
#endif

      }

    } else {

      if (max_data >= c->queueByteLength()) {
         flag_bw_req = 0;
         flag_pig = 0;
      } else {

#ifdef IF_PIG
      if ( ( max_data < c->queueByteLength() ) && (max_data > 10) && (c->queueByteLength() > 0)) {
         //*debug10 ("\t   CheckBW-REQ (cdma_bw_req exist), generate Piggybacking, maxdata :%d, queue :%d\n",max_data, c->queueByteLength());
         flag_pig = 2;
      }
#endif
      }

    }//end if sending bw-req


  }

//Start sending BW-REQ packet
  if (flag_bw_req > 0) {
    Packet *p1= mac_->getPacket();
    hdr_cmn* ch = HDR_CMN(p1);
    hdr_mac802_16 *wimaxHdr;

    wimaxHdr = HDR_MAC802_16(p1);

    bw_req_header_t *header = (bw_req_header_t *)&(HDR_MAC802_16(p1)->header);
    header->ht=1;
    header->ec=1;
    header->type = 0x1; //aggregate
//    header->br = c->queueByteLength();
    header->cid = c->get_cid();
    int real_bytes = 0;

    int slot_br = 0;
    int max_tmp_data_t1 = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_) - b_data;
    Ofdm_mod_rate dlul_map_mod = mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding();
    int num_slot_t1 =ceil((double)GENERIC_HEADER_SIZE/(double)phy->getSlotCapacity(dlul_map_mod,UL_));
    int max_tmp_data = phy->getMaxPktSize (b->getnumSubchannels()-num_slot_t1, mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_) - b_data;

//    real_bytes = c->queueByteLength()-max_tmp_data;

    Connection *c_tmp;
    c_tmp = c;

    int i_packet = 0;
    int  already_frag = 0;

    Packet *np_tmp;
    //debug10 ("Retrive connection :%d, qlen :%d\n", c_tmp->get_cid(), c_tmp->queueLength());
    for (int j_p = 0; j_p<c_tmp->queueLength(); j_p++) {
        if ( (np_tmp = c_tmp->queueLookup(i_packet)) != NULL ) {
            int p_size = hdr_cmn::access(np_tmp)->size();
//            debug10 ("CON CID :%d, packet-id :%d, q->byte :%d, q->len :%d, packet_size :%d, frag_no :%d, frag_byte :%d, frag_stat :%d, real_bytes :%d\n", c_tmp->get_cid(), i_packet, c_tmp->queueByteLength(), c_tmp->queueLength(), p_size, c_tmp->getFragmentNumber(), c_tmp->getFragmentBytes(), (int)c_tmp->getFragmentationStatus(), real_bytes );
            i_packet++;

            if ( (c_tmp->getFragmentBytes()>0) && (already_frag == 0) ){
               p_size = p_size - c_tmp->getFragmentBytes() + 4;
               already_frag = 1;
            }

            //Ofdm_mod_rate dlul_map_mod = mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(); //chackhai
            int num_of_slots = (int) ceil((double)p_size/(double)phy->getSlotCapacity(dlul_map_mod,UL_));

            real_bytes = real_bytes + (int) ceil((double)num_of_slots*(double)(phy->getSlotCapacity(dlul_map_mod,UL_)));
        }
     }

    slot_br = real_bytes - max_tmp_data;
    if (slot_br <0) slot_br = 0;
    if (max_tmp_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE) {
	slot_br = real_bytes;
    }
    header->br = slot_br;

    header->cid = c->get_cid();
    //*debug10 ("\t   Sending BW-REQ, cid :%d, qBytes :%d, qLen :%d, bw-req :%d, real_bytes :%d\n", header->cid, c->queueByteLength(), c->queueLength(), header->br, real_bytes);

    wimaxHdr->header.type_frag = 0;
    wimaxHdr->header.type_fbgm = 0;

    int assigned_subchannels;
    int symbol_offset;
    ch->size() = bw_req_packet;

    assigned_subchannels = phy->getNumSubchannels(b_data, mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);
    numsubchannels = phy->getNumSubchannels(ch->size(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);

//    Ofdm_mod_rate dlul_map_mod = mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(); //chakchai
    int num_of_slots = (int) ceil((double)ch->size()/phy->getSlotCapacity(dlul_map_mod,UL_)); //get the slots needs.
    symbol_offset = b->getStarttime() + num_symbol_per_slot * ((b->getSubchannelOffset()+assigned_subchannels)/ phy->getNumsubchannels(UL_));
    txtime_s = (int)(ceil((double)(numsubchannels + subchannel_offset) / (double)phy->getNumsubchannels(UL_)))*num_symbol_per_slot;
    offset = b->getStarttime( )+ txtime_s-num_symbol_per_slot; 
    subchannel_offset = (int) ceil((numsubchannels + subchannel_offset) % (phy->getNumsubchannels(UL_)));

    ch->txtime() = txtime_s*phy->getSymbolTime ();

    //*debug10 ("\t   BW-REQ, In station %d, Check packet to send: con id in BURST :%d, con id in connection :%d, size :%d, txtime :%f(%d), duration :%d(%f), numsubchnn :%d, subchnoff :%d\n",mac_->getNodeType(), c->get_cid(),b->getCid(), ch->size(),txtime, txtime_s, b->getDuration(), b->getDuration()*phy->getSymbolTime (),b->getnumSubchannels(),b->getSubchannelOffset ());

    wimaxHdr->phy_info.num_subchannels = numsubchannels;
    wimaxHdr->phy_info.subchannel_offset = initial_subchannel_offset;//subchannel_offset;
    wimaxHdr->phy_info.num_OFDMSymbol = txtime_s;
    wimaxHdr->phy_info.OFDMSymbol_offset = symbol_offset; //initial_offset;
    wimaxHdr->phy_info.channel_index = (mac_->getPeerNode_head())->getchannel();
    wimaxHdr->phy_info.direction = 1;

    //*debug10 ("\t   BW-REQ, In station %d, Packet phy header: numsymbols :%d, symboloffset :%d, subchanneloffset :%d, numsubchannel :%d, Channel_num :%d, direction :%d\n", mac_->getNodeType(), txtime_s,initial_offset, initial_subchannel_offset,numsubchannels,wimaxHdr->phy_info.channel_index,wimaxHdr->phy_info.direction);

    b->enqueue(p1);      //enqueue into burst
//    mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());
//    b_data = b_data+flag_bw_req;
//    c->setBW_REQ_QUEUE(BW_REQ_TIMEOUT);
//
    b_data += wimaxHdr->phy_info.num_subchannels*phy->getSlotCapacity(dlul_map_mod,UL_);
    int t16time = (ceil)((double)(mac_->macmib_.t16_timeout)/(double)(mac_->macmib_.frame_duration));
    c->setBW_REQ_QUEUE(t16time);

    initial_offset = offset;
    initial_subchannel_offset = subchannel_offset;

    if (mac_->getNodeType()!=STA_BS) {
       if ( mac_->getMap()->getUlSubframe()->getBw_req()->getRequest (c->get_cid()) != NULL ) {
//           mac_->getMap()->getUlSubframe()->getBw_req()->setTIMEOUT (c->get_cid(), BW_REQ_TIMEOUT);
           mac_->getMap()->getUlSubframe()->getBw_req()->setTIMEOUT (c->get_cid(), t16time);
           //*debug10("Reset CDMA_BW_REQ timer since send BW-REQ out for cid [%d].\n",c->get_cid());
       }
    }

    if (mac_->getNodeType()!=STA_BS) {
       if ( mac_->getMap()->getUlSubframe()->getBw_req()->getRequest (c->get_cid()) != NULL ) {
          Packet *p_tmp = NULL;
          FrameMap *map1 = mac_->getMap();
          Burst *b1;

          if ((p_tmp = mac_->getMap()->getUlSubframe()->getBw_req()->getPacket_P(c->get_cid())) != NULL) {
	     int flag_transmit = mac_->getMap()->getUlSubframe()->getBw_req()->getFLAGNOWTRANSMIT(c->get_cid());

             //debug10 ("Search flag_transmit :%d; remove cdma_bw_req if flag == 1, nbpdu :%d\n", flag_transmit, map1->getUlSubframe()->getNbPdu ());
	     if (flag_transmit == 1) {
		int break_frame = 0;
                for (int index = 0 ; index < map1->getUlSubframe()->getNbPdu (); index++) {
		    //debug10 ("Search index :%d for CDMA enqueue :%d\n", index, c->get_cid());
    		    b1 = map1->getUlSubframe()->getPhyPdu (index)->getBurst (0);
		    
    		    if (b1->getIUC()==UIUC_REQ_REGION_FULL) {
			int $index_p_burst = 0;
      			int q_len = map1->getUlSubframe()->getPhyPdu(index)->getBurst(0)->getQueueLength_packets();

      			while(q_len>0) {
              		   Packet *p_tmp_b = map1->getUlSubframe()->getPhyPdu(index)->getBurst(0)->lookup($index_p_burst);
			   q_len--;
			   $index_p_burst++;

        		   if (p_tmp_b) {
           			hdr_cmn* ch_tmp = HDR_CMN(p_tmp_b);
           			hdr_mac802_16 *wimaxHdr_send;
           			wimaxHdr_send = HDR_MAC802_16(p_tmp_b);
           			cdma_req_header_t *header_s = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp_b)->header);
				if (c->get_cid() == header_s->cid) {
         			   //debug10 ("\tFound enqueued cdma_bw_req in BURST (remove it) CID :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d\n", header_s->cid, wimaxHdr_send->phy_info.num_OFDMSymbol, wimaxHdr_send->phy_info.OFDMSymbol_offset, wimaxHdr_send->phy_info.num_subchannels, wimaxHdr_send->phy_info.subchannel_offset, header_s->code);
		        	   b1->remove(p_tmp_b);
	     			   mac_->getMap()->getUlSubframe()->getBw_req()->setFLAGNOWTRANSMIT(c->get_cid(), 0);
				   break_frame = 1;
				   break;
				} 
        		    }
      		        }
    		    }
		    if (break_frame == 1) break;
  	        }
             }
          }

//        debug10("\tRemove CDMA_BW_REQ (if exist)  since send BW-REQ out for cid [%d].\n",c->get_cid());
//      	mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());

       }

    }//end SS

//    return b_data;
  }
//end sending BW-REQ packet

  if (max_data < HDR_MAC802_16_SIZE ||
      (c->getFragmentationStatus()!=FRAG_NOFRAG && max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE))
    {
      //*debug2("In station %d returning as no space\n", mac_->getNodeType());
      return b_data; //not even space for header
    }

  if (flag_bw_req>0) {
      if ( (max_data-flag_bw_req) < HDR_MAC802_16_SIZE ||
      	 (c->getFragmentationStatus()!=FRAG_NOFRAG && (max_data-flag_bw_req) < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE)) {

         //*debug2("In station %d returning as no space\n", mac_->getNodeType());
         return b_data; //not even space for header
      }
  }

  int gm_flag = 0;
  int more_bw = 0;
#ifdef IF_PIG
//Piggybacking 1-May
  if (flag_pig > 0 ) {

     more_bw = c->queueByteLength() - max_data;
     if (mac_->getNodeType()!=STA_BS&&p) {
        ch = HDR_CMN(p);
        wimaxHdr = HDR_MAC802_16(p);

        //debug10("In station %d (SS) ch-size :%d, max_data :%d, queue_bytes :%d, queue_len :%d\n", mac_->getNodeType(), ch->size(),  max_data, c->queueByteLength(), c->queueLength());
        if (max_data < c->queueByteLength()) {
           wimaxHdr->header.type_fbgm = true;
           wimaxHdr->grant_subheader.piggyback_req = more_bw;
           //debug10 ("In station %d (SS) Piggybacking flag :%d, queue_bytes :%d, queue_len :%d, max_data :%d, askmore :%d\n", mac_->getNodeType(), wimaxHdr->header.type_fbgm, c->queueByteLength(), c->queueLength(), max_data, more_bw);
           gm_flag = 2;
        } else {
           wimaxHdr->header.type_fbgm = 0;
           //debug10 ("In station %d (SS) no gm :%d, maxdata :%d, queue :%d\n",  mac_->getNodeType(), wimaxHdr->header.type_fbgm, max_data, c->queueByteLength());
        }

    }
 }

//Remove cdma-bw-req since we send piggybacking
    if (mac_->getNodeType()!=STA_BS) {
       if ( mac_->getMap()->getUlSubframe()->getBw_req()->getRequest (c->get_cid()) != NULL ) {
          Packet *p_tmp = NULL;
          FrameMap *map1 = mac_->getMap();
          Burst *b1;

          if ((p_tmp = mac_->getMap()->getUlSubframe()->getBw_req()->getPacket_P(c->get_cid())) != NULL) {
	     int flag_transmit = mac_->getMap()->getUlSubframe()->getBw_req()->getFLAGNOWTRANSMIT(c->get_cid());

             //debug10 ("Search flag_transmit :%d; remove cdma_bw_req if flag == 1, nbpdu :%d\n", flag_transmit, map1->getUlSubframe()->getNbPdu ());
//	     debug10 ("p_tmp address :%d\n", p_tmp);
	     if (flag_transmit == 1) {
		int break_frame = 0;
                for (int index = 0 ; index < map1->getUlSubframe()->getNbPdu (); index++) {
		    //debug10 ("Search index :%d for CDMA enqueue :%d\n", index, c->get_cid());
    		    b1 = map1->getUlSubframe()->getPhyPdu (index)->getBurst (0);
		    
    		    if (b1->getIUC()==UIUC_REQ_REGION_FULL) {
			int $index_p_burst = 0;
      			int q_len = map1->getUlSubframe()->getPhyPdu(index)->getBurst(0)->getQueueLength_packets();

      			while(q_len>0) {
              		   Packet *p_tmp_b = map1->getUlSubframe()->getPhyPdu(index)->getBurst(0)->lookup($index_p_burst);
			   q_len--;
			   $index_p_burst++;

        		   if (p_tmp_b) {
  
//	     			debug10 ("p_tmp address from burst :%d\n", p_tmp_b);
           			hdr_cmn* ch_tmp = HDR_CMN(p_tmp_b);
           			hdr_mac802_16 *wimaxHdr_send;
           			wimaxHdr_send = HDR_MAC802_16(p_tmp_b);
           			cdma_req_header_t *header_s = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp_b)->header);
				if (c->get_cid() == header_s->cid) {
         			   //debug10 ("\tFound enqueued cdma_bw_req in BURST (remove it) CID :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d\n", header_s->cid, wimaxHdr_send->phy_info.num_OFDMSymbol, wimaxHdr_send->phy_info.OFDMSymbol_offset, wimaxHdr_send->phy_info.num_subchannels, wimaxHdr_send->phy_info.subchannel_offset, header_s->code);
		        	   b1->remove(p_tmp_b);
	     			   mac_->getMap()->getUlSubframe()->getBw_req()->setFLAGNOWTRANSMIT(c->get_cid(), 0);
				   break_frame = 1;
				   break;
				} 
        		    }
      		        }
    		    }
		    if (break_frame == 1) break;
  	        }
             }
          }

        //*debug10("\tRemove CDMA_BW_REQ if enqueued since send BW-REQ (PIGGYBACKING) out for cid [%d].\n",c->get_cid());
      	mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());

       }

    }//end SS
#endif

  // overloaded function added by rpi 

  while (p) {
    //debug10 ("In station %d Entering while loop, connection type %d, CID :%d\n", mac_->getNodeType(), c->get_category(), c->get_cid());
    ch = HDR_CMN(p);
    wimaxHdr = HDR_MAC802_16(p);
    if(ch->size() < 0 ) debug2(" packet size negative --- panic!!! "); 		

    if (mac_->getNodeType()==STA_BS)
      max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_) - b_data;
    else {
      max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_) - b_data - gm_flag;
    }

    if (max_data <= HDR_MAC802_16_SIZE ||
	(c->getFragmentationStatus()!=FRAG_NOFRAG && max_data <= HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE)) 
      return b_data; //not even space for header

    if (c->getFragmentationStatus()==FRAG_NOFRAG && max_data <= HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE && (ch->size()>HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE) ) {
      return b_data; //not even space for header 
    }

    if (c->getFragmentationStatus()!=FRAG_NOFRAG) 
      {
	if (max_data >= ch->size()-c->getFragmentBytes()+HDR_MAC802_16_FRAGSUB_SIZE)//getFrag => include MACHEADER 
	  {
	    //add fragmentation header
	    wimaxHdr->header.type_frag = true;
	    //no need to fragment again
	    wimaxHdr->frag_subheader.fc = FRAG_LAST;
	    wimaxHdr->frag_subheader.fsn = c->getFragmentNumber ();
            int more_bw = 0;
	   

	    c->dequeue();  /*remove packet from queue */
	    ch->size() = ch->size()-c->getFragmentBytes()+HDR_MAC802_16_FRAGSUB_SIZE; //new packet size
	    //update fragmentation
	    //*if(ch->size() < 0 ) debug2(" packet size negative -- panic !!! \n") ; 

            //debug2 ("\nEnd of fragmentation %d, CID :%d, (max_data :%d, bytes to send :%d, getFragmentBytes :%d, getFragNumber :%d, updated Frag :%d, update FragBytes :%d, con->qBytes :%d, con->qlen :%d, more_bw :%d)\n", wimaxHdr->frag_subheader.fsn, c->get_cid(), max_data, ch->size(), c->getFragmentBytes(), c->getFragmentNumber(), 0, 0, c->queueByteLength(), c->queueLength(), more_bw);
            
	    c->updateFragmentation (FRAG_NOFRAG, 0, 0);
	    if (gm_flag>0) { gm_flag = 0; ch->size() = ch->size() + HDR_PIG;}

	  }
	else {
	  //need to fragment the packet again
	  p = p->copy(); //copy packet to send
	  ch = HDR_CMN(p);
	  wimaxHdr = HDR_MAC802_16(p);
	  //add fragmentation header
	  wimaxHdr->header.type_frag = true;
	  wimaxHdr->frag_subheader.fc = FRAG_CONT;
	  wimaxHdr->frag_subheader.fsn = c->getFragmentNumber ();
          int more_bw = 0;
	  ch->size() = max_data; //new packet size

	  if (gm_flag>0) { gm_flag = 0; ch->size() = ch->size() + HDR_PIG;}
	  //update fragmentation

          //debug2 ("\nContinue fragmentation %d, CID :%d, (max_data :%d, bytes to send :%d, getFragmentBytes :%d, getFragNumber :%d, updated Frag :%d, update FragBytes :%d, con->qBytes :%d, con->qlen :%d, more_bw :%d)\n", wimaxHdr->frag_subheader.fsn, c->get_cid(), max_data, ch->size(),  c->getFragmentBytes(), c->getFragmentNumber(), (c->getFragmentNumber()+1)%8, c->getFragmentBytes()+max_data-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE), c->queueByteLength(), c->queueLength(), more_bw);

	  c->updateFragmentation (FRAG_CONT, (c->getFragmentNumber ()+1)%8, c->getFragmentBytes()+max_data-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
	}

      } else {//else no flag
      if (max_data < ch->size() && c->isFragEnable()) {
	//need to fragment the packet for the first time
	
	p = p->copy(); //copy packet to send
	ch = HDR_CMN(p);
	int ori_ch = ch->size();
	wimaxHdr = HDR_MAC802_16(p);
	//add fragmentation header
	wimaxHdr->header.type_frag = true;
	wimaxHdr->frag_subheader.fc = FRAG_FIRST;
	wimaxHdr->frag_subheader.fsn = c->getFragmentNumber ();
        int more_bw = 0;
	ch->size() = max_data; //new packet size

        //debug2 ("\nFirst fragmentation %d, CID :%d, (max_data :%d, bytes to send :%d, ori_size :%d, getFragmentBytes :%d, FRAGSUB :%d, getFragNumber :%d, updated Frag ;%d, update FragBytes :%d, con->qBytes :%d, con->qlen :%d, more_bw :%d)\n", wimaxHdr->frag_subheader.fsn, c->get_cid(), max_data, ch->size(), ori_ch, c->getFragmentBytes(), HDR_MAC802_16_FRAGSUB_SIZE, c->getFragmentNumber (), 1, c->getFragmentBytes()+max_data-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE),c->queueByteLength(), c->queueLength (), more_bw);

	c->updateFragmentation (FRAG_FIRST, 1, c->getFragmentBytes()+max_data-(HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE));
	if (gm_flag>0) { gm_flag = 0; ch->size() = ch->size() + HDR_PIG;}

      } else if (max_data < ch->size() && !c->isFragEnable()) {
	//the connection does not support fragmentation
	//can't move packets anymore
	return b_data;
      } else {
	//no fragmentation necessary
        int more_bw = 0;
	if (gm_flag>0) { gm_flag = 0; ch->size() = ch->size() + HDR_PIG;}

	//debug2 ("\nNo fragmentation %d, (max_data :%d, bytes to send :%d, con->qBytes :%d, con->qlen :%d, more_bw :%d\n", wimaxHdr->frag_subheader.fsn, max_data, ch->size(), c->queueByteLength(), c->queueLength(), more_bw);
	c->dequeue();

      }//end frag
    }

//    debug10 ("B_data1 :%d, ch->size :%d\n", b_data, ch->size());
//you did send something; reset BW_REQ_QUEUE
    c->setBW_REQ_QUEUE(0);

    if (mac_->getNodeType()!=STA_BS) {
       if ( mac_->getMap()->getUlSubframe()->getBw_req()->getRequest (c->get_cid()) != NULL ) {
          Packet *p_tmp = NULL;
          FrameMap *map1 = mac_->getMap();
          Burst *b1;

          if ((p_tmp = mac_->getMap()->getUlSubframe()->getBw_req()->getPacket_P(c->get_cid())) != NULL) {
	     int flag_transmit = mac_->getMap()->getUlSubframe()->getBw_req()->getFLAGNOWTRANSMIT(c->get_cid());

             //debug10 ("Search flag_transmit :%d; remove cdma_bw_req if flag == 1, nbpdu :%d\n", flag_transmit, map1->getUlSubframe()->getNbPdu ());
//	     debug10 ("p_tmp address :%d\n", p_tmp);
	     if (flag_transmit == 1) {
		int break_frame = 0;
                for (int index = 0 ; index < map1->getUlSubframe()->getNbPdu (); index++) {
		    //debug10 ("Search index :%d for CDMA enqueue :%d\n", index, c->get_cid());
    		    b1 = map1->getUlSubframe()->getPhyPdu (index)->getBurst (0);
		    
    		    if (b1->getIUC()==UIUC_REQ_REGION_FULL) {
			int $index_p_burst = 0;
      			int q_len = map1->getUlSubframe()->getPhyPdu(index)->getBurst(0)->getQueueLength_packets();

      			while(q_len>0) {
              		   Packet *p_tmp_b = map1->getUlSubframe()->getPhyPdu(index)->getBurst(0)->lookup($index_p_burst);
			   q_len--;
			   $index_p_burst++;

        		   if (p_tmp_b) {
  
//	     			debug10 ("p_tmp address from burst :%d\n", p_tmp_b);
           			hdr_cmn* ch_tmp = HDR_CMN(p_tmp_b);
           			hdr_mac802_16 *wimaxHdr_send;
           			wimaxHdr_send = HDR_MAC802_16(p_tmp_b);
           			cdma_req_header_t *header_s = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp_b)->header);
				if (c->get_cid() == header_s->cid) {
         			   //debug10 ("\tFound enqueued cdma_bw_req in BURST (remove it) CID :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d\n", header_s->cid, wimaxHdr_send->phy_info.num_OFDMSymbol, wimaxHdr_send->phy_info.OFDMSymbol_offset, wimaxHdr_send->phy_info.num_subchannels, wimaxHdr_send->phy_info.subchannel_offset, header_s->code);
		        	   b1->remove(p_tmp_b);
	     			   mac_->getMap()->getUlSubframe()->getBw_req()->setFLAGNOWTRANSMIT(c->get_cid(), 0);
				   break_frame = 1;
				   break;
				} 
        		    }
      		        }
    		    }
		    if (break_frame == 1) break;
  	        }
             }
          }

        //debug10("\tRemove CDMA_BW_REQ (if exist) since send some packet out for cid [%d].\n",c->get_cid());
      	mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());

       }

    }//end SS

//    debug10 ("B_data2 :%d, ch->size :%d\n", b_data, ch->size());
    int assigned_subchannels;
    int symbol_offset;
    //Trying to calculate how to fill up things here? Before this, frag and arq business?

/*Richard fixed*/

    if (mac_->getNodeType()==STA_BS) 
      {
	assigned_subchannels = phy->getNumSubchannels(b_data, mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_);
	numsubchannels = phy->getNumSubchannels(ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_);
	//debug10("Richard (BS) is going to use [%d] subchannels.\n",numsubchannels);

	Ofdm_mod_rate dlul_map_mod = mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding();
	int num_of_slots = (int) ceil((double)ch->size()/phy->getSlotCapacity(dlul_map_mod,DL_)); //get the slots needs.
	numsubchannels = num_of_slots;
	//*debug10(" (BS) is going to use [%d] subchannels.\n",numsubchannels);
	symbol_offset = b->getStarttime() + num_symbol_per_slot * ((b->getSubchannelOffset()+assigned_subchannels)/ phy->getNumsubchannels(DL_));

	//debug2("number subchannel before calculation is[%d]\t assigned_subchn[%d]\t subch_off[%d]\t PerSYmbolSubch#[%d] %f\n",
	       //numsubchannels,assigned_subchannels, subchannel_offset, phy->getNumsubchannels(DL_),
	       //ceil((double)(numsubchannels + subchannel_offset) / phy->getNumsubchannels(DL_)));
	
	txtime_s = (int)(ceil((double)(numsubchannels + subchannel_offset) / (double)phy->getNumsubchannels(DL_))) * num_symbol_per_slot;
	offset = b->getStarttime( )+ txtime_s-num_symbol_per_slot;
	subchannel_offset = (int) ceil((numsubchannels + subchannel_offset) % (phy->getNumsubchannels(DL_)));
	
      }
    else { //if its the SS

      assigned_subchannels = phy->getNumSubchannels(b_data, mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);

      numsubchannels = phy->getNumSubchannels(ch->size(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);
      Ofdm_mod_rate dlul_map_mod = mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding();
      int num_of_slots = (int) ceil((double)ch->size()/phy->getSlotCapacity(dlul_map_mod,UL_)); //get the slots needs.
      int numsubchannel_xingting = num_of_slots;
      //*debug10(" (SS) packet size[%d]\t Richard calculation[%d]\t Vs. xingting calculation[%d]\n", ch->size(), numsubchannels,numsubchannel_xingting);
	symbol_offset = b->getStarttime() + num_symbol_per_slot * ((b->getSubchannelOffset()+assigned_subchannels)/ phy->getNumsubchannels(UL_));

      //debug2("number subchannel before calculation is[%d]\t assigned_subchn[%d]\t subch_off[%d]\t PerSYmbolSubch#[%d] %f\n",
	     //numsubchannels,assigned_subchannels, subchannel_offset, phy->getNumsubchannels(UL_),
	     //ceil((double)(numsubchannels + subchannel_offset) / phy->getNumsubchannels(UL_)));
      
      txtime_s = (int)(ceil((double)(numsubchannels + subchannel_offset) / (double)phy->getNumsubchannels(UL_)))*num_symbol_per_slot;
      offset = b->getStarttime( )+ txtime_s-num_symbol_per_slot; 
      subchannel_offset = (int) ceil((numsubchannels + subchannel_offset) % (phy->getNumsubchannels(UL_)));

    }
    //printf ("txtime_s=%d, symbol time = %f txtime=%f\n", txtime_s, phy->getSymbolTime (), txtime_s*phy->getSymbolTime ());

    ch->txtime() = txtime_s*phy->getSymbolTime ();

    //debug2 ("In station %d Check packet to send: con id in burst = %d con id in connection = %d size=%d txtime=%f(%d) duration=%d(%f) numsubchnn = %d subchnoff = %d\n",
	    //mac_->getNodeType(), c->get_cid(),b->getCid(), ch->size(),ch->txtime(), txtime_s, b->getDuration(), b->getDuration()*phy->getSymbolTime (),b->getnumSubchannels(),b->getSubchannelOffset ());

    wimaxHdr = HDR_MAC802_16(p);
    wimaxHdr->phy_info.num_subchannels = numsubchannels;
    wimaxHdr->phy_info.subchannel_offset = initial_subchannel_offset;//subchannel_offset;
    wimaxHdr->phy_info.num_OFDMSymbol = txtime_s;
    wimaxHdr->phy_info.OFDMSymbol_offset = symbol_offset; //initial_offset;

    //debug10("In transfer_packets1-- packets info: symbol_off[%d]\t symbol_#[%d]\t subch_Off[%d]\t subch_#[%d]\n",
	    //wimaxHdr->phy_info.OFDMSymbol_offset,wimaxHdr->phy_info.num_OFDMSymbol,
	    //wimaxHdr->phy_info.subchannel_offset,wimaxHdr->phy_info.num_subchannels);

    //wimaxHdr->phy_info.channel_index = c->getPeerNode()->getchannel();
    if (mac_->getNodeType()==STA_BS)
      {
	wimaxHdr->phy_info.direction = 0;
	if(c->getPeerNode())
	{
		wimaxHdr->phy_info.channel_index = c->getPeerNode()->getchannel();
	}
      }
    else
      {
	wimaxHdr->phy_info.channel_index = (mac_->getPeerNode_head())->getchannel();     
	wimaxHdr->phy_info.direction = 1;
      }

    //debug2("In station %d Packet phy header : numsymbols =%d , symboloffset =%d,  subchanneloffset= %d, numsubchannel = %d, Channel_num = %d, direction =%d \n", 
	   //mac_->getNodeType(), txtime_s,initial_offset, initial_subchannel_offset,numsubchannels,wimaxHdr->phy_info.channel_index,wimaxHdr->phy_info.direction);

    initial_offset = offset;
    initial_subchannel_offset = subchannel_offset;

    //  rpi end put phy header info - like num subchannels and ofdmsymbols.... 
//    debug10 ("B_data3 :%d, ch->size :%d\n", b_data, ch->size());

    b->enqueue(p);      //enqueue into burst
    Ofdm_mod_rate dlul_map_mod;

    if (mac_->getNodeType()==STA_BS) {
      dlul_map_mod = mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding();
    } else {
      dlul_map_mod = mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding();
    }
    b_data += wimaxHdr->phy_info.num_subchannels*phy->getSlotCapacity(dlul_map_mod,UL_);

//    b_data += ch->size(); //increment amount of data enqueued
    //debug2 ("In station %d packet enqueued b_data = %d \n", mac_->getNodeType(), b_data);

    if (!pkt_transfered && mac_->getNodeType()!=STA_BS)
      { //if we transfert at least one packet, remove bw request
	pkt_transfered = true;
	mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());
      }

    p = c->get_queue()->head(); //get new head

  }
  return b_data;
}

/**
 * Transfer the packets from the given connection to the given burst
 * @param con The connection
 * @param b The burst
 * @param b_data Amount of data in the burst
 * @return the new burst occupation
 */
int WimaxScheduler::transfer_packets_with_fragpackarq(Connection *c, Burst *b, int b_data)
{
  Packet *p, *mac_pdu, *p_temp, *p_current;
  hdr_cmn* ch, *ch_pdu, *ch_temp, *ch_current;
  hdr_mac802_16 *wimaxHdr, *wimaxHdr_pdu, *wimaxHdr_temp, *wimaxHdr_current;
  double txtime;
  int txtime_s;
  bool pkt_transfered = false;
  OFDMAPhy *phy = mac_->getPhy();
  int offset = b->getStarttime( );
  PeerNode *peer;
  int numsubchannels,subchannel_offset; 
  int initial_offset = b->getStarttime( );
  int initial_subchannel_offset = b->getSubchannelOffset ();
  int bw_req_packet = GENERIC_HEADER_SIZE;
  int flag_bw_req = 0;

  int num_symbol_per_slot;
  if (mac_->getNodeType()==STA_BS) 
  {
    num_symbol_per_slot = phy->getSlotLength (DL_); 
  } 
  else 
  {
    num_symbol_per_slot = phy->getSlotLength (UL_); 
  }

  //*debug2 ("Entered transfer packets with ARQ for connection %d with bdata = %d\n ", c->get_cid(), b_data);
  peer = mac_->getPeerNode_head();
  //*debug2("ARQ Transfer_Packets with Frag pack arq\n");

    /*Firstly, check the retransmission queue status. */
	if (c->getArqStatus () != NULL && c->getArqStatus ()->isArqEnabled() == 1)
	{
		if((c->getArqStatus ()->arq_retrans_queue_) && (c->getArqStatus ()->arq_retrans_queue_->length() > 0))
		{
			//*debug2("ARQ Transfer Packets: Transferring a Retransmission packet\n");
			mac_pdu = c->getArqStatus ()->arq_retrans_queue_->head();
		}
		else
			mac_pdu = c->get_queue()->head();			
	} 
	else  /*If ARQ is not supported, directly get the header packet of the queue.*/
		mac_pdu = c->get_queue()->head();
 
	if(!mac_pdu)
	{
		//*debug2("ARQ: NO Data present to be sent\n");
		return b_data;
	}

	int max_data;
	wimaxHdr_pdu= HDR_MAC802_16(mac_pdu);
	ch_pdu = HDR_CMN(mac_pdu);
	//printf("this brust has [%d] of subchannels.\n",b->getnumSubchannels());

	int arq_block_size = ch_pdu->size ();

	/*get the maximum size of the data which could be transmitted using this data burst without changing anything.*/
	if (mac_->getNodeType()==STA_BS)
	{
		max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_) - b_data;    
		//*debug2 ("ARQ: In Mac %d max data=%d (b_data=%d) ch_pdu size=%d\n", mac_->addr(), max_data, b_data,ch_pdu->size ());
	}
	else
	{
		max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_) - b_data;    
		//*debug2 ("ARQ: In Mac %d max data=%d (b_data=%d) ch_pdu size=%d\n", mac_->addr(), max_data, b_data,ch_pdu->size ());

		/*If this data burst could hold one BW REQ message and could not hold even one ARQ block. We need sponser a BW REQ message.*/
		if ( (max_data >= bw_req_packet) && (max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE+ch_pdu->size ()) ) 
		{
			//*debug10 ("\t   CheckBW-REQ, SS Should generate BW-REQ, maxdata :%d, bw-req-size :%d, queue :%d\n",
					//*max_data, bw_req_packet, c->queueByteLength());
			flag_bw_req = 1;
		} 
		else /*Otherwise, this data burst could hold the all the ARQ block or even could not hold a BW REQ message, we donot need a 
		       BW REQ.*/
		{
			flag_bw_req = 0;
		}
	}


//#ifdef WIMAX_TEST
	/*This data burst could not hold even a BW REQ message, then we simply return. Maybe this will be a dead-end loop?????*/
	if (max_data < HDR_MAC802_16_SIZE ||
      (c->getFragmentationStatus()!=FRAG_NOFRAG && max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE))
    {
      //*debug2("In station %d returning as no space\n", mac_->getNodeType());
      return b_data; //not even space for header
    }

     /*If this data burst could hold a BW REQ, we fill out the necessary info. to ask for more BW.*/
	if (flag_bw_req == 1) 
	{
		Packet *p1= mac_->getPacket();
		hdr_cmn* ch = HDR_CMN(p1);
		hdr_mac802_16 *wimaxHdr;

		wimaxHdr = HDR_MAC802_16(p1);

		bw_req_header_t *header = (bw_req_header_t *)&(HDR_MAC802_16(p1)->header);
		header->ht=1;
		header->ec=1;
		header->type = 0x1; //aggregate
		//header->br = c->queueByteLength()+HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE;  /*set the BW we need.*/
		header->br = c->getArqStatus ()->arq_retrans_queue_->byteLength() +HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE;  /*set the BW we need.*/
		header->cid = c->get_cid();
		//*debug10 ("\t   Sending BW-REQ, cid :%d, qBytes :%d \n", header->cid, header->br);

		int assigned_subchannels;
		int symbol_offset;
		ch->size() = bw_req_packet;

		assigned_subchannels = phy->getNumSubchannels(b_data, mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);
		numsubchannels = phy->getNumSubchannels(ch->size(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);

		/*old code Chakchai*/
		symbol_offset = b->getStarttime() + (b->getSubchannelOffset()+assigned_subchannels)/ phy->getNumsubchannels(UL_);

		if ((numsubchannels + assigned_subchannels) > (phy->getNumsubchannels(UL_) - b->getSubchannelOffset () + 1))
		{
			txtime_s = num_symbol_per_slot + (int) ((ceil((numsubchannels + assigned_subchannels - (phy->getNumsubchannels(UL_) - b->getSubchannelOffset () + 1)) / (double)phy->getNumsubchannels(UL_)))) * num_symbol_per_slot ;
			offset = b->getStarttime( )+ txtime_s-num_symbol_per_slot;
			subchannel_offset = (int) ceil((numsubchannels + assigned_subchannels - (phy->getNumsubchannels(UL_) - b->getSubchannelOffset () + 1)) % (phy->getNumsubchannels(UL_)));
			subchannel_offset++;
		} 
		else
		{
			subchannel_offset = numsubchannels + assigned_subchannels;
			txtime_s=num_symbol_per_slot;
		}

		ch->txtime() = txtime_s*phy->getSymbolTime ();

		//*debug2 ("\t   In station %d, Check packet to send: con id in burst :%d, con id in connection :%d, size :%d, txtime :%f(%d), duration :%d(%f), numsubchnn :%d, subchnoff :%d\n",
				//*mac_->getNodeType(), c->get_cid(),b->getCid(), ch->size(),txtime, txtime_s, b->getDuration(), b->getDuration()*phy->getSymbolTime (),b->getnumSubchannels(),b->getSubchannelOffset ());

		wimaxHdr->phy_info.num_subchannels = numsubchannels;
		wimaxHdr->phy_info.subchannel_offset = initial_subchannel_offset;//subchannel_offset;
		wimaxHdr->phy_info.num_OFDMSymbol = txtime_s;
		wimaxHdr->phy_info.OFDMSymbol_offset = symbol_offset; //initial_offset;
		wimaxHdr->phy_info.channel_index = (mac_->getPeerNode_head())->getchannel();
		wimaxHdr->phy_info.direction = 1;

		//*debug2 ("\t   In station %d, Packet phy header: numsymbols :%d, symboloffset :%d, subchanneloffset :%d, numsubchannel :%d, Channel_num :%d, direction :%d\n", mac_->getNodeType(), txtime_s,initial_offset, initial_subchannel_offset,numsubchannels,wimaxHdr->phy_info.channel_index,wimaxHdr->phy_info.direction);

		initial_offset = offset;
		initial_subchannel_offset = subchannel_offset;

		b->enqueue(p1);      //enqueue into burst
		// mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());

		return b_data;
	}
//#endif



	// Frag/pack subheader size is always equal to 2 bytes
	// For each MAC PDU Created, there is definitely MAC Header and Packet Sub Header and at least one ARQ Block
	//if (max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE+ch_pdu->size ()) 
	//*debug2("ARQ retrans :The max_data is %d\n", max_data);
	if (max_data < HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE) 
	{
		//*debug2("ARQ: Not enough space even for one block and headers.\n");
		return b_data; //not even space for one block and headers
	}
	
	//If the Current window for ARQ is less than one, cannot transmit the packet
	//*debug2("ARQ1.0 Transfer_Packets PDU: Current window : %d , FSN: %d\n", (((c->getArqStatus()->getAckSeq()) + (c->getArqStatus()->getMaxWindow())) &0x7FF),(wimaxHdr_pdu->pack_subheader.sn));
	int seqno = wimaxHdr_pdu->pack_subheader.sn;
	int ack_seq = (c->getArqStatus()->getAckSeq());
	int max_window = (c->getArqStatus()->getMaxWindow()); 	
	int max_seq =  (c->getArqStatus()->getMaxSeq());	
	int curr_window = (((c->getArqStatus()->getAckSeq()) + (c->getArqStatus()->getMaxWindow())) &0x7FF);  // Get the right most of the ARQ slide window point

	//*debug2("xingting== seqno=%d,  ack_seq=%d,  max_window=%d,   max_seq=%d,  curr_window=%d\n",
					//*seqno,ack_seq,max_window, max_seq,curr_window);
	/*if the current ARQ block is outside the ARQ slide window, do not transfer it.*/
	//if((((seqno - ack_seq) > 0) && ((seqno - ack_seq) > max_window)) 
	if((((seqno - ack_seq) > 0) && ((seqno - ack_seq) >= max_window)) 
		//|| ((seqno >= curr_window && seqno < max_window) && (ack_seq < max_seq && ack_seq >= (max_seq - max_window)))) 
		||((seqno-ack_seq)<0 && ((seqno+max_seq-ack_seq)>=max_window)))
	{
		//*debug2("ARQ1.0 Transfer_Packets: Current window Low :curr_window_ : %d\n", (((c->getArqStatus()->getAckSeq()) + (c->getArqStatus()->getMaxWindow())) &0x7FF));
		//*debug2("ARQ1.0 Transfer_Packets: this ARQ block is outside the slide window. will not be transferred.\n");
		return b_data;
	}


  // MAC PDU will be the final packet to be transmitted 
  //mac_pdu = mac_pdu->copy();
	wimaxHdr_pdu= HDR_MAC802_16(mac_pdu);
	ch_pdu = HDR_CMN(mac_pdu);

#if 0
	int gm_flag = 0;
	ch = HDR_CMN(p);
	wimaxHdr = HDR_MAC802_16(p);

	int more_bw = c->queueByteLength() - max_data + HDR_MAC802_16_SIZE + HDR_MAC802_16_FRAGSUB_SIZE;
	if (mac_->getNodeType()==STA_BS) 
	{
	//     wimaxHdr->header.type_fbgm = false;
	}
	else 
	{
		 printf("SS ch-size :%d, max_data :%d, queue :%d\n", ch->size(),  max_data, c->queueByteLength());
        if (max_data < c->queueByteLength()+ GENERIC_HEADER_SIZE + HDR_MAC802_16_FRAGSUB_SIZE) 
        {
             wimaxHdr->header.type_fbgm = true;
             wimaxHdr->grant_subheader.piggyback_req = more_bw;
             ch->size() = ch->size()+HDR_MAC802_16_FRAGSUB_SIZE; //new packet size
             debug10 ("In station %d Piggybacking flag :%d, queue_bytes :%d, max_data :%d, askmore :%d\n", mac_->getNodeType(), wimaxHdr->header.type_fbgm, c->queueByteLength(), max_data, more_bw);
             gm_flag = 2;
        } 
        else 
        {
             wimaxHdr->header.type_fbgm = false;
             printf("SS no gm :%d, maxdata :%d, queue :%d\n", wimaxHdr->header.type_fbgm, max_data, c->queueByteLength());
        }
    }
 #endif

 
	//We can remove the packet from the queue now
	if (c->getArqStatus () != NULL && c->getArqStatus()->isArqEnabled() == 1)
	{
		if((c->getArqStatus ()->arq_retrans_queue_) && (c->getArqStatus ()->arq_retrans_queue_->length() > 0))
		{
			//*debug2("ARQ Transfer Packets: Transferring a Retransmission packet\n");	
			p = c->getArqStatus ()->arq_retrans_queue_->deque();
		}	
		else
			p = c->dequeue();			
	} 
	else  
		p = c->dequeue();

  //set header information
  wimaxHdr_pdu->header.ht = 0;
  wimaxHdr_pdu->header.ec = 1;
  wimaxHdr_pdu->header.type_mesh = 0;
  wimaxHdr_pdu->header.type_ext = 1; /*ARQ is enabled */
  wimaxHdr_pdu->header.type_frag = 0;
  wimaxHdr_pdu->header.type_pck = 1; /*Packing header is present */ 
  wimaxHdr_pdu->header.type_fbgm = 0;
  wimaxHdr_pdu->header.ci = 1;
  wimaxHdr_pdu->header.eks = 0;
  wimaxHdr_pdu->header.cid = c->get_cid ();
  wimaxHdr_pdu->header.hcs = 0;
  ch_pdu->size() += HDR_MAC802_16_SIZE+HDR_MAC802_16_FRAGSUB_SIZE+HDR_MAC802_16_CRC_SIZE; /*Size of generic Mac Header and CRC is set (Note one packet header and first ARQ Block is present)*/
  //*debug2 ("ARQ: MAC [%d] Generated PDU Size: %d FSN is %d \n",mac_->addr(),ch_pdu->size (),  wimaxHdr_pdu->pack_subheader.sn);

  // Set the Initial Conditions
  p_temp = mac_pdu;	
  wimaxHdr_temp= HDR_MAC802_16(mac_pdu);
  ch_temp = HDR_CMN(mac_pdu);
  //*debug2 ("ARQ: MAX_data %d \t MAC %d PDU: ARQ Fragment Added... PDU Size: %d FSN is %d re-trans-Q len is %d  data-trans-Q len is %d\n",
				//*max_data, mac_->addr(),ch_temp->size (),  wimaxHdr_temp->pack_subheader.sn, c->getArqStatus ()->arq_retrans_queue_->length()+1, c->get_queue()->length()+1);

  if (c->getArqStatus () != NULL && c->getArqStatus ()->isArqEnabled() == 1)
    {
      if((c->getArqStatus ()->arq_retrans_queue_) && (c->getArqStatus ()->arq_retrans_queue_->length() > 0)){
	//*debug2("ARQ Transfer Packets: Transferring a Retransmission packet\n");
	p = c->getArqStatus ()->arq_retrans_queue_->head();
      }
      else
        p = c->get_queue()->head();			
    } 
  else  
    p = c->get_queue()->head();
  
	while (p)
	{
		ch = HDR_CMN(p);
		wimaxHdr = HDR_MAC802_16(p);

		if (mac_->getNodeType()==STA_BS)
			max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_) - b_data;    
		else
			max_data = phy->getMaxPktSize (b->getnumSubchannels(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_) - b_data;  

		//*debug2 ("ARQ : In Mac %d max data=%d (burst duration=%d, b_data=%d) PDU SIZE = %d\n", mac_->addr(), max_data, b->getDuration(), b_data, ch_pdu->size());

		if((wimaxHdr->pack_subheader.fc == FRAG_FIRST) 
			|| (((wimaxHdr->pack_subheader.sn - wimaxHdr_temp->pack_subheader.sn) &0x7FF)!= 1) 
			|| (wimaxHdr->pack_subheader.fc == FRAG_NOFRAG))
		{   
			if (max_data < ch_pdu->size()+ ch->size()+HDR_MAC802_16_FRAGSUB_SIZE)  // why at less need to fit in 2 blocks?
			{
				//*debug2("Can not fit in 2 blocks + frag Subheader in this data burst.\n");
				break; //cannot fit in more
			}
		} 
		else
		{   
			if (max_data < ch_pdu->size()+ ch->size())   // why at less need to fit in 2 blocks??
			{
				//*debug2("Can not fit in 2 blocks in this data burst.\n");
				break; //cannot fit in more
			}
		} 

    // If the Current window for ARQ is less than one, cannot transmit the packet
    //*debug2("ARQ2.0 Transfer_Packets: Current window : %d , FSN: %d\n", (((c->getArqStatus()->getAckSeq()) + (c->getArqStatus()->getMaxWindow())) &0x7FF),(wimaxHdr->pack_subheader.sn));
    int seqno = wimaxHdr->pack_subheader.sn;
    int ack_seq = (c->getArqStatus()->getAckSeq());
    int max_window = (c->getArqStatus()->getMaxWindow());
    int max_seq =  (c->getArqStatus()->getMaxSeq());	
    int curr_window = (((c->getArqStatus()->getAckSeq()) + (c->getArqStatus()->getMaxWindow())) &0x7FF);
    if((((seqno - ack_seq) > 0) && ((seqno - ack_seq) > max_window)) 
	    //|| ((seqno >= curr_window && seqno < max_window) && (ack_seq < max_seq && ack_seq >= (max_seq - max_window)))) 
	    ||((seqno-ack_seq)<0 && ((seqno+max_seq-ack_seq)>max_window)))
    {	 	
  		//*printf("ARQ2.0 Transfer_Packets: seqno=%d, ack_seq=%d, 1st judge=%d,  2nd judge=%d",
					//*seqno,ack_seq,(((seqno - ack_seq) > 0) && ((seqno - ack_seq) > max_window)),((seqno-ack_seq)<0 && ((seqno+max_seq-ack_seq)>max_window)));
      //*debug2("ARQ2.0 Transfer_Packets: this ARQ block is outside of ARQ slide window, Will not be transferred.\n ");
      break;
    }

    p_temp->allocdata (sizeof (class Packet));
    p_current = (Packet*) p_temp->accessdata();
    
    HDR_CMN(p)->direction() = hdr_cmn::UP; //to be received properly

    // Copies the entire contents of the current packet into data part of the previous packet 
    memcpy(p_current, p, sizeof (class Packet));
    ch_current = HDR_CMN(p_current);
    wimaxHdr_current = HDR_MAC802_16(p_current);

    // Update the Mac PDU Size
    if((wimaxHdr->pack_subheader.fc == FRAG_FIRST) || (((wimaxHdr->pack_subheader.sn - wimaxHdr_temp->pack_subheader.sn) &0x7FF)!= 1)|| (wimaxHdr->pack_subheader.fc == FRAG_NOFRAG))
      ch_pdu->size() += ch->size()+HDR_MAC802_16_FRAGSUB_SIZE ;  	
    else
      ch_pdu->size() += ch->size();

    //*debug2 ("ARQ: MAC PDU: ARQ Fragment Added... PDU Size: %d FSN is %d \n",ch_pdu->size (),  wimaxHdr_current->pack_subheader.sn);
    
    // Set the Conditions
    p_temp = p_current;	
    wimaxHdr_temp= wimaxHdr_current;
    ch_temp = ch_current;

    // Remove the packet from the queue
    if (c->getArqStatus () != NULL && c->getArqStatus()->isArqEnabled() == 1)
      {
	if((c->getArqStatus ()->arq_retrans_queue_) && (c->getArqStatus ()->arq_retrans_queue_->length() > 0)){
	  //*debug2("ARQ Transfer Packets: Transferring a Retransmission packet\n");	
	  p = c->getArqStatus ()->arq_retrans_queue_->deque();
	}	
	else
          p = c->dequeue();			
      } 
    else  
      p = c->dequeue();

    // Fetch the next packet
    if (c->getArqStatus () != NULL && c->getArqStatus ()->isArqEnabled() == 1)
      {
	if((c->getArqStatus ()->arq_retrans_queue_) && (c->getArqStatus ()->arq_retrans_queue_->length() > 0)){
	  //*debug2("ARQ Transfer Packets: Transferring a Retransmission packet\n");
	  p = c->getArqStatus ()->arq_retrans_queue_->head();
	}
	else
	  p = c->get_queue()->head();			
      } 
    else  
      p = c->get_queue()->head();   				
  }
  //*debug2 ("ARQ: MAC PDU Generated Size : %d \n", ch_pdu->size());
  int assigned_subchannels;
  int symbol_offset;
  //Trying to calculate how to fill up things here? Before this, frag and arq business?
  if (mac_->getNodeType()==STA_BS) {
    assigned_subchannels = phy->getNumSubchannels(b_data, mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_);
    numsubchannels = phy->getNumSubchannels(ch_pdu->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_);
    //RPI not working
    //      symbol_offset = b->getStarttime() + (b->getSubchannelOffset()+assigned_subchannels)/numsubchannels;
    //Richard fixed it.
    symbol_offset = b->getStarttime() + (b->getSubchannelOffset()+assigned_subchannels)/ phy->getNumsubchannels(DL_);

    if((numsubchannels+assigned_subchannels) > (phy->getNumsubchannels(DL_) - b->getSubchannelOffset () + 1)){ //its crossing over to the next symbol
      txtime_s = num_symbol_per_slot + (int) ((ceil((numsubchannels + assigned_subchannels - (phy->getNumsubchannels(DL_) - b->getSubchannelOffset () + 1)) / (double)phy->getNumsubchannels(DL_)))) * num_symbol_per_slot;
      offset = b->getStarttime( )+ txtime_s-num_symbol_per_slot;
      subchannel_offset = (int) ceil((numsubchannels + assigned_subchannels - (phy->getNumsubchannels(DL_) - b->getSubchannelOffset () + 1)) % (phy->getNumsubchannels(DL_)));
      subchannel_offset++;
    }  
    else{
      txtime_s=num_symbol_per_slot; 
      subchannel_offset = numsubchannels + assigned_subchannels;
    }
  }
  else { //if its the SS
    //RPI not working
    //      assigned_subchannels = phy->getNumSubchannels(b_data, mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), DL_);
    //Richard fixed it.
    assigned_subchannels = phy->getNumSubchannels(b_data, mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);

    numsubchannels = phy->getNumSubchannels(ch_pdu->size(), mac_->getMap()->getUlSubframe()->getProfile (b->getIUC())->getEncoding(), UL_);
    //RPI not working
    //      symbol_offset = b->getStarttime() + (b->getSubchannelOffset()+assigned_subchannels)/numsubchannels;
    //Richard fixed it.
    symbol_offset = b->getStarttime() + (b->getSubchannelOffset()+assigned_subchannels)/ phy->getNumsubchannels(UL_);

    if((numsubchannels + assigned_subchannels) > (phy->getNumsubchannels(UL_) - b->getSubchannelOffset () + 1)){
      txtime_s = num_symbol_per_slot + (int) ((ceil((numsubchannels + assigned_subchannels - (phy->getNumsubchannels(UL_) - b->getSubchannelOffset () + 1)) / (double)phy->getNumsubchannels(UL_)))) * num_symbol_per_slot ; 
      offset = b->getStarttime( )+ txtime_s-num_symbol_per_slot; 
      subchannel_offset = (int) ceil((numsubchannels + assigned_subchannels - (phy->getNumsubchannels(UL_) - b->getSubchannelOffset () + 1)) % (phy->getNumsubchannels(UL_)));
      subchannel_offset++;
    }
    else{
      subchannel_offset = numsubchannels + assigned_subchannels;
      txtime_s=num_symbol_per_slot;
    }
  }

  ch_pdu->txtime() = txtime_s*phy->getSymbolTime ();

  //*debug10 ("In station %d Check packet to send: con id in burst = %d con id in connection = %d size=%d txtime=%f(%d) duration=%d(%f) numsubchnn = %d subchnoff = %d\n",mac_->getNodeType(), c->get_cid(),b->getCid(), ch_pdu->size(),txtime, txtime_s, b->getDuration(), b->getDuration()*phy->getSymbolTime (),b->getnumSubchannels(),b->getSubchannelOffset ());
   
  // rpi start put phy header info - like num subchannels and ofdmsymbols.... 

  wimaxHdr_pdu->phy_info.num_subchannels = numsubchannels;
  wimaxHdr_pdu->phy_info.subchannel_offset = initial_subchannel_offset;//subchannel_offset;
  wimaxHdr_pdu->phy_info.num_OFDMSymbol = txtime_s;
  wimaxHdr_pdu->phy_info.OFDMSymbol_offset = symbol_offset; //initial_offset;
  if (mac_->getNodeType()==STA_BS)
    {
      wimaxHdr_pdu->phy_info.direction = 0;
      wimaxHdr_pdu->phy_info.channel_index = c->getPeerNode()->getchannel();
    }
  else
    {
      wimaxHdr_pdu->phy_info.channel_index = (mac_->getPeerNode_head())->getchannel();     
      wimaxHdr_pdu->phy_info.direction = 1;
    }
  
  //*debug10 ("In station %d Packet phy header : numsymbols =%d , symboloffset =%d,  subchanneloffset= %d, numsubchannel = %d, Channel_num = %d, direction =%d \n", mac_->getNodeType(), txtime_s,initial_offset, initial_subchannel_offset,numsubchannels,wimaxHdr_pdu->phy_info.channel_index,wimaxHdr_pdu->phy_info.direction);
        
 	 
  initial_offset = offset;
  initial_subchannel_offset = subchannel_offset;
 
  b->enqueue(mac_pdu);      //enqueue into burst
  b_data += ch_pdu->size(); //increment amount of data enqueued
  //*debug2 ("In station %d packet enqueued b_data = %d" , mac_->getNodeType(), b_data);
  if (!pkt_transfered && mac_->getNodeType()!=STA_BS){ //if we transfert at least one packet, remove bw request
    pkt_transfered = true;
    mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (c->get_cid());
  }

  return b_data;
}


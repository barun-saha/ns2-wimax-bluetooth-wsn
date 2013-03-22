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

#include "ssscheduler.h"
#include "burst.h"
#include "mac802_16SS.h"

int frame_no = 0;
/**
 * Tcl hook for creating SS scheduler 
 */
static class SSschedulerClass : public TclClass {
public:
  SSschedulerClass() : TclClass("WimaxScheduler/SS") {}
  TclObject* create(int, const char*const*) {
    return (new SSscheduler());
    
  }
} class_ssscheduler;

/*
 * Create a scheduler
 */
SSscheduler::SSscheduler () : WimaxScheduler ()
{
  //*debug2 ("SSscheduler created\n");
}

/**
 * Initializes the scheduler
 */
void SSscheduler::init ()
{
  WimaxScheduler::init();
}

/**
 * Interface with the TCL script
 * @param argc The number of parameter
 * @param argv The list of parameters
 */
int SSscheduler::command(int argc, const char*const* argv)
{
  //no command. Remove this function if not used.
  return TCL_ERROR;
}

/**
 * Schedule bursts/packets
 */
void SSscheduler::schedule ()
{
  int b_data;
  Connection *c;
  Burst *b, *b_tmp, *my_burst, *my_burst_rng;
  FrameMap *map = mac_->getMap();
  PeerNode *peer = mac_->getPeerNode_head(); //this is the BS
  #ifdef DEBUG_WIMAX
  assert (peer!=NULL);
  #endif

//  rpi - increment the propagation channel 
  
 
  if(peer)
    peer->setchannel((peer->getchannel())+1);


if(peer->getchannel()>999) peer->setchannel(2);  
 
//rpi
// Begin RPI   
 // We will try to Fill in the ARQ Feedback Information now...
  Packet * ph = 0;
  PeerNode * peernode;
  Connection * basic ;
  Connection * OutData;
  Packet * pfb = 0;
  hdr_mac802_16 *wimaxHdrMap ;
  u_int16_t   temp_num_of_acks = 0;
  bool out_datacnx_exists = false; 
  for (Connection *n= mac_->getCManager ()->get_in_connection (); 
       n; n=n->next_entry()) {
   if(n->getArqStatus () != NULL && n->getArqStatus ()->isArqEnabled() == 1)
   {
     if(!(n->getArqStatus ()->arq_feedback_queue_) || (n->getArqStatus ()->arq_feedback_queue_->length() == 0))
     {
				continue;
     } 
     else 
     {
		   peernode = n->getPeerNode ();
		   if(peernode->getOutData () != NULL && peernode->getOutData ()->queueLength () != 0 && getMac()->arqfb_in_ul_data_) {
		      out_datacnx_exists = true;
	   }
	   if(out_datacnx_exists == false) 
	   {
		   //*debug2("ARQ SS: Feedback in Basic Cid \n");
		   basic = peernode->getBasic (OUT_CONNECTION);
       pfb = mac_->getPacket ();
       wimaxHdrMap= HDR_MAC802_16(pfb);
		   wimaxHdrMap->header.cid = basic->get_cid ();	
       wimaxHdrMap->num_of_acks = 0;
	   }
	   else {
	   //*debug2("ARQ SS : Feedback in data Cid \n");
	   OutData = peernode->getOutData ();
 	   pfb = OutData->dequeue ();
	   wimaxHdrMap= HDR_MAC802_16(pfb);
	   if(wimaxHdrMap->header.type_arqfb == 1)
	   {
		//*debug2("ARQ SS: Feedback already present, do nothing \n");
		OutData->enqueue_head (pfb);
		continue;
	   }
	   wimaxHdrMap->num_of_acks = 0;
	   }
          //Dequeue is only once as one feedback in 5ms frame is permitted 
	  ph = n->getArqStatus ()->arq_feedback_queue_->deque();
          wimaxHdrMap->header.type_arqfb = 1;
	  hdr_mac802_16 *wimaxHdr= HDR_MAC802_16(ph);
	  for (temp_num_of_acks = 0; temp_num_of_acks < wimaxHdr->num_of_acks; temp_num_of_acks++)
	  {	
            wimaxHdrMap->arq_ie[temp_num_of_acks].cid = wimaxHdr->arq_ie[temp_num_of_acks].cid;
            wimaxHdrMap->arq_ie[temp_num_of_acks].last = wimaxHdr->arq_ie[temp_num_of_acks].last;
            wimaxHdrMap->arq_ie[temp_num_of_acks].ack_type = wimaxHdr->arq_ie[temp_num_of_acks].ack_type;
            wimaxHdrMap->arq_ie[temp_num_of_acks].fsn = wimaxHdr->arq_ie[temp_num_of_acks].fsn;
          }    
          wimaxHdrMap->num_of_acks = wimaxHdr->num_of_acks;
	  HDR_CMN(pfb)->size()	+= (wimaxHdr->num_of_acks * HDR_MAC802_16_ARQFEEDBK_SIZE);  
	  //*debug2("ARQ : In SSScheduler: Enqueueing an feedback cid: %d arq_ie->fsn:%d \n", wimaxHdr->arq_ie[0].cid,  wimaxHdr->arq_ie[0].fsn);
          if(out_datacnx_exists == false) {	
     	    // If I am here then the Ack packet has been created, so we will enqueue it in the Basic Cid
     	    basic->enqueue (pfb);
          }
          else
	    OutData->enqueue_head (pfb);  	
     	}
    }
  }
// End RPI
//	debug10("=SS= frame number :%d\n", frame_no++);

	//*debug2("\n==========MAC SS subframe =============\n");
	//*mac_->getMap()->print_frame();
	//*debug2("=========================================\n");


//Print the map
  #ifdef SAM_DEBUG
  mac_->getMap()->print_frame();
  #endif

  for (int index = 0 ; index < map->getUlSubframe()->getNbPdu (); index++) 
  {
    b_tmp = map->getUlSubframe()->getPhyPdu (index)->getBurst (0);
    if (b_tmp->getIUC()==UIUC_END_OF_MAP) continue;
    else if (b_tmp->getIUC()==UIUC_INITIAL_RANGING) continue;
    else if (b_tmp->getIUC()==UIUC_REQ_REGION_FULL) continue;
    else {

	int allocationfound = 0; Connection *c_burst;
	c_burst = mac_->getCManager ()->get_connection (b_tmp->getCid(), true);

    	if (!c_burst) {
      		continue;
        } else{
                my_burst = b_tmp;
      		allocationfound = 1;
    	}

	b_data = 0;
    	if (allocationfound == 1) {
       		u_char b_code = b_tmp->getB_CDMA_CODE ();
       		u_char b_top = b_tmp->getB_CDMA_TOP ();
       		int init_code = mac_->getMap()->getUlSubframe()->getRanging()->getCODE (mac_->addr());
       		int init_top = mac_->getMap()->getUlSubframe()->getRanging()->getTOP (mac_->addr());       

       		int bw_code = mac_->getMap()->getUlSubframe()->getBw_req()->getCODE (b_tmp->getCid());
       		int bw_top = mac_->getMap()->getUlSubframe()->getBw_req()->getTOP (b_tmp->getCid());

		//*debug10 ("\tStart SS addr: %d, CID :%d, b_code :%d, b_top :%d, ssid_code :%d, ssid_top :%d, bw_code :%d, bw_top :%d\n", mac_->addr(), b_tmp->getCid(), b_code, b_top, init_code, init_top, bw_code, bw_top);       
		//*debug10 ("\tBurst start time %d, Burst duration %d, IUC %d, subchannel offset %d, num of subchannels %d, CID %d\n", my_burst->getStarttime(), my_burst->getDuration(), my_burst->getIUC(),  my_burst->getSubchannelOffset(),  my_burst->getnumSubchannels(), my_burst->getCid());
		if (b_tmp->getCid() == 0) {
       			if ( (b_code == init_code) && (b_top == init_top) ) {          
			   //*debug10 ("\tFound initial_ranging_msg opportunity (remove enqueued cdma_init_req), Burst start time %d, Burst duration %d, IUC %d, subchannel offset %d, num of subchannels %d, CID %d\n", my_burst->getStarttime(), my_burst->getDuration(), my_burst->getIUC(),  my_burst->getSubchannelOffset(),  my_burst->getnumSubchannels(), my_burst->getCid());
//		           mac_->getMap()->getUlSubframe()->getRanging()->removeRequest_mac (mac_->addr());
		           b_data = transfer_packets1(c_burst, my_burst, b_data);
			}
		} else {
//leave it for now -> not sure how that works -> many bw-req op per SS
//
////

   		 ConnectionType_t contype;
  		 Connection *head = mac_->getCManager()->get_out_connection();
		 Connection *c_tmp1;

    		 for(int i=0; i<4; ++i){
      		    c_tmp1 = head;
      		    if(i==0) 	    contype = CONN_BASIC;
      		    else if(i==1)   contype = CONN_PRIMARY;
      		    else if(i==2)   contype = CONN_SECONDARY;
      		    else 	    contype = CONN_DATA;

		    b_data = 0;
      	 	    while(c_tmp1!=NULL){
        		if(c_tmp1->get_category()==contype){
       		            int bw_code = mac_->getMap()->getUlSubframe()->getBw_req()->getCODE (c_tmp1->get_cid());
       		            int bw_top = mac_->getMap()->getUlSubframe()->getBw_req()->getTOP (c_tmp1->get_cid());
       			    if ( (b_code == bw_code) && (b_top == bw_top) ) {          
			        //*debug10 ("\tFound bw_req opportunity (remove enqueued cdma_bw_req), Burst start time %d, Burst duration %d, IUC %d, subchannel offset %d, num of subchannels %d, CID %d\n", my_burst->getStarttime(), my_burst->getDuration(), my_burst->getIUC(),  my_burst->getSubchannelOffset(),  my_burst->getnumSubchannels(), my_burst->getCid());
				//*debug10 ("\tCID :%d, b_code :%d, b_top :%d, bw_code :%d, bw_top :%d\n", c_tmp1->get_cid(), b_code, b_top, bw_code, bw_top);       
		                b_data = transfer_packets1(c_tmp1, my_burst, b_data);
			    }
			}

		    c_tmp1 = c_tmp1->next_entry();
      	 	    }//end while
    		}//end for

	      }

    	} 

    }//end else all IUC

  }//end for all NbPU


  int allocationfound = 0;
  //We go through the list of UL bursts until we find an allocation for us


  //*debug2 ("in ss schedule function SS has %d ul bursts\n", map->getUlSubframe()->getNbPdu ());


  #ifdef SAM_DEBUG
  //*debug2 ("in ss schedule function SS has %d ul bursts\n", map->getUlSubframe()->getNbPdu ());
  #endif
  for (int index = 0 ; index < map->getUlSubframe()->getNbPdu (); index++) 
  {
    b = map->getUlSubframe()->getPhyPdu (index)->getBurst (0);

    if (b->getIUC()==UIUC_END_OF_MAP) 
    {
      //consistency check..
      #ifdef DEBUG_WIMAX
      assert (index == map->getUlSubframe()->getNbPdu ()-1);
      #endif
      break;
    }    
    


if (b->getIUC()==UIUC_INITIAL_RANGING) {

	bool t_bool;
        Connection *c_tmp = mac_->getCManager()->get_connection(0, t_bool );
//        debug10 ("CON :%d\n", c_tmp->get_cid());

        my_burst_rng = b;
        int tmp_mac = mac_->addr(); 
        ContentionSlot *slot1 = getMac()->getMap()->getUlSubframe()->getRanging ();        
	//*debug10 ("Start CDMA-INIT Ranging Region: GET init-req-backoff-start :%d, stop :%d, contention size :%d, nbretry :%d\n", slot1->getBackoff_start(), slot1->getBackoff_stop(), slot1->getSize(), getMac()->macmib_.request_retry);

        int s_init_contention_size = slot1->getSize(); //0 to 4
        int s_nbretry = getMac()->macmib_.reg_req_retry;
        int s_backoff_start = slot1->getBackoff_start();
        int s_backoff_stop = slot1->getBackoff_stop();
        if (mac_->getMap()->getUlSubframe()->getRanging()->getRequest_mac (tmp_mac) != NULL) {
             int tmp_cid_mac = mac_->getMap()->getUlSubframe()->getRanging()->getCID (tmp_mac);
             //debug10 ("Init Ranging: Found request for SS Mac addr :%d\n", tmp_mac);
             int tmp_backoff = mac_->getMap()->getUlSubframe()->getRanging()->getBACKOFF (tmp_mac);
             int tmp_timeout = mac_->getMap()->getUlSubframe()->getRanging()->getTIMEOUT (tmp_mac);
             int tmp_nbretry = mac_->getMap()->getUlSubframe()->getRanging()->getNBRETRY (tmp_mac);
             int tmp_win = mac_->getMap()->getUlSubframe()->getRanging()->getWINDOW (tmp_mac);
             int tmp_code = mac_->getMap()->getUlSubframe()->getRanging()->getCODE (tmp_mac);
             int tmp_top = mac_->getMap()->getUlSubframe()->getRanging()->getTOP (tmp_mac);
             int tmp_flagtransmit = mac_->getMap()->getUlSubframe()->getRanging()->getFLAGTRANSMIT (tmp_mac);
             //*debug10 ("\tINIT.1 SSID :%d, backoff :%d, timeout :%d, nbretry :%d, win :%d, code :%d, top :%d, flagtransmit :%d\n", tmp_mac, tmp_backoff, tmp_timeout, tmp_nbretry, tmp_win, tmp_code, tmp_top, tmp_flagtransmit);

             int update_w        = tmp_win;
             int update_retry    = tmp_nbretry;             
	     int newbackoff      = tmp_backoff;             
	     int newtop          = tmp_top;             
	     int newcode         = tmp_code;             
	     int newtimeout      = tmp_timeout;
             int newflagtransmit = tmp_flagtransmit;

             Packet *p_tmp = NULL;
             if ((p_tmp = mac_->getMap()->getUlSubframe()->getRanging ()->getPacket_P_mac (tmp_mac)) != NULL) {
                newtimeout--;
                //debug10 ("Init Ranging: Enqueue request for SS Mac addr :%d timeout %d\n", tmp_mac, newtimeout);

                if (newtimeout==0) {
                   update_retry++;

                   if (update_retry > s_nbretry) {
                      //debug10 ("\tINIT.3.removeRequest (>retry) SSID :%d, Max_nbretry :%d, current_nbretry :%d\n", tmp_mac, s_nbretry, update_retry);
                      mac_->getMap()->getUlSubframe()->getRanging()->removeRequest_mac (tmp_mac);
                      continue;
                   } else {

                     if (tmp_win<s_backoff_stop) {
                        update_w =tmp_win+1;
                        mac_->getMap()->getUlSubframe()->getRanging()->setWINDOW (tmp_mac, update_w);
                     } else {
                       update_w = s_backoff_stop;
                       mac_->getMap()->getUlSubframe()->getRanging()->setWINDOW (tmp_mac, update_w);
                     }

                     int result = rand() % ((int)(pow (2, update_w)+1));
                     newbackoff = result;
		     newtop     = result % s_init_contention_size;
                     int code_range = getMac()->macmib_.cdma_code_bw_start - getMac()->macmib_.cdma_code_bw_stop + 1;
                     int c_rand = rand() % code_range;
                     newcode  = (u_char)getMac()->macmib_.cdma_code_bw_start + (u_char)c_rand;

                     newtimeout = CDMA_TIMEOUT-1;
                     mac_->getMap()->getUlSubframe()->getRanging()->setBACKOFF (tmp_mac, newbackoff);
                     mac_->getMap()->getUlSubframe()->getRanging()->setTIMEOUT (tmp_mac, newtimeout);
                     mac_->getMap()->getUlSubframe()->getRanging()->setNBRETRY (tmp_mac, update_retry);
                     mac_->getMap()->getUlSubframe()->getRanging()->setFLAGTRANSMIT (tmp_mac, 0);
                     mac_->getMap()->getUlSubframe()->getRanging()->setTOP (tmp_mac, newtop);
                     mac_->getMap()->getUlSubframe()->getRanging()->setCODE (tmp_mac, newcode);

                     //debug10 ("\tINIT.3.updatebackoff (0 TIMEOUT) SSID :%d, oldbackoff :%d, newbackoff :%d, old_timeout :%d, new_timeout :%d, old_nbretry :%d, new_nbretry :%d, old_win :%d, new_win :%d, oldcode :%d, newcode :%d\n", tmp_mac, tmp_backoff, newbackoff, tmp_timeout-1, newtimeout, tmp_nbretry, update_retry, tmp_win, update_w, tmp_code, newcode);
                     //debug10 ("\t   start_backoff :%d, stop_backoff :%d, contention_size :%d, NBRETRY :%d, TIMEOUT :%d\n", s_backoff_start, s_backoff_stop, s_init_contention_size, s_nbretry, CDMA_TIMEOUT);
                     newflagtransmit = 0;

                     hdr_mac802_16 *wimaxHdr_t;
                     wimaxHdr_t = HDR_MAC802_16(p_tmp);
                     cdma_req_header_t *header_t = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp)->header);

                     wimaxHdr_t->phy_info.subchannel_offset = (newbackoff % s_init_contention_size)*6;

                     //debug10 ("\tINIT.3.updateOFFSET SSID :%d, #old_subchannel_offset :%d, #subchannel_offset :%d, oldtop :%d, newtop :%d\n", tmp_mac, wimaxHdr_t->phy_info.subchannel_offset, (newbackoff%s_init_contention_size)*6, header_t->top, (newbackoff%s_init_contention_size));

		     header_t->top = newtop;
		     header_t->code = newcode;

                     c_tmp->setINIT_REQ_QUEUE(mac_->addr(), 0);
                   }

                } else {
                  mac_->getMap()->getUlSubframe()->getRanging()->setTIMEOUT (tmp_mac, newtimeout);
                  //debug10 ("\tINIT.3.updatetimeout_perframe SSID :%d, oldtimeout :%d, newtimeout :%d\n", tmp_mac, tmp_timeout, newtimeout);
                }

                int newbackoff_bak = newbackoff;
                newbackoff = newbackoff - s_init_contention_size;
                if (newbackoff <0) newbackoff = 0;
                mac_->getMap()->getUlSubframe()->getRanging()->setBACKOFF (tmp_mac, newbackoff);
                //debug10 ("\tINIT.4.updatebackoff_perframe SSID :%d, oldbackoff :%d, newbackoff :%d (ori_new_backoff :%d), newtimeout :%d, newnbretry :%d, newwin :%d, top :%d, code :%d\n", tmp_mac, tmp_backoff, newbackoff, newbackoff_bak, newtimeout, update_retry, update_w, newtop, newcode);

                if (newbackoff == 0 ) {
                   hdr_cmn* ch_tmp = HDR_CMN(p_tmp);
                   hdr_mac802_16 *wimaxHdr_send;
                   wimaxHdr_send = HDR_MAC802_16(p_tmp);
                   cdma_req_header_t *header_s = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp)->header);
                   //debug10 ("\tINIT.4.backoff=0 may_transmit (if flagtransmit == 0) SSID :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d, top :%d, channel_index :%d, direction :%d\n", tmp_mac, wimaxHdr_send->phy_info.num_OFDMSymbol, wimaxHdr_send->phy_info.OFDMSymbol_offset, wimaxHdr_send->phy_info.num_subchannels, wimaxHdr_send->phy_info.subchannel_offset, header_s->code, header_s->top, wimaxHdr_send->phy_info.channel_index , wimaxHdr_send->phy_info.direction);
                   //debug10 ("\t   SSID :%d, size :%d, time :%f, direction :%d, flagtransmit :%d\n", tmp_mac, ch_tmp->size(), ch_tmp->txtime(), ch_tmp->direction(), newflagtransmit);

                   if (newflagtransmit == 0) {
                      newtimeout = CDMA_TIMEOUT-1;
                      //debug10 ("\tINIT.5.transmit SSID :%d, #old_flagtransmit :%d, #new_flagtransmit :%d reset timeout to :%d\n", tmp_mac, newflagtransmit, 1, newtimeout);
                      newflagtransmit = 1;
                      mac_->getMap()->getUlSubframe()->getRanging()->setFLAGTRANSMIT (tmp_mac, newflagtransmit);
                      mac_->getMap()->getUlSubframe()->getRanging()->setTIMEOUT (tmp_mac, newtimeout);
                      mac_->getMap()->getUlSubframe()->getRanging()->setFLAGNOWTRANSMIT (tmp_mac, 1);
                      my_burst_rng->enqueue(p_tmp->copy());
                   }
                }
             }
        }
    }
///////////End CDMA_INITIAL_RANGING COUNTER-BASED

    if (b->getIUC()==UIUC_REQ_REGION_FULL) {
        my_burst_rng = b;        
        Connection *head_tmp = mac_->getCManager()->get_out_connection();
        Connection *con_tmp = head_tmp;
        int t_i = 0;
        ContentionSlot *slot1 = getMac()->getMap()->getUlSubframe()->getBw_req ();
        //*debug10 ("Start CDMA-BW-REQ Ranging Region: GET bw-req-backoff-start :%d, stop :%d, contention size :%d, nbretry :%d\n", slot1->getBackoff_start(), slot1->getBackoff_stop(), slot1->getSize(), getMac()->macmib_.request_retry);

        int s_bw_contention_size = slot1->getSize(); //0 to 4
        int s_nbretry = getMac()->macmib_.request_retry;
        int s_backoff_start = slot1->getBackoff_start();
        int s_backoff_stop = slot1->getBackoff_stop();

        while (con_tmp!=NULL){

          int tmp_cid = con_tmp->get_cid();
          if (con_tmp->queueByteLength() == 0 && (con_tmp->getArqStatus () == NULL || con_tmp->getArqStatus ()->arq_retrans_queue_->length() == 0))  {
                //*debug10 ("\tCID %d, queueBytes = 0 so do not need to create CDMA_BW_REQ (remove if exist), c->queueBytes :%d\n", tmp_cid, con_tmp->queueByteLength());
                mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (tmp_cid);
                con_tmp = con_tmp->next_entry();
                continue;
          }

//We always create cdma_requst but we remove it later if we can send somepacket or bandwidth request out
          if(con_tmp->get_category()==CONN_BASIC || con_tmp->get_category()==CONN_PRIMARY || con_tmp->get_category()==CONN_SECONDARY || con_tmp->get_category()==CONN_DATA){
            if(con_tmp->get_category()==CONN_DATA) {
                if( (con_tmp->get_serviceflow()->getScheduling()==SERVICE_UGS) || (con_tmp->get_serviceflow()->getScheduling()==SERVICE_rtPS) ){
                } else {
                  create_cdma_request(con_tmp);
                }
            } else {
                create_cdma_request(con_tmp);
            }
          }

          //*debug10 ("CDMA-BW_REQ REGION %d CID(BC) :%d, con_CID :%d, con_category :%d, con_qlength :%d\n", t_i, my_burst_rng->getCid(), con_tmp->get_cid(), con_tmp->get_category(), con_tmp->queueByteLength());

          if (con_tmp->queueLength()>0 || (con_tmp->getArqStatus () != NULL && con_tmp->getArqStatus ()->arq_retrans_queue_->length() > 0) ) {
             int tmp_backoff = mac_->getMap()->getUlSubframe()->getBw_req()->getBACKOFF (tmp_cid);
             int tmp_timeout = mac_->getMap()->getUlSubframe()->getBw_req()->getTIMEOUT (tmp_cid);
             int tmp_nbretry = mac_->getMap()->getUlSubframe()->getBw_req()->getNBRETRY (tmp_cid);
             int tmp_win = mac_->getMap()->getUlSubframe()->getBw_req()->getWINDOW (tmp_cid);
             int tmp_code = mac_->getMap()->getUlSubframe()->getBw_req()->getCODE (tmp_cid);
             int tmp_top = mac_->getMap()->getUlSubframe()->getBw_req()->getTOP (tmp_cid);
             int tmp_flagtransmit = mac_->getMap()->getUlSubframe()->getBw_req()->getFLAGTRANSMIT (tmp_cid);
             //*debug10 ("\tBW-REQ.1 CID :%d, backoff :%d, timeout :%d, nbretry :%d, win :%d, code :%d, top :%d, flagtransmit :%d\n", tmp_cid, tmp_backoff, tmp_timeout, tmp_nbretry, tmp_win, tmp_code, tmp_top, tmp_flagtransmit);

             int update_w 		= tmp_win;
             int update_retry 		= tmp_nbretry;
             int newbackoff 		= tmp_backoff;
             int newtop 		= tmp_top;
             int newcode 		= tmp_code;
             int newtimeout 		= tmp_timeout;
             int newflagtransmit 	= tmp_flagtransmit;

             Packet *p_tmp = NULL;
             if ((p_tmp = mac_->getMap()->getUlSubframe()->getBw_req()->getPacket_P (tmp_cid)) != NULL) {
                hdr_mac802_16 *wimaxHdr_tmp;
                wimaxHdr_tmp = HDR_MAC802_16(p_tmp);
                cdma_req_header_t *header_tmp = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp)->header);
		
                mac_->getMap()->getUlSubframe()->getBw_req()->setFLAGNOWTRANSMIT (tmp_cid, 0);

                int n_sub = wimaxHdr_tmp->phy_info.num_subchannels;
                int sub_off = wimaxHdr_tmp->phy_info.subchannel_offset;
                //*debug10 ("\tBW-REQ.2.packetinfo CID :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d, top :%d\n", tmp_cid, wimaxHdr_tmp->phy_info.num_OFDMSymbol, wimaxHdr_tmp->phy_info.OFDMSymbol_offset, wimaxHdr_tmp->phy_info.num_subchannels, wimaxHdr_tmp->phy_info.subchannel_offset, header_tmp->code, header_tmp->top);
                //*debug10 ("\tBW-REQ.2.burstinfo  #b_duration :%d, #b_subchannel :%d, #b_subchannel_offset :%d, b_starttime :%f\n", my_burst_rng->getDuration(), my_burst_rng->getnumSubchannels(), my_burst_rng->getSubchannelOffset(), my_burst_rng->getStarttime());

                newtimeout--;

                if (newtimeout==0) {
                   update_retry++;

                   if (update_retry > s_nbretry) {
                      //*debug10 ("\tBW-REQ.3.removeRequest CID :%d, Max_nbretry :%d, current_nbretry :%d\n", tmp_cid, s_nbretry, update_retry);
                      mac_->getMap()->getUlSubframe()->getBw_req()->removeRequest (tmp_cid);
                      continue;
                   } else {

                     if (tmp_win<s_backoff_stop) {
                        update_w =tmp_win+1;
                        mac_->getMap()->getUlSubframe()->getBw_req()->setWINDOW (tmp_cid, update_w);
                     } else {
                       update_w = s_backoff_stop;
                       mac_->getMap()->getUlSubframe()->getBw_req()->setWINDOW (tmp_cid, update_w);
                     }

                     int result = rand() % ((int)(pow (2, update_w)+1));
                     newbackoff = result;
		     newtop     = result % s_bw_contention_size;
                     newtimeout = CDMA_TIMEOUT-1;

		     int code_range = getMac()->macmib_.cdma_code_bw_start - getMac()->macmib_.cdma_code_bw_stop + 1;
  		     int c_rand = rand() % code_range;
		     newcode  = (u_char)getMac()->macmib_.cdma_code_bw_start + (u_char)c_rand;

                     mac_->getMap()->getUlSubframe()->getBw_req()->setBACKOFF (tmp_cid, newbackoff);
                     mac_->getMap()->getUlSubframe()->getBw_req()->setTOP (tmp_cid, newtop);
                     mac_->getMap()->getUlSubframe()->getBw_req()->setCODE (tmp_cid, newcode);
                     mac_->getMap()->getUlSubframe()->getBw_req()->setTIMEOUT (tmp_cid, newtimeout);
                     mac_->getMap()->getUlSubframe()->getBw_req()->setNBRETRY (tmp_cid, update_retry);
                     mac_->getMap()->getUlSubframe()->getBw_req()->setFLAGTRANSMIT (tmp_cid, 0);

                     //*debug10 ("\tBW-REQ.3.updatebackoff (0 TIMEOUT) CID :%d, oldbackoff :%d, newbackoff :%d, old_timeout :%d, new_timeout :%d, old_nbretry :%d, new_nbretry :%d, old_win :%d, new_win :%d, code :%d\n", tmp_cid, tmp_backoff, newbackoff, tmp_timeout-1, newtimeout, tmp_nbretry, update_retry, tmp_win, update_w, tmp_code);
                     //*debug10 ("\t   start_backoff :%d, stop_backoff :%d, contention_size :%d, NBRETRY :%d, TIMEOUT :%d\n", s_backoff_start, s_backoff_stop, s_bw_contention_size, s_nbretry, CDMA_TIMEOUT);
                     newflagtransmit = 0;

                     hdr_mac802_16 *wimaxHdr_t;
                     wimaxHdr_t = HDR_MAC802_16(p_tmp);
                     cdma_req_header_t *header_t = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp)->header);

                     wimaxHdr_t->phy_info.subchannel_offset = (newbackoff % s_bw_contention_size)*6;

                     //*debug10 ("\tBW-REQ.3.updateOFFSET CID :%d, #old_subchannel_offset :%d, #subchannel_offset :%d, oldtop :%d, newtop :%d, oldcode :%d, newcode :%d\n", tmp_cid, wimaxHdr_t->phy_info.subchannel_offset, (newbackoff%s_bw_contention_size)*6, header_t->top, (newbackoff%s_bw_contention_size), header_t->code, newcode);
                     header_t->top = newtop;
                     header_t->code = newcode;

//                     tmp_top = header_t->top = newbackoff % s_bw_contention_size;
//             	     mac_->getMap()->getUlSubframe()->getBw_req()->setTOP (tmp_cid, (int)header_t->top);

                     //mac_->getMap()->getUlSubframe()->getBw_req()->setPacket_P (tmp_cid, p_tmp);
                     con_tmp->setBW_REQ_QUEUE(0);
                   }

                } else {
                  mac_->getMap()->getUlSubframe()->getBw_req()->setTIMEOUT (tmp_cid, newtimeout);
                  //*debug10 ("\tBW-REQ.3.updatetimeout_perframe CID :%d, oldtimeout :%d, newtimeout :%d\n", tmp_cid, tmp_timeout, newtimeout);
                }

                int newbackoff_bak = newbackoff;
                newbackoff = newbackoff - s_bw_contention_size;
                if (newbackoff <0) newbackoff = 0;
                mac_->getMap()->getUlSubframe()->getBw_req()->setBACKOFF (tmp_cid, newbackoff);
                //*debug10 ("\tBW-REQ.4.updatebackoff_perframe CID :%d, oldbackoff :%d, newbackoff :%d (ori_newbackoff :%d), newtimeout :%d, newnbretry :%d, newwin :%d, top :%d, code :%d\n", tmp_cid, tmp_backoff, newbackoff, newbackoff_bak, newtimeout, update_retry, update_w, newtop, newcode);

                if (newbackoff == 0 ) {
                   hdr_cmn* ch_tmp = HDR_CMN(p_tmp);
                   hdr_mac802_16 *wimaxHdr_send;
                   wimaxHdr_send = HDR_MAC802_16(p_tmp);
                   cdma_req_header_t *header_s = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp)->header);
                   //*debug10 ("\tBW-REQ.4.backoff=0 may_transmit (if flagtransmit == 0) CID :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d, top :%d, channel_index :%d, direction :%d\n", tmp_cid, wimaxHdr_send->phy_info.num_OFDMSymbol, wimaxHdr_send->phy_info.OFDMSymbol_offset, wimaxHdr_send->phy_info.num_subchannels, wimaxHdr_send->phy_info.subchannel_offset, header_s->code, header_s->top, wimaxHdr_send->phy_info.channel_index , wimaxHdr_send->phy_info.direction);
                   //*debug10 ("\t   CID :%d, size :%d, time :%f, direction :%d, flagtransmit :%d\n", tmp_cid, ch_tmp->size(), ch_tmp->txtime(), ch_tmp->direction(), newflagtransmit);

                   if (newflagtransmit == 0) {
                      newtimeout = CDMA_TIMEOUT-1;
                      //*debug10 ("\tBW-REQ.5.transmit CID :%d, #old_flagtransmit :%d, #new_flagtransmit :%d reset timeout to :%d\n", tmp_cid, newflagtransmit, 1, newtimeout);
                      newflagtransmit = 1;
                      mac_->getMap()->getUlSubframe()->getBw_req()->setFLAGTRANSMIT (tmp_cid, newflagtransmit);
                      mac_->getMap()->getUlSubframe()->getBw_req()->setTIMEOUT (tmp_cid, newtimeout);
                      mac_->getMap()->getUlSubframe()->getBw_req()->setFLAGNOWTRANSMIT (tmp_cid, 1);
                      my_burst_rng->enqueue(p_tmp->copy());
                   }
                }
            }

           }
           t_i++;
           con_tmp = con_tmp->next_entry();
      }
    }
///////////End CDMA_BW_REQ COUNTER-BASED

    //get the packets from the connection with the same CID
    //debug2 ("\tBurst CID=%d\n", b->getCid());
    c = mac_->getCManager ()->get_connection (b->getCid(), true);
    
    if (!c) {
      continue;
    } else{
      my_burst = b;
      allocationfound = 1;
    }

/*
    b_data = 0;
    if (allocationfound == 1) {
       u_char b_code = b->getB_CDMA_CODE ();
       u_char b_top = b->getB_CDMA_TOP ();
       int init_code = mac_->getMap()->getUlSubframe()->getRanging()->getCODE (mac_->addr());
       int init_top = mac_->getMap()->getUlSubframe()->getRanging()->getTOP (mac_->addr());

       debug10 ("In SS addr: %d, CID :%d, b_code :%d, b_top :%d, ssid_code :%d, ssid_top :%d\n", mac_->addr(), c->get_cid(), b_code, b_top, init_code, init_top);
       debug10 ("\tBurst start time %d, Burst duration %d, IUC %d, subchannel offset %d, num of subchannels %d, CID %d\n", my_burst->getStarttime(), my_burst->getDuration(), my_burst->getIUC(),  my_burst->getSubchannelOffset(),  my_burst->getnumSubchannels(), my_burst->getCid());
       if ( (b->getCid() == 0 ) && (b_code == init_code) && (b_top == init_top) ) {
          debug10 ("\tFound initial_ranging_msg opportunity (transfer_packet1), Burst start time %d, Burst duration %d, IUC %d, subchannel offset %d, num of subchannels %d, CID %d, b_data :%d\n", my_burst->getStarttime(), my_burst->getDuration(), my_burst->getIUC(),  my_burst->getSubchannelOffset(),  my_burst->getnumSubchannels(), my_burst->getCid(), b_data);
//          b_data = transfer_packets1(c, my_burst, b_data);

       }
    }
*/
//end Check if same cdma code

  }
  
  Connection *head = mac_->getCManager()->get_out_connection();
  Connection *con = head;

  if ( (allocationfound == 1) && (my_burst->getB_CDMA_CODE()==0) && (my_burst->getB_CDMA_TOP()==0) ) {
    b_data = 0;
    //*debug10 ("In SS, Found data opportunity, Burst start time %d, Burst duration %d, IUC %d, subchannel offset %d, num of subchannels %d, CID %d\n", my_burst->getStarttime(), my_burst->getDuration(), my_burst->getIUC(),  my_burst->getSubchannelOffset(),  my_burst->getnumSubchannels(), my_burst->getCid());
    ConnectionType_t contype;

    for(int i=0; i<4; ++i){
      c = head;
      if(i==0) 	    contype = CONN_BASIC;
      else if(i==1) contype = CONN_PRIMARY;
      else if(i==2) contype = CONN_SECONDARY;
      else 	    contype = CONN_DATA;

      while(c!=NULL){
        if(c->get_category()==contype){
	  if(c->queueLength()>0 || (con->getArqStatus () != NULL && con->getArqStatus ()->arq_retrans_queue_->length() > 0)){
	    //*debug10 ("In SS, Entering transfer_packets1, CID :%d, c->queueBytes :%d, c->queueLength :%d\n", c->get_cid(), con->queueByteLength(), c->queueLength());
		//*debug10 ("Check FRAG/PACK/ARG: Frag : %d, Pack :%d, ARQ: %p\n",c->isFragEnable(),c->isPackingEnable(), c->getArqStatus ()); 				
	    if (c->isFragEnable() && c->isPackingEnable() && (c->getArqStatus () != NULL) && (c->getArqStatus ()->isArqEnabled() == 1))
		{
	    	//*debug2("In SS, before transfer_packet_with_fragpackarq(). retrans Q length is %d\r\n", c->getArqStatus ()->arq_retrans_queue_->length() );
            b_data = transfer_packets_with_fragpackarq (c, my_burst, b_data); /*RPI*/
        }
	    else 
	    {
           //*debug2 ("In SS, before transfer_packets1, c->queueBytes :%d, c->queueLength :%d, b_data :%d\n", c->queueByteLength(), c->queueLength(), b_data);
	       b_data = transfer_packets1(c, my_burst, b_data);     
	    }

	    //*debug10 ("In SS, after transfer_packets1, c->queueBytes :%d, c-queueLength :%d, b_data :%d\n", con->queueByteLength(), c->queueLength(), b_data);
	  }
	}

	c = c->next_entry();
      }//end while
    }//end for
  }

}

/**
 * Create a cdma bw-req request for the given connection
 * This function may need to be updated to handle
 * incremental and aggregate requests
 * @param con The connection to check
 */
void SSscheduler::create_cdma_request (Connection *con)
{ 
  if (con->queueLength()==0 && (con->getArqStatus () == NULL || con->getArqStatus ()->arq_retrans_queue_->length() == 0)){
    //*debug2 ("Create CDMA-BW-REQ, contype :%d CID :%d -- At %f in Mac %d but con->queuelength is empty => return\n", con->get_category(), con->get_cid(), NOW, mac_->addr());
    return; 
  }
  if (mac_->getMap()->getUlSubframe()->getBw_req()->getRequest (con->get_cid())!=NULL) {
    //*debug2 ("Create CDMA-BW-REQ, contype :%d CID %d -- At %f in Mac %d already pending requests => return\n", con->get_category(), con->get_cid(), NOW, mac_->addr());    
    return; 
  }

  //*debug2("Create CDMA-BW-REQ, contype :%d CID :%d -- (queue is not empty :%d) in ssscheduler for basic :5, pri :6, sec :7, data :8\n", con->get_category(), con->get_cid(), con->queueByteLength());
    
  ContentionSlot *slot1 = getMac()->getMap()->getUlSubframe()->getBw_req ();
//  debug10 ("GET bw-req-backoff-start :%d, stop :%d, contention size :%d\n", slot1->getBackoff_start(), slot1->getBackoff_stop(), slot1->getSize());

  int s_bw_contention_size = slot1->getSize(); //0 to 4
  int s_nbretry = 0;
  int s_backoff_start = slot1->getBackoff_start();
  int s_backoff_stop = slot1->getBackoff_stop();
  int s_window = s_backoff_start;
  int fix_six_subchannel = 6;
//  int result = rand() % ((int)(pow (2, s_window)+1));
  int result = rand() % s_bw_contention_size;
//  int s_backoff = floor(result / s_bw_contention_size);
//BW_REQ; no backoff
  int s_backoff = 0;
  int sub_channel_off = result % s_bw_contention_size;
  int s_flagtransmit = 0;
  int s_flagnowtransmit = 0;

  Packet *p= mac_->getPacket();
  hdr_cmn* ch = HDR_CMN(p);
  hdr_mac802_16 *wimaxHdr;

  wimaxHdr = HDR_MAC802_16(p);
  wimaxHdr->cdma = 1; //mark the packet 
  wimaxHdr->phy_info.num_subchannels = fix_six_subchannel;
  wimaxHdr->phy_info.subchannel_offset = sub_channel_off*fix_six_subchannel;
  wimaxHdr->phy_info.num_OFDMSymbol = 1;
  wimaxHdr->phy_info.OFDMSymbol_offset = 0; //initial_offset;
  wimaxHdr->phy_info.channel_index = con->getPeerNode()->getchannel();
  wimaxHdr->phy_info.direction = 1;
  u_char header_top = sub_channel_off;
//  u_char header_code = rand() % (MAXCODE-1);

  int code_range = getMac()->macmib_.cdma_code_bw_start - getMac()->macmib_.cdma_code_bw_stop + 1;
  int c_rand = rand() % code_range;
  u_char header_code = (u_char)getMac()->macmib_.cdma_code_bw_start + (u_char)c_rand;
  //*debug10 (" CODE init start :%d, stop :%d, rand :%d, final code :%d\n", getMac()->macmib_.cdma_code_bw_start, getMac()->macmib_.cdma_code_bw_stop, c_rand, header_code);

  cdma_req_header_t *header = (cdma_req_header_t *)&(HDR_MAC802_16(p)->header);
  header->ht=1;
  header->ec=1;
  header->type = 0x3;
  header->top = header_top;
  header->br = mac_->addr();
  header->code = header_code;
  header->cid = con->get_cid();

  int req_size = con->queueByteLength();

  if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1) {
    assert ((con->getArqStatus ()->arq_retrans_queue_->length() == 0 && con->getArqStatus ()->arq_retrans_queue_->byteLength() == 0)
	    || (con->getArqStatus ()->arq_retrans_queue_->length() > 0 && con->getArqStatus ()->arq_retrans_queue_->byteLength() > 0));
     if ((con->getArqStatus ()->arq_retrans_queue_) && (con->getArqStatus ()->arq_retrans_queue_->length() > 0)) {
        //*debug2(" ARQ Create Request: Picking retransmission queue too for sending request\n");
        req_size += con->getArqStatus ()->arq_retrans_queue_->byteLength ();
     }
  }

  double txtime = mac_->getPhy()->getSymbolTime();
  ch->txtime() = txtime;

  mac_->getMap()->getUlSubframe()->getBw_req()->addRequest (p, con->get_cid(), req_size, result, CDMA_TIMEOUT, s_nbretry, s_window, (int)header_code, (int)header_top, s_flagtransmit, s_flagnowtransmit);

  //*debug10 (" SSscheduler enqueued cdma_bw request for cid :%d, q-len :%d, (nbPacket :%d), code :%d, top :%d, ss_id :%d, backoff :%d, window :%d, nbretry :%d, flagtransmit :%d, flagnowtransmit :%d\n", con->get_cid(), req_size, con->queueLength(), header_code, header_top, mac_->addr(), s_backoff, s_window, s_nbretry, s_flagtransmit, s_flagnowtransmit);

  //debug10 (" symbol_offset :%d, symbol :%d, subchannel_offset :%d, subchannel :%d, channel_index :%d, direction :%d, txtime :%e\n", wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.subchannel_offset, wimaxHdr->phy_info.num_subchannels, wimaxHdr->phy_info.channel_index , wimaxHdr->phy_info.direction, ch->txtime());

}

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
 * @modified by Chakchai So-In
 */

#include "ulsubframetimer.h"
#include "framemap.h"
#include "subframe.h"
#include "wimaxscheduler.h"
#include "contentionslot.h"

/**
 * Creates a timer to handle the subframe transmission
 * @param subframe The UlSubframe
 */
UlSubFrameTimer::UlSubFrameTimer (UlSubFrame *subframe): pdu_(0), newphy_(true), mac_(0)
{
  assert (subframe);
  subframe_ = subframe;
  //OFDMSymbol_ =5;
  OFDMSymbol_ = 0;
  /*  if (!mac_) {
      mac_= subframe_->map_->getMac();
      }*/
  //  debug2 (" start ulduration = %d \n" , mac_->getStartUlduration());
  //  debug2 (" start ulduration = %d \n" , mac_->getMaxUlduration());
  
  //OFDMSymbol_ =  mac_->getStartUlduration();
}

/**
 * Reset the timer
 */
void UlSubFrameTimer::reset ()
{
  pdu_= NULL;
  newphy_ = true;
  //OFDMSymbol_=5;
  OFDMSymbol_= 0;
  //OFDMSymbol_ =  mac_->getStartUlduration();
  if (status()==TIMER_PENDING)
    cancel();
}

/**
 * When it expires, the timer will handle the next packet to send
 * @param e not used
 */
void UlSubFrameTimer::expire( Event* e )
{
  if (!mac_) {
    mac_= subframe_->map_->getMac();
  }

  // debug2 (" inside expire start ulduration = %d \n" , mac_->getStartUlduration());
  // debug2 (" inside expire max ulduration = %d \n" , mac_->getMaxUlduration());

  //mac_->debug ("At %f in Mac %d UlsubFrameTimer expires\n", NOW, mac_->addr());
  // Commented by Barun : 21-Sep-2011
  //debug2("At %f in Mac %d UlsubFrameTimer expires, startulduration :%d\n", NOW, mac_->addr(), mac_->getStartUlduration());

  if(mac_->getNodeType() == STA_BS ) return;
  
  int iuc=0;

  if (newphy_) {
    if (pdu_==NULL){
     
      OFDMSymbol_ =  3;//mac_->getStartUlduration();
      //OFDMSymbol_ =  mac_->getStartUlduration();
      //debug2 ("\t ul take first pdu\n");
      //get the first pdu
      pdu_ = subframe_->getFirstPdu();
      if (!pdu_)
	return; //this means there was no uplink burst allocated

    } else {
      //debug10 ("\t continue pdu\n");
      iuc = pdu_->getBurst(0)->getIUC();
      //check if this is a contention slot
      if (iuc == UIUC_INITIAL_RANGING) {
	//stop ranging timer
        //debug10 ("\t ul_subframe pause initial ranging\n");

	//subframe_->ranging_->pauseTimers ();
      } else if (iuc == UIUC_REQ_REGION_FULL) {
	//stop bw request timers
        //debug10 ("\t ul_subframe pause bw/cdma requests\n");
	
	//subframe_->bw_req_->pauseTimers();
      }
      pdu_ = pdu_->next_entry();
    }  
    //if(pdu_)  
    if (pdu_->getBurst(0)->getIUC()==UIUC_END_OF_MAP){
      pdu_=NULL; //reset for next frame
      newphy_=true;
      //mac_->debug ("\tend of map\n");
      //*debug10 ("\t ul_subframe end of map\n");

      if (mac_->getNodeType()==STA_BS) {
	mac_->getPhy()->setMode (OFDM_SEND);
      } else {
	mac_->getPhy()->setMode (OFDM_RECV);
      }
      return; //end of subframe
    }
    
    Lastpdu_ = pdu_;
    //change the modulation
    UlBurst *burst =  (UlBurst*)pdu_->getBurst(0);
    iuc = burst->getIUC();
    //debug10 ("\t ul_subframe Searching for IUC :%d in getBurst(0)\n", iuc);
    if (iuc == UIUC_EXT_UIUC && burst->getExtendedUIUC()== UIUC_FAST_RANGING) {
      iuc = burst->getFastRangingUIUC ();
      //debug2 ("Searching for IUC=%d\n", iuc);
    }
    Ofdm_mod_rate rate = subframe_->getProfile (iuc)->getEncoding();
    mac_->getPhy()->setModulation (rate);
    //check if this is a contention slot
    if (iuc == UIUC_INITIAL_RANGING) {
      //resume ranging timer
      //debug10 ("\t ul_subframe resume initial ranging, IUC :%d dudration :%d symbols\n", iuc, burst->getDuration());
      //subframe_->ranging_->resumeTimers(burst->getDuration()*mac_->getPhy()->getSymbolTime());
    } else if (iuc == UIUC_REQ_REGION_FULL) {
      //resume cdma bw request timers
      //debug10 ("\t ul_subframe resume bw/cdma requests, IUC :%d dudration :%d symbols\n", iuc, burst->getDuration());
      //subframe_->bw_req_->resumeTimers(burst->getDuration()*mac_->getPhy()->getSymbolTime());
    }
  }

  //Lastpdu_ = pdu_; 
  //UlBurst *burst =  (UlBurst*)Lastpdu_->getBurst(0);
  //    iuc = burst->getIUC();

  //added for contention region packet transmission- they looked weird 
  if((iuc == UIUC_INITIAL_RANGING) || (iuc == UIUC_REQ_REGION_FULL)) 
    {

      //check if packet to send
      int q_len = pdu_->getBurst(0)->getQueueLength_packets();
      //debug10 ("\t ul_subframe inside old code iuc :%d (ranging or cdma bw_req region) => only one Phypdu, Qburst :%d\n", iuc, q_len);

/*
//Old code
      Packet *p = pdu_->getBurst(0)->dequeue();
      if (p) {

         hdr_cmn* ch_tmp = HDR_CMN(p);
         hdr_mac802_16 *wimaxHdr_send;
         wimaxHdr_send = HDR_MAC802_16(p);
         cdma_req_header_t *header_s = (cdma_req_header_t *)&(HDR_MAC802_16(p)->header);

         debug10 ("\t ul_subframe enqueue CID :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d, channel_index :%d, direction :%d\n", header_s->cid, wimaxHdr_send->phy_info.num_OFDMSymbol, wimaxHdr_send->phy_info.OFDMSymbol_offset, wimaxHdr_send->phy_info.num_subchannels, wimaxHdr_send->phy_info.subchannel_offset, header_s->code, wimaxHdr_send->phy_info.channel_index , wimaxHdr_send->phy_info.direction);
         debug10 ("\t ul_subframe packet CID :%d, size :%d, time :%f, direction :%d\n", header_s->cid, ch_tmp->size(), ch_tmp->txtime(), ch_tmp->direction());

	newphy_= false;
	double txtime = HDR_CMN(p)->txtime();
	debug10 ("\t ul_subframe -CDMA packet to send, contention/ranging packets\n");
	//schedule for next packet
	mac_->transmit (p);

	if (pdu_->getBurst(0)->getQueueLength()!=0) {
	  debug10 ("\t ul_subframe -reschedule in contention/ranging packet txitme :%e (now+txtime :%e), getBurst(0)->getQueue :%d\n", txtime, NOW+txtime, pdu_->getBurst(0)->getQueueLength());
	  resched (txtime); //wait transmition time + GAP
	  return;
	}
      }
*/

//This loop will check how many cdma req enqueued; however, it's only 2 cols or 1 cols for either cdma initial ranging or cdma bw-req so for more than 2 or 1 => need OFDM looping; do it later

      while(q_len>0) {
	Packet *p_tmp;
	//Packet *p = pdu_->getBurst(0)->lookup(0);
	p_tmp = pdu_->getBurst(0)->dequeue();
        if (p_tmp) {

	   q_len--;
           hdr_cmn* ch_tmp = HDR_CMN(p_tmp);
           hdr_mac802_16 *wimaxHdr_send;
           wimaxHdr_send = HDR_MAC802_16(p_tmp);
           cdma_req_header_t *header_s = (cdma_req_header_t *)&(HDR_MAC802_16(p_tmp)->header);

        // Commented by Barun : 21-Sep-2011
         //debug10 ("\t ul_subframe enqueue CID :%d, #symbol :%d, #symbol_offset :%d, #subchannel :%d, #subchannel_offset :%d, code :%d, channel_index :%d, direction :%d\n", header_s->cid, wimaxHdr_send->phy_info.num_OFDMSymbol, wimaxHdr_send->phy_info.OFDMSymbol_offset, wimaxHdr_send->phy_info.num_subchannels, wimaxHdr_send->phy_info.subchannel_offset, header_s->code, wimaxHdr_send->phy_info.channel_index , wimaxHdr_send->phy_info.direction);
         debug10 ("\t ul_subframe packet CID :%d, size :%d, time :%f, direction :%d\n", header_s->cid, ch_tmp->size(), ch_tmp->txtime(), ch_tmp->direction());

	   double txtime = HDR_CMN(p_tmp)->txtime();
	   //debug10 ("\t ul_subframe -CDMA packet to send, contention/ranging packets\n");
	   mac_->transmit (p_tmp);
        }

      }

      if(iuc == UIUC_INITIAL_RANGING) {
         resched (2*mac_->getPhy()->getSymbolTime());
      } else {
         resched (mac_->getPhy()->getSymbolTime());
      }

/*
      newphy_= true;
      double stime=0.0;
      stime = subframe_->map_->getStarttime();
      //mac_->debug("\tstart frame=%f\n", stime);
      stime += subframe_->getStarttime()*mac_->getPhy()->getPS();
      //mac_->debug ("\tulstart = %f\n", stime);
      stime += pdu_->next_entry()->getBurst(0)->getStarttime()*mac_->getPhy()->getSymbolTime();
      mac_->debug ("\t ul-next pdu start :%d\n", pdu_->next_entry()->getBurst(0)->getStarttime());
      mac_->debug ("\t At %f ul-Next burst :%d at :%f\n", NOW, pdu_->next_entry()->getBurst(0)->getIUC(), stime);
      mac_->debug ("\t ul_subframe (contention region) At %f, subframe_start :%f, next PDU start :%d, burst IUC :%d, stime (start frame+ulstart+pdu_next) :%f, resched at :%f\n", NOW, subframe_->map_->getStarttime(), pdu_->next_entry()->getBurst(0)->getStarttime(), pdu_->next_entry()->getBurst(0)->getIUC(), stime, stime-NOW);


      resched (stime-NOW);
*/

      Lastpdu_ = pdu_->next_entry(); 
    }
  //end - contention region packet transmission
  else
    {
      //debug2("entering my code \n " ); 

      if((newphy_==true) && pdu_== NULL) return; 
      //else if((newphy_==true) && pdu_!= NULL)  Lastpdu_ = pdu_; 

      if(newphy_ == false)
	pdu_=Lastpdu_;
      //Lastpdu_ = pdu_;
      //debug2 ("At %f in Mac %d  in my code  OFDMSymbol_ = %d maxulduration = %d \n", NOW, mac_->addr(),OFDMSymbol_,mac_->getMaxUlduration());

      //debug10 ("\t ul_subframe inside old code (data regions) => loop (one phypdu per SS), OFDMSymbol_ :%d, Maxulduration :%d\n", OFDMSymbol_,mac_->getMaxUlduration());

      while(pdu_!=NULL)

	{
	  //if (pdu_->getBurst(0)->getIUC()==UIUC_END_OF_MAP) continue;
	  /*  if(pdu_->getBurst(0)->getQueueLength() != NULL)
	      while(pdu_->getBurst(0)->getQueueLength()!=0)
	      {
	      //check if packet to send
	      Packet *p = pdu_->getBurst(0)->dequeue();
	  */

	  int len = pdu_->getBurst(0)->getQueueLength_packets();

	  while(len!=0) 
	    {
	      Packet *p = pdu_->getBurst(0)->lookup(0);

	      if (p) {
		//newphy_= false;
		hdr_mac802_16 *wimaxHdr;
		wimaxHdr = HDR_MAC802_16(p);
		len--;
		//debug10 ("\t loop ul_subframe packet being chk for transmission in ulsubframe timer -pkt OFDMSymbol_ :%d, symbol_offset :%d, symbol :%d, subchannel_offset :%d, subchannels :%d, len :%d\n", OFDMSymbol_, wimaxHdr->phy_info.OFDMSymbol_offset,wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.subchannel_offset, wimaxHdr->phy_info.num_subchannels,len);
 
		if(wimaxHdr->phy_info.OFDMSymbol_offset == OFDMSymbol_)
		  { 		   
		    //debug rpi
		    //mac802_16_dl_map_frame *frame = (mac802_16_dl_map_frame*) p->accessdata();
		    //  if(frame) 
		    
		    //debug10 ("\t loop ul_subframe packet being transmitted(p) from ulsubframetime timer (phy_info.OFDMSymbol_offset == OFDMSymbol_)\n");
                         
		    //debug rpi
		    p = pdu_->getBurst(0)->dequeue();
		    mac_->transmit (p);
		  }	     
		else
		  break;                   
	      }

	      //double txtime = HDR_CMN(p)->txtime();
	      //debug2 ("\tPacket to send\n");
	      //schedule for next packet
	      //mac_->transmit (p);
	      //if (pdu_->getBurst(0)->getQueueLength()!=0) {
	      //mac_->debug ("\treschedule in %f (%f)\n", txtime, NOW+txtime);
	      //resched (txtime); //wait transmition time + GAP
	      //return;
	    }
	  pdu_ = pdu_->next_entry();

	}
 
      OFDMSymbol_++;  
      if (OFDMSymbol_ > mac_->getMaxUlduration()) //reset for nextframe
	{
	  pdu_ =NULL ; 
	  newphy_= true;
	  //debug10 ("\t OFDMSymbol_ :%d > Maxulduration :%d so reset for nextframe and update OFDM to 3\n",OFDMSymbol_, mac_->getMaxUlduration());

	  OFDMSymbol_ = 3;// mac_->getStartUlduration(); 
	  if (mac_->getNodeType()==STA_BS) {
	    mac_->getPhy()->setMode (OFDM_SEND);
	  } else {
	    mac_->getPhy()->setMode (OFDM_RECV);
	  }
	  return;
	}

      newphy_=false;
      pdu_=Lastpdu_;
      //debug2("reschedule the ulsubframe timer\n");
      resched (/*OFDMSymbol_ */ mac_->getPhy()->getSymbolTime());

    }

}

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

#include "dlsubframetimer.h"
#include "framemap.h"
#include "subframe.h"
#include "wimaxscheduler.h"
#include "contentionslot.h"

/**
 * Creates a timer to handle the subframe transmission
 * @param subframe The DlSubframe
 */
DlSubFrameTimer::DlSubFrameTimer (DlSubFrame *subframe): OFDMSymbol_(DL_PREAMBLE), newsubframe_(true), mac_(0)
{
  assert (subframe);
  subframe_ = subframe;
}

/**
 * Reset the timer
 */
void DlSubFrameTimer::reset ()
{
  //  burstIndex_ = 0;
  //  newburst_ = true;
  OFDMSymbol_ = DL_PREAMBLE;   
  newsubframe_ = true; 
  if (status()==TIMER_PENDING)
    cancel();
}

/**
 * When it expires, the timer will handle the next packet to send
 * @param e not used
 */
void DlSubFrameTimer::expire( Event* e )
{

  hdr_mac802_16 *wimaxHdr;  

  if (!mac_) {
    mac_= subframe_->map_->getMac();
  }

  FrameMap *map = mac_->getMap();
  if(mac_->getNodeType() == STA_MN )
    return;

  int iuc;
  if (OFDMSymbol_ >= (mac_->getMaxDlduration()))
    {
    
      if (mac_->getNodeType()==STA_MN) {
	mac_->getPhy()->setMode (OFDM_SEND);
      } else {
	mac_->getPhy()->setMode (OFDM_RECV);
      }

      OFDMSymbol_ = DL_PREAMBLE;

      return;  // scheduled all the packets for the subframe. 
    
    }
  
  for (int index = 0 ; index < map->getDlSubframe()->getPdu ()->getNbBurst(); index++) 
    {
      Burst *b = subframe_->getPdu()->getBurst(index);
      if (b->getIUC()==DIUC_END_OF_MAP)
	{
	  break;
	}
		
      //change modulation
      iuc = b->getIUC();
      Ofdm_mod_rate rate = subframe_->getProfile (iuc)->getEncoding();
      mac_->getPhy()->setModulation (rate);

      int len = b->getQueueLength_packets();
      while(len!=0) {
	  Packet *p = b->lookup(0);
	  if(p) {
	      wimaxHdr = HDR_MAC802_16(p);
	      len--;
	      // Commented by Barun : 21-Sep-2011
	      //debug2(" loop dlsubframe timer -OFDMSymbols_[%d]\t -Symbol_off[%d]\t symbol #[%d]\t subch_off[%d]\t subch_#[%d], len :%d, burst_index :%d\n",
		     //OFDMSymbol_, wimaxHdr->phy_info.OFDMSymbol_offset,wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.subchannel_offset,
		     //wimaxHdr->phy_info.num_subchannels, len, index);

	      if(wimaxHdr->phy_info.OFDMSymbol_offset == OFDMSymbol_)      // change this to == remember 
		{ 		   

/*
		  mac802_16_dl_map_frame *frame = (mac802_16_dl_map_frame*) p->accessdata();
		  if(frame) 
		    debug2(" loop dl_subframe packet being transmitted(p) from dlsubframe timer - frame type :%d, (phy_info.OFDMSymbol_offset == OFDMSymbol_)\n", (frame->type));
                         
		  p = b->dequeue();
		  mac_->transmit (p);
*/
                  //debug2(" loop dl_subframe packet being transmitted(p) from dlsubframe timer - (phy_info.OFDMSymbol_offset == OFDMSymbol_ (%d))\n", OFDMSymbol_);
                  p = b->dequeue();
                  mac_->transmit (p);


		}	
	      else
		break;   
	    }
	}
    }

  OFDMSymbol_++; 
  resched (/*OFDMSymbol_ */ mac_->getPhy()->getSymbolTime());

  //no packet to send...schedule for next phypdu
  /*  newburst_= true;
      burstIndex_++;
      double stime=0.0;
      assert (b->next_entry());
  
      stime = subframe_->map_->getStarttime();
      stime += b->next_entry()->getStarttime()*mac_->getPhy()->getSymbolTime();
      //debug2 ("\tMap start time=%f Next burst at %f\n", subframe_->map_->getStarttime(), stime);
      resched (stime-NOW);
  */  

}

/* This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and
 * is in the public domain.
 * NIST assumes no responsibility whatsoever for its use by other parties,
 File * and makes no guarantees, expressed or implied, about its quality,
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


#include "framemap.h"
#include "contentionslot.h"
#include "wimaxscheduler.h"
#include "ulburst.h"
#include "dlburst.h"

/*
 * Creates a map of the frame
 * @param mac Pointer to the mac layer
 */
FrameMap::FrameMap (Mac802_16 *mac): dlsubframe_(this), ulsubframe_(this)
{
  assert (mac);
  mac_ = mac; 

  //retreive information from mac
  rtg_ = mac_->phymib_.rtg;
  ttg_ = mac_->phymib_.ttg;
  duration_ = mac_->getFrameDuration();
}

/**
 * Compute the DL_MAP packet based on the information contained in the structure
 */
Packet* FrameMap::getDL_MAP( )
{
  Packet *p = mac_->getPacket();
  hdr_cmn* ch = HDR_CMN(p);

  //printf ("Creating DL_MAP:");
  int nbies = dlsubframe_.getPdu()->getNbBurst();
  // Commented by Barun : 21-Sep-2011
  //printf ("nbies=%d\n",nbies);

  //allocate data for DL_MAP
  p->allocdata (sizeof (struct mac802_16_dl_map_frame));
  mac802_16_dl_map_frame *frame = (mac802_16_dl_map_frame*) p->accessdata();

  frame->type = MAC_DL_MAP;
  frame->bsid = mac_->addr();
  frame->nb_ies = nbies;
  
  //allocate IEs
  mac802_16_dlmap_ie *ies = frame->ies;

  int bc_ie = 0;
  for (int i = 0 ; i < nbies ; i++) {
    Burst *b = dlsubframe_.getPdu()->getBurst(i);
    ies[i].diuc = b->getIUC();
    ies[i].start_time = b->getStarttime();
//rpi added for including subchannels for OFDMA
    ies[i].subchannel_offset=b->getSubchannelOffset ();
    
    ies[i].num_of_subchannels=b->getnumSubchannels();
    
    ies[i].num_of_symbols=b->getDuration();

    ies[i].symbol_offset=b->getStarttime();

//    if ( (b->getCid() == 65535) || (b->getCid() == DIUC_END_OF_MAP) ) bc_ie++;
    if (  (b->getCid() == DIUC_END_OF_MAP) ) bc_ie++;
//rpi end    
    //printf ("DL_MAP_IE index :%d, cid :%d, bc_ie :%d, diuc :%d, starttime :%d, #subchannels :%d, #symbols :%d\n", i, b->getCid(), bc_ie, ies[i].diuc, ies[i].start_time, ies[i].num_of_subchannels, ies[i].num_of_symbols);
    if (b->getIUC()!=DIUC_END_OF_MAP) {
      ies[i].cid =  b->getCid();
      if (i==0)
	ies[i].preamble = dlsubframe_.getPdu()->getPreamble();
      else
	ies[i].preamble = 0;
    }
  }
  
//#ie -> we exclude dl-map but include ul-map
//In next version, the way to calcuate dl-map will be investigated.
  int dl_ie = nbies - bc_ie - 1;
  //printf ("Frame_map.DL_MAP before size => chsize :%d, ie (include ulmap) :%d, after ie_size+fix :%d\n", ch->size(), nbies-bc_ie-1, int(ceil(GET_DL_MAP_SIZE(dl_ie))));
//  ch->size() = ch->size() + int (GET_DL_MAP_SIZE(dl_ie));
  ch->size() = ceil (GET_DL_MAP_SIZE(dl_ie));

  return p;
}

/**
 * Compute and return the DCD frame
 */
Packet* FrameMap::getDCD( )
{
  Packet *p = mac_->getPacket ();
  hdr_cmn* ch = HDR_CMN(p);

  //allocate data for DL_MAP
  //printf ("getDCD...nbprofile=%d\n", dlsubframe_.getNbProfile());
  p->allocdata (sizeof (struct mac802_16_dcd_frame));
  mac802_16_dcd_frame *frame = (mac802_16_dcd_frame*) p->accessdata();

  frame->type = MAC_DCD;
  frame->dcid = mac_->addr(); //to check if needs to be different from ucid
  frame->config_change_count = dlsubframe_.getCCC(); 
  frame->frame_duration_code = mac_->getFrameDurationCode ();
  frame->frame_number = mac_->frame_number_;
  frame->nb_prof = dlsubframe_.getNbProfile();
  frame->ttg = mac_->phymib_.ttg;
  frame->rtg = mac_->phymib_.rtg;
  frame->frequency = (int) (mac_->getPhy()->getFreq()/1000); 

  //allocate IEs
  mac802_16_dcd_profile *profiles = frame->profiles;

  int i=0;
  for (Profile *p = dlsubframe_.getFirstProfile() ; p ; p=p->next_entry()) {
    //set data for first burst
    profiles[i].diuc = p->getIUC(); 
    profiles[i].frequency = p->getFrequency();
    profiles[i].fec = p->getEncoding(); 
    i++;
  }
  //the end of map is already included in the frame length
  
  //printf ("Frame_map.DCD before size => chsize :%d, after size :%d\n", ch->size(), int(GET_DCD_SIZE(dlsubframe_.getNbProfile())) );
//  ch->size() += GET_DCD_SIZE(dlsubframe_.getNbProfile());
  ch->size() = int(GET_DCD_SIZE(dlsubframe_.getNbProfile()));
  return p;
}

/**
 * Compute and return the UL_MAP frame
 */
Packet* FrameMap::getUL_MAP( )
{
  Packet *p = mac_->getPacket ();
  hdr_cmn* ch = HDR_CMN(p);

  int nbies = ulsubframe_.getNbPdu(); //there is one burst per UL phy PDU
  //printf ("getUL_MAP, nbies=%d\n", nbies);
  //allocate data for DL_MAP
  p->allocdata (sizeof (struct mac802_16_ul_map_frame));
  mac802_16_ul_map_frame *frame = (mac802_16_ul_map_frame*) p->accessdata();

  frame->type = MAC_UL_MAP;
  frame->ucid = mac_->addr();  //set channel ID to index_ to be unique
  frame->ucd_count = ulsubframe_.getCCC(); 
  frame->allocation_start = ulsubframe_.getStarttime(); //the subframe starts with the contention slot
  frame->nb_ies = nbies; 
  
  //allocate IEs
  mac802_16_ulmap_ie *ies = frame->ies;

  int bc_ie = 0;
  int sum_ie_size = 0;
  int i_normal_ie = 0;
  int i_cdma_ie = 0;
  int i=0;
  for (PhyPdu *p = ulsubframe_.getFirstPdu(); p ; p= p ->next_entry()) {
    UlBurst *b = (UlBurst*) p->getBurst(0);
    ies[i].uiuc = b->getIUC(); //end of map
    ies[i].start_time = b->getStarttime();
    //rpi added for including subchannels for OFDMA
    ies[i].subchannel_offset=b->getSubchannelOffset ();
    
    ies[i].num_of_subchannels=b->getnumSubchannels(); 
    
    ies[i].num_of_symbols=b->getDuration();

    ies[i].symbol_offset=b->getStarttime();

    ies[i].cdma_ie.code=b->getB_CDMA_CODE();
    ies[i].cdma_ie.subchannel=b->getB_CDMA_TOP();

    if ( (b->getCid() == -1) || (b->getCid() == UIUC_END_OF_MAP) ) bc_ie++;

    // Commented by Barun : 21-Sep-2011
    //printf ("Frame_map.UL_MAP_IE index :%d, cid :%d, bc_ie :%d, uiuc :%d, starttime :%d, #subchannels :%d, #symbols :%d\n", i, b->getCid(), bc_ie, ies[i].uiuc, ies[i].start_time, ies[i].num_of_subchannels, ies[i].num_of_symbols);
//rpi end 
    if (b->getIUC()!=UIUC_END_OF_MAP) {
      ies[i].cid =  b->getCid();
      ies[i].midamble_rep = b->getMidamble();
      ies[i].duration = b->getDuration();
/*
      ies[i].cdma_ie.code=0;
      ies[i].cdma_ie.subchannel=0;
*/
      if (b->getIUC() == UIUC_EXT_UIUC) {
	ies[i].extended_uiuc = b->getExtendedUIUC();
	if (b->getExtendedUIUC ()== UIUC_FAST_RANGING) {
	  ies[i].fast_ranging.mac_addr = b->getFastRangingMacAddr ();
	  ies[i].fast_ranging.uiuc = b->getFastRangingUIUC ();
	}
      }
    }
    int normal_or_cdma = 0;

    if (b->getCid() >= 0) {
       if ( (ies[i].cdma_ie.code==0) && (ies[i].cdma_ie.subchannel==0) ) {
	  normal_or_cdma = UL_MAP_IE_SIZE;
	  i_normal_ie++;
       } else {
	    normal_or_cdma = UL_CDMA_MAP_IE_SIZE;
	    i_cdma_ie++;
       }
       sum_ie_size += normal_or_cdma;
    }
//    ch->size() += normal_or_cdma;

    i++;
  }

  //printf ("Frame_map.UL_MAP_IE index :%d, #normal_ie :%d, #cdma_ie :%d, ie_size :%d, ul_map_size :%d\n", i, i_normal_ie, i_cdma_ie, sum_ie_size, int(GET_UL_MAP_SIZE(0))+sum_ie_size);
  ch->size() = int(GET_UL_MAP_SIZE(0))+sum_ie_size;
//  ch->size() += GET_UL_MAP_SIZE(nbies);
  return p;
}

/**
 * Compute and return the UCD frame
 */
Packet* FrameMap::getUCD( )
{
  Packet *p = mac_->getPacket ();
  hdr_cmn* ch = HDR_CMN(p);

  //allocate data for DL_MAP
  p->allocdata (sizeof (struct mac802_16_ucd_frame));
  mac802_16_ucd_frame *frame = (mac802_16_ucd_frame*) p->accessdata();

  frame->type = MAC_UCD;
  frame->config_change_count = 0; //changed by scheduler
  frame->rng_backoff_start = ulsubframe_.getRanging()->getBackoff_start();
  frame->rng_backoff_end = ulsubframe_.getRanging()->getBackoff_stop();
  frame->rng_req_size = ulsubframe_.getRanging()->getSize();
  frame->req_backoff_start = ulsubframe_.getBw_req()->getBackoff_start();
  frame->req_backoff_end = ulsubframe_.getBw_req()->getBackoff_stop()+1;
  frame->bw_req_size = ulsubframe_.getBw_req()->getSize();

  frame->nb_prof = ulsubframe_.getNbProfile();
  //allocate IEs
  mac802_16_ucd_profile *profiles = frame->profiles;

  int i=0;
  for (Profile *p = ulsubframe_.getFirstProfile() ; p ; p=p->next_entry()) {
    //set data for first burst
    profiles[i].uiuc = p->getIUC(); 
    profiles[i].fec = p->getEncoding(); 
    i++;
  }

  //the end of map is already included in the frame length
  //printf ("Frame_map.UCD before size => chsize :%d, after size :%d\n",ch->size(), int(GET_UCD_SIZE(ulsubframe_.getNbProfile())));
//  ch->size() += int(GET_UCD_SIZE(ulsubframe_.getNbProfile()));
  ch->size() = int(GET_UCD_SIZE(ulsubframe_.getNbProfile()));
  return p;
}

/**
 * Parse a DL_MAP message and create the data structure
 * @param frame The DL frame information
 */
void FrameMap::parseDLMAPframe (mac802_16_dl_map_frame *frame)
{
  //printf ("parse DL-MAP in %d\n", mac_->addr());
  // Clear previous information  
  while (dlsubframe_.getPdu()->getNbBurst()>0) {
    Burst *b = dlsubframe_.getPdu()->getBurst (0);
    dlsubframe_.getPdu()->removeBurst (b);
    delete b;
  }

  int nbies = frame->nb_ies;
  mac802_16_dlmap_ie *ies = frame->ies;

  for (int i = 0 ; i < nbies ; i++) {
    Burst *b = dlsubframe_.getPdu()->addBurst(i);
    b->setIUC(ies[i].diuc);
    b->setStarttime(ies[i].start_time);

    //rpi added for including subchannels for OFDMA
    b->setnumSubchannels(ies[i].num_of_subchannels);//=ies[i].num_of_subchannels;ies[i].subchannel_offset=;
    
    b->setSubchannelOffset (ies[i].subchannel_offset);//ies[i].num_of_subchannels=b->getSubchannelOffset (); 
    
    b->setDuration(ies[i].num_of_symbols);//ies[i].num_of_symbols=b->getDuration();

   // ies[i].symbol_offset=b->getStarttime();
//rpi end 

    if (b->getIUC()!=DIUC_END_OF_MAP) {
      b->setCid(ies[i].cid);
      if (i==0) //first burst contains preamble
	dlsubframe_.getPdu()->setPreamble(ies[i].preamble);
    }
    //printf ("\t Adding burst %d: cid=%d, iuc=%d start=%d\n", i, b->getCid(), b->getIUC(),b->getStarttime());
  }
  //should we parse end of map too?
}

/**
 * Parse a DCD message and create the data structure
 * @param frame The DL frame information
 */
void FrameMap::parseDCDframe (mac802_16_dcd_frame *frame)
{
  //clear previous profiles
  dlsubframe_.removeProfiles();

  int nb_prof = frame->nb_prof;
  mac_->frame_number_ = frame->frame_number;
  mac802_16_dcd_profile *profiles = frame->profiles;
  mac_->setFrameDurationCode (frame->frame_duration_code);

  for (int i = 0 ; i < nb_prof ; i++) {
    Profile *p = dlsubframe_.addProfile (profiles[i].frequency, (Ofdm_mod_rate)profiles[i].fec);
    p->setIUC (profiles[i].diuc);
    //printf ("\t Adding dl profile %i: f=%d, rate=%d, iuc=%d\n", i, p->getFrequency(), p->getEncoding(), p->getIUC());
  }
}

/**
 * Parse a UL_MAP message and create the data structure
 * @param frame The UL frame information
 */
void FrameMap::parseULMAPframe (mac802_16_ul_map_frame *frame)
{
  //printf ("parse UL-MAP\n");
  // Clear previous information
  for (PhyPdu *p = ulsubframe_.getFirstPdu(); p ; p = ulsubframe_.getFirstPdu()) {
    ulsubframe_.removePhyPdu(p);
    delete (p);
  }
  
  int nbies = frame->nb_ies;
  mac802_16_ulmap_ie *ies = frame->ies;

  ulsubframe_.setStarttime(frame->allocation_start);
  //mac_->debug ("\tul start time = %d %f\n", frame->allocation_start, frame->allocation_start*mac_->getPhy()->getPS());

  for (int i = 0 ; i < nbies ; i++) {
    UlBurst *b = (UlBurst*)(ulsubframe_.addPhyPdu(i,0))->addBurst(0);
    b->setIUC(ies[i].uiuc);
    b->setStarttime(ies[i].start_time);

    //rpi added for including subchannels for OFDMA
    b->setnumSubchannels(ies[i].num_of_subchannels);//=ies[i].num_of_subchannels;ies[i].subchannel_offset=;
    
    b->setSubchannelOffset (ies[i].subchannel_offset);//ies[i].num_of_subchannels=b->getSubchannelOffset (); 
    b->setB_CDMA_CODE (ies[i].cdma_ie.code);
    b->setB_CDMA_TOP (ies[i].cdma_ie.subchannel);
    
    //b->setDuration(ies[i].num_of_symbols);//ies[i].num_of_symbols=b->getDuration();

   // ies[i].symbol_offset=b->getStarttime();
//rpi end 

    if (b->getIUC()!=UIUC_END_OF_MAP) {
      b->setCid(ies[i].cid);
      b->setMidamble(ies[i].midamble_rep);
      b->setDuration(ies[i].duration);
      if (b->getIUC() == UIUC_EXT_UIUC) {
	if(ies[i].extended_uiuc== UIUC_FAST_RANGING) {
	  b->setFastRangingParam (ies[i].fast_ranging.mac_addr, ies[i].fast_ranging.uiuc);
	}
      }
    }
    /*mac_->debug ("\t Adding burst %d: cid=%d, iuc=%d start=%d (%f) duration=%d\n", \
      i, b->getCid(), b->getIUC(),b->getStarttime(), starttime_+frame->allocation_start*mac_->getPhy()->getPS()+b->getStarttime()*mac_->getPhy()->getSymbolTime(), b->getDuration());*/
  }
}

/**
 * Parse a UCD message and create the data structure
 * @param frame The DL frame information
 */
void FrameMap::parseUCDframe (mac802_16_ucd_frame *frame)
{
  assert (frame);
  //clear previous profiles
  ulsubframe_.removeProfiles();
  
  /*printf ("parse UCD..rng_start=%d, rng_stop=%d, req_start=%d, req_stop=%d\n",\
    frame->rng_backoff_start, frame->rng_backoff_end, frame->req_backoff_start,
    frame->req_backoff_end);*/
  ulsubframe_.getRanging()->setBackoff_start(frame->rng_backoff_start);
  ulsubframe_.getRanging()->setBackoff_stop(frame->rng_backoff_end);
  ulsubframe_.getRanging()->setSize(frame->rng_req_size);
  ulsubframe_.getBw_req()->setBackoff_start(frame->req_backoff_start);
  ulsubframe_.getBw_req()->setBackoff_stop(frame->req_backoff_end);  
  ulsubframe_.getBw_req()->setSize(frame->bw_req_size);

  int nb_prof = frame->nb_prof;
  mac802_16_ucd_profile *profiles = frame->profiles;
  for (int i = 0 ; i < nb_prof ; i++) {
    Profile *p = ulsubframe_.addProfile (0, (Ofdm_mod_rate)(profiles[i].fec));
    p->setIUC (profiles[i].uiuc);
    //printf ("\t Adding ul profile %i: f=%d, rate=%d, iuc=%d\n", i, p->getFrequency(), p->getEncoding(), p->getIUC());
  }
}


/**
 * Print a snapshot of the frame
 */
void FrameMap::print_frame ()
{
  printf ("Frame %d: duration=%f start=%f stop=%f\n", mac_->getFrameNumber (), duration_, starttime_, starttime_+duration_);
  printf ("\tDownlink:\n");
  
  int nbies = dlsubframe_.getPdu()->getNbBurst();
  for (int i = 0 ; i < nbies ; i++) {
    Burst *b = dlsubframe_.getPdu()->getBurst(i);
    printf ("\t\tBurst %d: start=%d (%f)", i, b->getStarttime(), starttime_+b->getStarttime()*mac_->getPhy()->getSymbolTime());
    printf (" DIUC=%d",  b->getIUC());
    //printf(" num subchanels = %d subchannel offset =%d \n",b->getnumSubchannels(),b->getSubchannelOffset () );
    if (b->getIUC()==DIUC_END_OF_MAP) {
      printf (" (END_OF_MAP)\n");
    } else {
      printf (" CID=%d\n",b->getCid());
    }
  }

  printf ("\tUplink:\n");  
  nbies = ulsubframe_.getNbPdu(); //there is one burst per UL phy PDU
  int i=0;
  for (PhyPdu *p = ulsubframe_.getFirstPdu(); p ; p= p ->next_entry()) {
    UlBurst *b = (UlBurst*) p->getBurst(0);
    printf ("\t\tBurst %d: start=%d (%f)", i, b->getStarttime(), starttime_+ulsubframe_.getStarttime()*mac_->getPhy()->getPS()+b->getStarttime()*mac_->getPhy()->getSymbolTime());
    // Commented by Barun : 21-Sep-2011
    //printf(" num subchanels = %d subchannel offset =%d code =%d top =%d\n",b->getnumSubchannels(),b->getSubchannelOffset (), b->getB_CDMA_CODE(), b->getB_CDMA_TOP() );
    //printf(" num subchanels = %d subchannel offset =%d\n",b->getnumSubchannels(),b->getSubchannelOffset () );

    printf (" UIUC=%d",  b->getIUC());

    switch (b->getIUC()) {
    case UIUC_INITIAL_RANGING:
      //*printf (" (INITIAL_RANGING)");
      break;
    case UIUC_REQ_REGION_FULL:
      //*printf (" (REQ_REGION_FULL)");
      break;
    case UIUC_REQ_REGION_FOCUSED:
      //*printf (" (REQ_REGION_FOCUSED)");
      break;
    case UIUC_FOCUSED_CONTENTION_IE:
      //*printf (" (FOCUSED_CONTENTION_IE)");
      break;
    case UIUC_PROFILE_1:
    case UIUC_PROFILE_2:
    case UIUC_PROFILE_3:
    case UIUC_PROFILE_4:
    case UIUC_PROFILE_5:
    case UIUC_PROFILE_6:
    case UIUC_PROFILE_7:
    case UIUC_PROFILE_8:
      //*printf (" (PROFILE %d)", b->getIUC() - UIUC_PROFILE_1 + 1);
      break;
    case UIUC_SUBCH_NET_ENTRY:
      //*printf (" (SUBCH_NET_ENTRY)");
      break;
    case UIUC_END_OF_MAP:
      //*printf (" (END_OF_MAP)\n");
      break;
    case UIUC_EXT_UIUC:
      //*printf (" (EXT_UIUC)");
      if (b->getExtendedUIUC ()== UIUC_FAST_RANGING) {
	//printf (" ExtUIUC=FAST_RANGING addr=%d FR_UIUC=%d", b->getFastRangingMacAddr (), b->getFastRangingUIUC ());
      }      
      break;
    }
    //*if (b->getIUC()!=UIUC_END_OF_MAP) {
      //*printf (" CID=%d duration=%d\n", b->getCid(), b->getDuration());
    //*}
    printf("\n");
    i++;
  }

}

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
 * @modified by  Chakchai So-In
 */

#include "bsscheduler.h"
#include "burst.h"
#include "dlburst.h"
#include "ulburst.h"
#include "random.h"
#include "wimaxctrlagent.h"
#include "mac802_16BS.h"
#include "mac802_16.h"
#include <stdlib.h>

#define FRAME_SIZE 0.005
#define CODE_SIZE 256
#define CDMA_6SUB 6
#define MAX_CONN 2048

//Scheduler allocates CBR every frame 
#define UGS_AVG	

/**
 * Bridge to TCL for BSScheduler
 */
int frame_number=0;

//This structure is used for virtual allocation and "index_burst" is used for #counting bursts
int index_burst=0;
struct virtual_burst {
  int alloc_type; //0 = DL_MAP, 1 = UL_MAP, 2 = DCD, 3 = UCD, 4 = other broadcast, 5 = DL_burst, 6 = UL_burst
  int cid;
  int n_cid;
  int iuc;
  Ofdm_mod_rate mod;
  bool preamble;
  int symboloffset;
  int suboffset;
  int numsymbol;
  int numsub;   
  int numslots; 
  float byte;     
  int rep;      
  int dl_ul;    //0 = DL, 1 = UL
  int ie_type;
} virtual_alloc[MAX_MAP_IE], ul_virtual_alloc[MAX_MAP_IE];


static class BSSchedulerClass : public TclClass {
public:
  BSSchedulerClass() : TclClass("WimaxScheduler/BS") {}
  TclObject* create(int, const char*const*) {
    return (new BSScheduler());
    
  }
} class_bsscheduler;

/*
 * Create a scheduler
 */
BSScheduler::BSScheduler () : WimaxScheduler ()
{
  //*debug2 ("BSScheduler created\n");
  default_mod_ = OFDM_BPSK_1_2;
  //bind ("dlratio_", &dlratio_);
  bind("Repetition_code_", &Repetition_code_);
  bind("init_contention_size_", &init_contention_size_);
  bind("bw_req_contention_size_", &bw_req_contention_size_);

//  contention_size_ = MIN_CONTENTION_SIZE;

  nextDL_ = -1;
  nextUL_ = -1;  
}
 
/**
 * Return the MAC casted to BSScheduler
 * @return The MAC casted to BSScheduler
 */
Mac802_16BS* BSScheduler::getMac()
{
  return (Mac802_16BS*)mac_;
}

/*
 * Interface with the TCL script
 * @param argc The number of parameter
 * @param argv The list of parameters
 */
int BSScheduler::command(int argc, const char*const* argv)
{
  if (argc == 3) {
    if (strcmp(argv[1], "set-default-modulation") == 0) {
      if (strcmp(argv[2], "OFDM_BPSK_1_2") == 0)
	default_mod_ = OFDM_BPSK_1_2;
      else if (strcmp(argv[2], "OFDM_QPSK_1_2") == 0)
	default_mod_ = OFDM_QPSK_1_2;
      else if (strcmp(argv[2], "OFDM_QPSK_3_4") == 0)
	default_mod_ = OFDM_QPSK_3_4;
      else if (strcmp(argv[2], "OFDM_16QAM_1_2") == 0)
	default_mod_ = OFDM_16QAM_1_2;
      else if (strcmp(argv[2], "OFDM_16QAM_3_4") == 0)
	default_mod_ = OFDM_16QAM_3_4;
      else if (strcmp(argv[2], "OFDM_64QAM_2_3") == 0)
	default_mod_ = OFDM_64QAM_2_3;
      else if (strcmp(argv[2], "OFDM_64QAM_3_4") == 0)
	default_mod_ = OFDM_64QAM_3_4;
      else
	return TCL_ERROR;
      return TCL_OK;
    }
    else if (strcmp(argv[1], "set-contention-size") == 0) {
      contention_size_ = atoi (argv[2]);
#ifdef DEBUG_WIMAX
      assert (contention_size_>=0);
#endif 
      return TCL_OK;      
    }
/*
    else if (strcmp(argv[1], "set-init-contention-size") == 0) {
      init_contention_size_ = atoi (argv[2]);
#ifdef DEBUG_WIMAX
      assert (init_contention_size_>=0);
#endif 
      return TCL_OK;      
    }
    else if (strcmp(argv[1], "set-bw-req-contention-size") == 0) {
      bw_req_contention_size_ = atoi (argv[2]);
#ifdef DEBUG_WIMAX
      assert (bw_req_contention_size_>=0);
#endif 
      return TCL_OK;      
    }
*/



  }
  return TCL_ERROR;
}

/**
 * Initializes the scheduler
 */
void BSScheduler::init ()
{
  WimaxScheduler::init();

  // If the user did not set the profiles by hand, let's do it
  // automatically
  if (getMac()->getMap()->getDlSubframe()->getProfile (DIUC_PROFILE_1)==NULL) {
    //#ifdef SAM_DEBUG
    //debug2 ("Adding profiles\n");
    //#endif
    Profile *p = getMac()->getMap()->getDlSubframe()->addProfile ((int)round((getMac()->getPhy()->getFreq()/1000)), OFDM_BPSK_1_2);
    p->setIUC (DIUC_PROFILE_1);
    p = getMac()->getMap()->getDlSubframe()->addProfile ((int)round((getMac()->getPhy()->getFreq()/1000)), OFDM_QPSK_1_2);
    p->setIUC (DIUC_PROFILE_2);
    p = getMac()->getMap()->getDlSubframe()->addProfile ((int)round((getMac()->getPhy()->getFreq()/1000)), OFDM_QPSK_3_4);
    p->setIUC (DIUC_PROFILE_3);
    p = getMac()->getMap()->getDlSubframe()->addProfile ((int)round((getMac()->getPhy()->getFreq()/1000)), OFDM_16QAM_1_2);
    p->setIUC (DIUC_PROFILE_4);
    p = getMac()->getMap()->getDlSubframe()->addProfile ((int)round((getMac()->getPhy()->getFreq()/1000)), OFDM_16QAM_3_4);
    p->setIUC (DIUC_PROFILE_5);
    p = getMac()->getMap()->getDlSubframe()->addProfile ((int)round((getMac()->getPhy()->getFreq()/1000)), OFDM_64QAM_2_3);
    p->setIUC (DIUC_PROFILE_6);
    p = getMac()->getMap()->getDlSubframe()->addProfile ((int)round((getMac()->getPhy()->getFreq()/1000)), OFDM_64QAM_3_4);
    p->setIUC (DIUC_PROFILE_7);

    p = getMac()->getMap()->getUlSubframe()->addProfile (0, default_mod_);
    p->setIUC (UIUC_INITIAL_RANGING);
    p = getMac()->getMap()->getUlSubframe()->addProfile (0, default_mod_);
    p->setIUC (UIUC_REQ_REGION_FULL);  

    p = getMac()->getMap()->getUlSubframe()->addProfile (0, OFDM_BPSK_1_2);
    p->setIUC (UIUC_PROFILE_1);
    p = getMac()->getMap()->getUlSubframe()->addProfile (0, OFDM_QPSK_1_2);
    p->setIUC (UIUC_PROFILE_2);
    p = getMac()->getMap()->getUlSubframe()->addProfile (0, OFDM_QPSK_3_4);
    p->setIUC (UIUC_PROFILE_3);
    p = getMac()->getMap()->getUlSubframe()->addProfile (0, OFDM_16QAM_1_2);
    p->setIUC (UIUC_PROFILE_4);
    p = getMac()->getMap()->getUlSubframe()->addProfile (0, OFDM_16QAM_3_4);
    p->setIUC (UIUC_PROFILE_5);
    p = getMac()->getMap()->getUlSubframe()->addProfile (0, OFDM_64QAM_2_3);
    p->setIUC (UIUC_PROFILE_6);
    p = getMac()->getMap()->getUlSubframe()->addProfile (0, OFDM_64QAM_3_4);
    p->setIUC (UIUC_PROFILE_7);
  }

  //init contention slots
  ContentionSlot *slot = getMac()->getMap()->getUlSubframe()->getRanging ();
//  slot->setSize (getInitRangingopportunity ());
  slot->setSize (getMac()->macmib_.init_contention_size);
  slot->setBackoff_start (getMac()->macmib_.rng_backoff_start);
  slot->setBackoff_stop (getMac()->macmib_.rng_backoff_stop);
  
  slot = getMac()->getMap()->getUlSubframe()->getBw_req ();
//  slot->setSize (getBWopportunity ());
  slot->setSize (getMac()->macmib_.bw_req_contention_size);
  slot->setBackoff_start (getMac()->macmib_.bw_backoff_start);
  slot->setBackoff_stop (getMac()->macmib_.bw_backoff_stop);

}

/**
 * Compute and return the bandwidth request opportunity size
 * @return The bandwidth request opportunity size
 */
int BSScheduler::getBWopportunity ()
{
  int nbPS = BW_REQ_PREAMBLE * getMac()->getPhy()->getSymbolPS();
  //add PS for carrying header
  nbPS += (int) round((getMac()->getPhy()->getTrxTime (HDR_MAC802_16_SIZE, getMac()->getMap()->getUlSubframe()->getProfile(UIUC_REQ_REGION_FULL)->getEncoding())/getMac()->getPhy()->getPS ()));
  //debug2 ("BWopportunity size=%d\n", nbPS);
  //debug10 ("BWopportunity size 1 oppo :%d nbPS, BW_PREAMBLE :%d + HDR_MAC802.16 :%d, with UIUC_REQ_REGION_FULL :%d\n", nbPS, BW_REQ_PREAMBLE, HDR_MAC802_16_SIZE, UIUC_REQ_REGION_FULL);
  return nbPS;
}

/**
 * Compute and return the initial ranging opportunity size
 * @return The initial ranging opportunity size
 */
int BSScheduler::getInitRangingopportunity ()
{
  int nbPS = INIT_RNG_PREAMBLE * getMac()->getPhy()->getSymbolPS();
  // Commented by Barun : 21-Sep-2011
  //debug2("nbPS = %d " , nbPS);  
  //add PS for carrying header
  nbPS += (int) round((getMac()->getPhy()->getTrxTime (RNG_REQ_SIZE+HDR_MAC802_16_SIZE, getMac()->getMap()->getUlSubframe()->getProfile(UIUC_INITIAL_RANGING)->getEncoding())/getMac()->getPhy()->getPS ()));
  // Commented by Barun : 21-Sep-2011
  //debug2 ("Init ranging opportunity size=%d\n", nbPS);
  //debug10 ("InitRanging opportunity size 1 oppo :%d nbPS, INIT_RNG_PREAMBLE :%d + RNG_REQ_SIZE :%d + HDR_MAC802.16 :%d, with UIUC_INITIAL_RANGING :%d\n", nbPS, INIT_RNG_PREAMBLE, RNG_REQ_SIZE, HDR_MAC802_16_SIZE, UIUC_INITIAL_RANGING);
  return nbPS;  
}

// This function is used to increase #DL_MAP_IEs (DL_MAP size)
int BSScheduler::increase_dl_map_ie(int num_of_entries, int totalslots, int num_ie) {

  OFDMAPhy *phy = mac_->getPhy();
  int slots_sofar = 0;
  float dl_ie_size = DL_MAP_IE_SIZE;
//padding to byte based
  float dl_map_increase_byte = virtual_alloc[0].byte + num_ie*dl_ie_size;
  int dl_map_increase_slots = virtual_alloc[0].rep*(int)ceil(ceil(dl_map_increase_byte)/(double)phy->getSlotCapacity(virtual_alloc[0].mod,DL_));

    // Commented by Barun : 21-Sep-2011
  //debug10 ("Check_DLMAP.From array DL MAP byte :%f, slots :%d, IE_SIZE :%f, slotcapacity :%d, #ie :%d\n", virtual_alloc[0].byte, virtual_alloc[0].numslots, dl_ie_size, phy->getSlotCapacity(virtual_alloc[0].mod,DL_), num_ie);
  //debug10 ("\tNew DL MAP byte :%f, slots :%d\n", dl_map_increase_byte, dl_map_increase_slots);

  for(int i = 1; i<num_of_entries; i++){
    // Commented by Barun : 21-Sep-2011
	//debug2 ("\t index :%d, numslots :%d, accum :%d\n", i, virtual_alloc[i].numslots, slots_sofar);
        slots_sofar = slots_sofar + virtual_alloc[i].numslots;
  }

  int slots_temp = slots_sofar;
  slots_sofar = slots_sofar + dl_map_increase_slots;
  if ( slots_sofar <= totalslots) {
        // Commented by Barun : 21-Sep-2011
        //debug10 ("Check_DLMAP. Old DL map byte :%f, slots :%d, New DL map byte :%f, slots :%d, #ie :%d\n", virtual_alloc[0].byte, virtual_alloc[0].numslots, dl_map_increase_byte, dl_map_increase_slots, num_ie);

	virtual_alloc[0].byte = dl_map_increase_byte;
	virtual_alloc[0].numslots = dl_map_increase_slots;
	return slots_sofar;
  } else {
	return -1;
  }

}


// This function is used to check if the allocation is feasible (enough slots for both DL_MAP_IE and its own data allocation
int BSScheduler::overallocation_withdlmap(int num_of_entries, int totalslots, int ownslots) {

  OFDMAPhy *phy = mac_->getPhy();
  int slots_sofar = 0;
  float dl_ie_size = DL_MAP_IE_SIZE;
//padding to byte based
  float dl_map_increase_byte = virtual_alloc[0].byte + dl_ie_size;
  int dl_map_increase_slots = virtual_alloc[0].rep*(int)ceil((ceil)(dl_map_increase_byte)/(double)phy->getSlotCapacity(virtual_alloc[0].mod,DL_));

    // Commented by Barun : 21-Sep-2011
  //debug10 ("Check_DLMAP.From array DL MAP byte :%f, slots :%d, IE_SIZE :%f, slotcapacity :%d\n", virtual_alloc[0].byte, virtual_alloc[0].numslots, dl_ie_size, phy->getSlotCapacity(virtual_alloc[0].mod,DL_));
  //debug10 ("\tNew DL MAP byte :%f, slots :%d\n", dl_map_increase_byte, dl_map_increase_slots);

  for(int i = 1; i<num_of_entries; i++){
    // Commented by Barun : 21-Sep-2011
	//debug2 ("\t index :%d, numslots :%d, accum :%d\n", i, virtual_alloc[i].numslots, slots_sofar);
        slots_sofar = slots_sofar + virtual_alloc[i].numslots;
  }

  int slots_temp = slots_sofar;
  slots_sofar = slots_sofar + ownslots + dl_map_increase_slots;
  if ( slots_sofar <= totalslots) {
        // Commented by Barun : 21-Sep-2011
        //debug10 ("Check_DLMAP. Old DL map byte :%f, slots :%d, New DL map byte :%f, slots :%d, available burst slot size :%d\n", virtual_alloc[0].byte, virtual_alloc[0].numslots, dl_map_increase_byte, dl_map_increase_slots, ownslots);

	virtual_alloc[0].byte = dl_map_increase_byte;
	virtual_alloc[0].numslots = dl_map_increase_slots;
	return ownslots;
  } else {
	if ( (slots_temp + dl_map_increase_slots) <= totalslots ) {
	        // Commented by Barun : 21-Sep-2011
        	//debug10 ("Check_DLMAP. Old DL map byte :%f, slots :%d, New DL map byte :%f, slots :%d, available slot burst size :%d\n", virtual_alloc[0].byte, virtual_alloc[0].numslots, dl_map_increase_byte, dl_map_increase_slots, totalslots - slots_temp - dl_map_increase_slots);
		virtual_alloc[0].byte = dl_map_increase_byte;
		virtual_alloc[0].numslots = dl_map_increase_slots;
		return (totalslots - slots_temp - dl_map_increase_slots);
	} else {
		return -1;
	}
  }
}


// This function is used to calculate maximum number of connection with the increase of DL_MAP_IE
int BSScheduler::max_conn_withdlmap(int num_of_entries, int totalslots) {

  OFDMAPhy *phy = mac_->getPhy();
  int slots_sofar = 0;
  float dl_ie_size = DL_MAP_IE_SIZE;
  int max_conn = 0;
  float dl_map_increase_byte = 0; 
  int dl_map_increase_slots = 0;
  float virtual_increase_byte = virtual_alloc[0].byte;
  int virtual_increase_slots = virtual_alloc[0].numslots;

  for(int i = 0; i<num_of_entries; i++) {
//	debug2 ("\t index :%d, numslots :%d, accum :%d\n", i, virtual_alloc[i].numslots, slots_sofar);
	slots_sofar = slots_sofar + virtual_alloc[i].numslots;
  }
  int slots_data = slots_sofar - virtual_alloc[0].numslots;

    // Commented by Barun : 21-Sep-2011
  //debug10 ("Check_DLMAP. Max supported conn total_dl_slots :%d, sofar slots :%d, dlmap slots :%d, dlmap byte :%f, available slots :%d\n", totalslots, slots_sofar, virtual_alloc[0].numslots, virtual_alloc[0].byte, totalslots-slots_sofar);

  while (1) {
	virtual_increase_byte += dl_ie_size;
	int virtual_increase_byte_ceil = (int)ceil(virtual_increase_byte);
	virtual_increase_slots = virtual_alloc[0].rep*ceil((double)(virtual_increase_byte_ceil)/(double)phy->getSlotCapacity(virtual_alloc[0].mod,DL_));
	if ( (virtual_increase_slots + slots_data) >= totalslots ) {
	  break;
	} else {
	  max_conn++;
	}
  }

  return max_conn;
}


// This function is used to calculate number of available slots after #DL_MAP_IEs are taken into account.
int BSScheduler::freeslots_withdlmap_given_conn(int num_of_entries, int totalslots, int newconn) {

  OFDMAPhy *phy = mac_->getPhy();
  int slots_sofar = 0;
  float dl_ie_size = DL_MAP_IE_SIZE;
  int max_conn = 0;
  float dl_map_increase_byte = 0; 
  int dl_map_increase_slots = 0;
  float virtual_increase_byte = virtual_alloc[0].byte;
  int virtual_increase_slots = virtual_alloc[0].numslots;
  int total_slots_withdlmap_nodata = 0;

  for(int i = 0; i<num_of_entries; i++) {
//	debug2 ("\t index :%d, numslots :%d, accum :%d\n", i, virtual_alloc[i].numslots, slots_sofar);
	slots_sofar = slots_sofar + virtual_alloc[i].numslots;
  }
  int slots_data = slots_sofar - virtual_alloc[0].numslots;

  for (int i=0; i<newconn; i++) {
	virtual_increase_byte += dl_ie_size;
  }
  int virtual_increase_byte_ceil = (int)ceil(virtual_increase_byte);
  virtual_increase_slots = virtual_alloc[0].rep*ceil((double)(virtual_increase_byte_ceil)/(double)phy->getSlotCapacity(virtual_alloc[0].mod,DL_));

    // Commented by Barun : 21-Sep-2011
  //debug10 ("Check_DLMAP. #Free slots newconn :%d, sofar slots :%d, dlmap slots :%d, dlmap byte :%f, newdlmap slots :%d, newdlmap byte :%d\n", newconn, slots_sofar, virtual_alloc[0].numslots, virtual_alloc[0].byte, virtual_increase_slots, virtual_increase_byte_ceil);

  return totalslots - slots_data - virtual_increase_slots;
}


// This function is used check #available slots excluing DL_MAP_IE
// return #request slots if feasible else return #available slots
int BSScheduler::overallocation_withoutdlmap(int num_of_entries, int totalslots, int ownslots) {

  int slots_sofar = 0;

  for(int i = 0; i<num_of_entries; i++){
        slots_sofar = slots_sofar + virtual_alloc[i].numslots;
  }

  int slots_temp = slots_sofar;
  slots_sofar = slots_sofar + ownslots;
  if ( slots_sofar <= totalslots) {
	return ownslots;
  } else {
	return (totalslots-slots_temp);
  }
}


// This function is a traditional bubble sort.
// Note that "sort_field" can be ascending/descending order
void BSScheduler::bubble_sort (int arrayLength, con_data_alloc array[], int sort_field) {
   int i, j, flag = 1;
   con_data_alloc temp;

   for (int k = 0; k < arrayLength; k++) {
//        debug10 ("-Before i :%d, cid :%d, counter :%d, req_slots :%d, mod :%d\n", k, array[k].cid, array[k].counter, array[k].req_slots, (int)array[k].mod_rate);
   }

   if (sort_field == 0) {
   for (i = 1; (i <= arrayLength) && flag; i++) {
       flag = 0;
       for (j = 0; j < (arrayLength -1); j++) {
           if (array[j+1].req_slots < array[j].req_slots) {
              temp.cid = array[j].cid;
              temp.direction = array[j].direction;
              temp.mod_rate = array[j].mod_rate;
              temp.req_slots = array[j].req_slots;
              temp.grant_slots = array[j].grant_slots;
              temp.weight = array[j].weight;
              temp.counter = array[j].counter;

              array[j].cid = array[j+1].cid;
              array[j].direction = array[j+1].direction;
              array[j].mod_rate = array[j+1].mod_rate;
              array[j].req_slots = array[j+1].req_slots;
              array[j].grant_slots = array[j+1].grant_slots;
              array[j].weight = array[j+1].weight;
              array[j].counter = array[j+1].counter;

              array[j+1].cid = temp.cid;
              array[j+1].direction = temp.direction;
              array[j+1].mod_rate = temp.mod_rate;
              array[j+1].req_slots = temp.req_slots;
              array[j+1].grant_slots = temp.grant_slots;
              array[j+1].weight = temp.weight;
              array[j+1].counter = temp.counter;
              flag = 1;
           }//end if
       }//end 2nd for
     }//end 1st for
   }//end sort_field == 0

   if (sort_field == 1) {
   for (i = 1; (i <= arrayLength) && flag; i++) {
       flag = 0;
       for (j = 0; j < (arrayLength -1); j++) {
           if (array[j+1].counter > array[j].counter) {
              temp.cid = array[j].cid;
              temp.direction = array[j].direction;
              temp.mod_rate = array[j].mod_rate;
              temp.req_slots = array[j].req_slots;
              temp.grant_slots = array[j].grant_slots;
              temp.weight = array[j].weight;
              temp.counter = array[j].counter;

              array[j].cid = array[j+1].cid;
              array[j].direction = array[j+1].direction;
              array[j].mod_rate = array[j+1].mod_rate;
              array[j].req_slots = array[j+1].req_slots;
              array[j].grant_slots = array[j+1].grant_slots;
              array[j].weight = array[j+1].weight;
              array[j].counter = array[j+1].counter;

              array[j+1].cid = temp.cid;
              array[j+1].direction = temp.direction;
              array[j+1].mod_rate = temp.mod_rate;
              array[j+1].req_slots = temp.req_slots;
              array[j+1].grant_slots = temp.grant_slots;
              array[j+1].weight = temp.weight;
              array[j+1].counter = temp.counter;
              flag = 1;
           }//end if
       }//end 2nd for
     }//end 1st for
   }//end sort_field == 1

   for (int k = 0; k < arrayLength; k++) {
//        debug10 ("-After i :%d, cid :%d, counter :%d, req_slots :%d, mod :%d\n", k, array[k].cid, array[k].counter, array[k].req_slots, (int)array[k].mod_rate);
   }

   return;   //arrays are passed to functions by address; nothing is returned
}


// This function is used to check if the virutall allocation exists
int BSScheduler::doesvirtual_allocexist(int num_of_entries, int cid) {
  for(int i = 0; i<num_of_entries-1; i++){
	if (virtual_alloc[i].cid == cid) {
		return cid;
	}
  }
  return -1;
}


// This function is used to add the virtual burst regardless of DL_MAP_IE
int BSScheduler::addslots_withoutdlmap(int num_of_entries, int byte, int slots, int cid) {
  for(int i = 0; i<num_of_entries-1; i++){
	if (virtual_alloc[i].cid == cid) {
		virtual_alloc[i].byte += byte;
		virtual_alloc[i].numslots += slots;
		return i;
	}
  }
  return -1;
}


// This function is used caculate #slots used so far
int BSScheduler::check_overallocation(int num_of_entries){

  int slots_sofar = 0;
  for(int i = 0; i<num_of_entries; i++){
        slots_sofar = slots_sofar + virtual_alloc[i].numslots;
  }
  return slots_sofar;
}


/**
 * Schedule bursts/packets
 */
void BSScheduler::schedule ()
{
  //The scheduler will perform the following steps:
  //1-Clear UL map
  //2-Allocate CDMA-Ranging region for initial ranging (2 OFDMs) and bw-req (1 OFDMs)
  //  In this version we reserved a whold column resulting 5-subchannel waste
  //3-Allocate UL data allocation (all-cid) per-MS allocation
  //  In this version we do not consider horizontal stripping yet
  //4-Clear DL map
  //5-Allocate Burst for Broadcast message
  //  In this version we allocate one burst for each broadcast message type say DL_MAP, UL_MAP, DCD, UCD, other other broadcast message
  //6-Allocate DL data allocation (all-cid) per-cid allocation
  //7-Assign burst -> physical allocation
  //Note that, we do not simulate FCH in this version. We will do that in next version.

  Packet *p;
  struct hdr_cmn *ch;
  double txtime; //tx time for some data (in second)
  int txtime_s;  //number of symbols used to transmit the data
  DlBurst *db;
  PeerNode *peer;
  int Channel_num=0;

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
	if(!(n->getArqStatus ()->arq_feedback_queue_) || (n->getArqStatus ()->arq_feedback_queue_->length() == 0)){
	  continue;
        } 
	else {
	  peernode = n->getPeerNode ();
	  if(peernode->getOutData () != NULL && peernode->getOutData ()->queueLength () != 0 && getMac()->arqfb_in_dl_data_) {
	    out_datacnx_exists = true;
	  }
	  if(out_datacnx_exists == false) {
	    //debug2("ARQ BS: Feedback in Basic Cid \n");
	    basic = peernode->getBasic (OUT_CONNECTION);
	    pfb = mac_->getPacket ();
	    wimaxHdrMap= HDR_MAC802_16(pfb);
	    wimaxHdrMap->header.cid = basic->get_cid ();	
	    wimaxHdrMap->num_of_acks = 0;
	  }
	  else {
	    // Commented by Barun : 21-Sep-2011
	    //debug2("ARQ BS : Feedback in data Cid \n");
	    OutData = peernode->getOutData ();
	    pfb = OutData->dequeue ();
	    wimaxHdrMap= HDR_MAC802_16(pfb);
	    if(wimaxHdrMap->header.type_arqfb == 1)
	      {
		//debug2("ARQ BS: Feedback already present, do nothing \n");
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
	  //debug2("ARQ : In BSScheduler: Enqueueing an feedback cid: %d arq_ie->fsn:%d \n", wimaxHdr->arq_ie[0].cid,  wimaxHdr->arq_ie[0].fsn);
          if(out_datacnx_exists == false) {	
     	    // If I am here then the Ack packet has been created, so we will enqueue it in the Basic Cid
     	    basic->enqueue (pfb);
          }
          else
	    OutData->enqueue_head (pfb);  	
     	}
      }
  }

  peer = mac_->getPeerNode_head();
  if(peer)
    for (int i=0; i<mac_->getNbPeerNodes() ;i++) {
      //peer->setchannel(++Channel_num);
      peer->setchannel((peer->getchannel())+1);
 
      if(peer->getchannel()>999) peer->setchannel(2); 

      peer = peer->next_entry();
    }

  if(Channel_num>999) Channel_num = 2;//GetInitialChannel();

  /// rpi random channel allocation 

  OFDMAPhy *phy = mac_->getPhy();
  FrameMap *map = mac_->getMap();
  int nbPS = (int) floor((mac_->getFrameDuration()/phy->getPS()));
#ifdef DEBUG_WIMAX
  assert (nbPS*phy->getPS()<=mac_->getFrameDuration()); //check for rounding errors
#endif

  int nbPS_left = nbPS - mac_->phymib_.rtg - mac_->phymib_.ttg;
  int nbSymbols = (int) floor((phy->getPS()*nbPS_left)/phy->getSymbolTime());  // max num of OFDM symbols available per frame. 
  assert (nbSymbols*phy->getSymbolTime()+(mac_->phymib_.rtg + mac_->phymib_.ttg)*phy->getPS() < mac_->getFrameDuration());
  int dlduration = DL_PREAMBLE;                             //number of symbols currently used for downlink
  int maxdlduration = (int) (nbSymbols / (1.0/dlratio_)); //number of symbols for downlink

/////////////////////
// In next version, proper use of DL:UL symbols will be calculated.
//  int nbSymbols_wo_preamble = nbSymbols - DL_PREAMBLE;
//  int nbSymbols_wo_preamble = nbSymbols;
//
//  int virtual_maxdld = (int) (nbSymbols_preamble / (1.0/dlratio_)); //number of symbols for downlink
//  int virtual_maxuld = nbSymbols_wo_preamble - virtual_maxdld;
//  int counter_maxdld = virtual_maxdld;
//
//  for (int i=counter_maxdld; i<nbSymbols_wo_preamble; i++) {
//     if ( ((virtual_maxdld % 2) == 0) && ((virtual_maxuld % 3) == 0) ) {
//	break;
//     } else {
//
//     }
//  }
/////////////////////

  int subchannel_offset = 0;
  int nbdlbursts = 0;
  int maxulduration = nbSymbols - maxdlduration;                //number of symbols for uplink
  int ulduration = 0;           		                //number of symbols currently used for uplink
  int nbulpdus = 0;
  int total_dl_subchannel_pusc = 30;
  int total_ul_subchannel_pusc = 35;
  int total_dl_slots_pusc = floor((double)(maxdlduration-DL_PREAMBLE)/2)*total_dl_subchannel_pusc;
  int total_ul_slots_pusc = floor((double)maxulduration/3)*total_ul_subchannel_pusc;
  int numie;
  int number_ul_ie = 0;
  int number_dl_ie = 0;
  
  //*debug10 ("\n---------------------------------------------------\n");
//| | | |       => symbols
//0 1 2 3       => dlduration
  //*debug10 ("Start BS Scheduling: TotalSymbols :%d, MAXDL :%d, MAXUL :%d, Preamble :%d, dlduration after preamble :%d\n", nbSymbols, maxdlduration, maxulduration, DL_PREAMBLE, dlduration);

#ifdef DEBUG_WIMAX
  assert ((nbSymbols*phy->getSymbolPS()+mac_->phymib_.rtg + mac_->phymib_.ttg)*phy->getPS()< mac_->getFrameDuration());
#endif
#ifdef DEBUG_WIMAX
  assert (maxdlduration*phy->getSymbolTime()+mac_->phymib_.rtg*phy->getPS()+maxulduration*phy->getSymbolTime()+mac_->phymib_.ttg*phy->getPS() < mac_->getFrameDuration());
#endif
  
  mac_->setMaxDlduration (maxdlduration);
  mac_->setMaxUlduration (maxulduration);

  //============================UL allocation=================================
  //1 and 2 - Clear Ul_MAP and Allocate inital ranging and bw-req regions
  int ul_subframe_subchannel_offset = 0;
  int total_UL_num_subchannel = phy->getNumsubchannels(UL_);
	
  mac_->getMap()->getUlSubframe()->setStarttime (maxdlduration*phy->getSymbolPS()+mac_->phymib_.rtg);
  
  while (mac_->getMap()->getUlSubframe()->getNbPdu()>0) {
      PhyPdu *pdu = mac_->getMap()->getUlSubframe()->getPhyPdu(0);
      pdu->removeAllBursts();
      mac_->getMap()->getUlSubframe()->removePhyPdu(pdu);
      delete (pdu);
  }

  //Set contention slots for cdma initial ranging
  UlBurst *ub = (UlBurst*)mac_->getMap()->getUlSubframe()->addPhyPdu (nbulpdus++,0)->addBurst (0);
  ub->setIUC (UIUC_INITIAL_RANGING);
  int int_rng_num_sub = (int)(init_contention_size_*CDMA_6SUB);		//init_contention_size is set to 5 in this version
  int int_rng_num_sym = 2;	
  ub->setDuration (int_rng_num_sym);
  ub->setStarttime (ulduration); 	
  ub->setSubchannelOffset (0);
  ub->setnumSubchannels(int_rng_num_sub);
  ul_subframe_subchannel_offset = (0);
  // Commented by Barun : 21-Sep-2011
  //debug10 ("UL.Initial Ranging, contention_size :%d, 1opportunity = 6sub*2symbols, ulduration :%d, initial_rng_duration :%d, updated ulduration :%d\n", init_contention_size_, ulduration, int_rng_num_sym, ulduration + int_rng_num_sym);

  ulduration += int_rng_num_sym; 

  //Set contention slots for cdma bandwidth request
  ub = (UlBurst*)mac_->getMap()->getUlSubframe()->addPhyPdu (nbulpdus++,0)->addBurst (0);
  ub->setIUC (UIUC_REQ_REGION_FULL);
  int bw_rng_num_sub = (int)(bw_req_contention_size_*CDMA_6SUB);	//bw_req_contention_size is set to 5 in this version
  int bw_rng_num_sym = 1;
  ub->setDuration (bw_rng_num_sym);
  ub->setStarttime (ulduration);
  ub->setSubchannelOffset (0);
  ub->setnumSubchannels(bw_rng_num_sub);
  ul_subframe_subchannel_offset = (0);
  // Commented by Barun : 21-Sep-2011
  //debug10 ("UL.Bw-req, contention_size :%d, 1opportunity = 6sub*1symbols, ulduration :%d, bw-req_duration :%d, updated ulduration :%d\n", bw_req_contention_size_, ulduration, bw_rng_num_sym, ulduration + bw_rng_num_sym);

  ulduration += bw_rng_num_sym; 

  mac_->setStartUlduration (ulduration);

  peer = mac_->getPeerNode_head();

  //*if(ulduration > maxulduration ) { debug2 (" not enough UL symbols to allocate \n " ); }

  // This cdma_flag is used in case there is no peer yet however the allocation for cdma_transmission oppportunity needed to be allocated by the BS-UL scheduler
  int cdma_flag = 0;
  Connection *con1;
  con1 = mac_->getCManager()->get_in_connection();
  while (con1!=NULL){
    if(con1->get_category() == CONN_INIT_RANGING) {
        if (con1->getCDMA()>0) {
           cdma_flag = 2;
        }
    }
    con1 = con1->next_entry();
  }

  //Call ul_stage2 to allocate the uplink resource
  if ( (peer)  || (cdma_flag>0) ) {
    if(maxulduration > ulduration) {
    // Commented by Barun : 21-Sep-2011
	//debug2 ("UL.Before going to ul_stage2 (data allocation) Frame duration :%f, PSduration :%e, Symboltime :%e, nbPS :%d, rtg :%d, ttg :%d, nbPSleft :%d, nbSymbols :%d\n", mac_->getFrameDuration(), phy->getPS(), phy->getSymbolTime(), nbPS, mac_->phymib_.rtg, mac_->phymib_.ttg, nbPS_left, nbSymbols);
	// Commented by Barun : 21-Sep-2011
	//debug2 ("\tmaxdlduration :%d, maxulduration :%d, numsubchannels :%d, ulduration :%d\n", maxdlduration, (maxulduration), phy->getNumsubchannels(/*phy->getPermutationscheme (),*/ UL_), ulduration);

	mac802_16_ul_map_frame * ulmap = ul_stage2 (mac_->getCManager()->get_in_connection (),phy->getNumsubchannels( UL_), (maxulduration-ulduration), ulduration, VERTICAL_STRIPPING );

// we don't support horizontal stripping in this version
// mac802_16_ul_map_frame * ulmap = ul_stage2 (mac_->getCManager()->get_in_connection (),phy->getNumsubchannels( UL_), maxulduration-ulduration,ulduration, HORIZONTAL_STRIPPING );

// From UL_MAP_IE, we map the allocation into UL_BURST
	number_ul_ie = (int)ulmap->nb_ies;
	for (numie = 0 ; numie < (int) ulmap->nb_ies ; numie++) {
	    mac802_16_ulmap_ie ulmap_ie = ulmap->ies[numie];
	    ub = (UlBurst*) mac_->getMap()->getUlSubframe()->addPhyPdu (nbulpdus++,0)->addBurst (0);
	    
#ifdef SAM_DEBUG
        // Commented by Barun : 21-Sep-2011
	    //debug2 ("In side ulmap adding to burst,  UL_MAP_IE.UIUC = %d, UL_MAP_IE = %d", ulmap_ie.uiuc, ulmap->nb_ies);
#endif
	    ub->setCid (ulmap_ie.cid);
	    ub->setIUC (ulmap_ie.uiuc);
	    ub->setStarttime (ulmap_ie.symbol_offset);
	    ub->setDuration (ulmap_ie.num_of_symbols);
	    ub->setSubchannelOffset (ulmap_ie.subchannel_offset);
	    ub->setnumSubchannels (ulmap_ie.num_of_subchannels);
            ub->setB_CDMA_TOP (ulmap_ie.cdma_ie.subchannel);
            ub->setB_CDMA_CODE (ulmap_ie.cdma_ie.code);

        // Commented by Barun : 21-Sep-2011
	    //debug2("UL.Data region (Addburst): symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n", ulmap_ie.symbol_offset, ulmap_ie.num_of_symbols, ulmap_ie.subchannel_offset, ulmap_ie.num_of_subchannels);
            //debug2("   Addburst cdma code :%d, cdma top :%d\n", ub->getB_CDMA_CODE(), ub->getB_CDMA_TOP());
	}
	free (ulmap);
    }
  }


  //End of map
  //Note that in OFDMA, there is no end of UL map, it'll be removed in next version; however, this is a virtual end of map say there is no transmitted packet/message
  ub = (UlBurst*)mac_->getMap()->getUlSubframe()->addPhyPdu (nbulpdus,0)->addBurst (0);
  ub->setIUC (UIUC_END_OF_MAP);
  ub->setStarttime (maxulduration);
  ub->setSubchannelOffset (0); //Richard: changed 1->0
  ub->setnumSubchannels (phy->getNumsubchannels(UL_));
  // Commented by Barun : 21-Sep-2011
  //debug10 ("UL.EndofMAP: (addBurst_%d) maxuluration :%d, lastbursts :%d, nbulpdus :%d\n", nbdlbursts, maxulduration, nbdlbursts, nbulpdus);

  //============================DL allocation=================================
  //debug10 ("DL.Scheduler: FrameDuration :%5.4f, PSduration :%e, symboltime :%e, nbPS :%d, rtg :%d, ttg :%d, nbPSleft :%d, nbSymbols :%d, dlratio_ :%5.2f\n", mac_->getFrameDuration(), phy->getPS(), phy->getSymbolTime(), nbPS, mac_->phymib_.rtg, mac_->phymib_.ttg, nbPS_left, nbSymbols, dlratio_);

  map->getDlSubframe()->getPdu()->removeAllBursts();
  bzero(virtual_alloc, MAX_MAP_IE*sizeof(virtual_burst));
  index_burst = 0;

//1. Virtual DL_MAP
//Note that, we virtually allocate all burst into "virtual_alloc[]", then we will physically allocate/map those allocation into burst/physical later. The reason is because we have variable part of DL_MAPs
//"index_burst" is the #total_burst including each DL_MAP, UL_MAP, DCD, UCD, other broadcast message and each CID allocation
//In this version, the allocation is per CID/burst for downlink
  int fixed_byte_dl_map = DL_MAP_HEADER_SIZE;
  int rep_fixed_dl = Repetition_code_;
  Ofdm_mod_rate fixed_dl_map_mod = map->getDlSubframe()->getProfile (map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC())->getEncoding();
  int fixed_dl_map_slots = (int) rep_fixed_dl*ceil((double)fixed_byte_dl_map/(double)phy->getSlotCapacity(fixed_dl_map_mod, DL_)); 
  virtual_alloc[index_burst].alloc_type = 0;
  virtual_alloc[index_burst].cid = BROADCAST_CID;
  virtual_alloc[index_burst].n_cid = 0;
  virtual_alloc[index_burst].iuc = map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC();
  virtual_alloc[index_burst].preamble = true;
  virtual_alloc[index_burst].numslots = fixed_dl_map_slots;
  virtual_alloc[index_burst].mod = fixed_dl_map_mod;
  virtual_alloc[index_burst].byte = fixed_byte_dl_map;
  virtual_alloc[index_burst].rep =  Repetition_code_;
  virtual_alloc[index_burst].dl_ul = 0;
  virtual_alloc[index_burst].ie_type = 0;
  index_burst++;

//2. Virtual UL_MAP:
  int fixed_byte_ul_map = UL_MAP_HEADER_SIZE + UL_MAP_IE_SIZE*number_ul_ie;
  int rep_fixed_ul = Repetition_code_;
  Ofdm_mod_rate fixed_ul_map_mod = map->getDlSubframe()->getProfile (map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC())->getEncoding();
  int fixed_ul_map_slots = (int) rep_fixed_ul*ceil((double)fixed_byte_ul_map/(double)phy->getSlotCapacity(fixed_ul_map_mod, DL_)); 
  virtual_alloc[index_burst].alloc_type = 1;
  virtual_alloc[index_burst].cid = BROADCAST_CID;
  virtual_alloc[index_burst].n_cid = 0;
  virtual_alloc[index_burst].iuc = map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC();
  virtual_alloc[index_burst].preamble = true;
  virtual_alloc[index_burst].numslots = fixed_ul_map_slots;
  virtual_alloc[index_burst].mod = fixed_ul_map_mod;
  virtual_alloc[index_burst].byte = fixed_byte_ul_map;
  virtual_alloc[index_burst].rep =  Repetition_code_;
  virtual_alloc[index_burst].dl_ul = 0;
  virtual_alloc[index_burst].ie_type = 0;
  int add_ie_to_dlmap = increase_dl_map_ie(index_burst, total_dl_slots_pusc, 1);
  if (add_ie_to_dlmap <0 ) {
	debug10 ("Panic: not enough space for UL_MAP\n");
	exit(1);
  }
  index_burst++;

//3. Virtual DCD
  if (getMac()->sendDCD || map->getDlSubframe()->getCCC()!= getMac()->dlccc_) {
      p = map->getDCD();
      ch = HDR_CMN(p);
      int byte_dcd = ch->size();
      int rep_dcd = 1;
      Ofdm_mod_rate dcd_mod = map->getDlSubframe()->getProfile (map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC())->getEncoding();
      int dcd_slots = (int) rep_dcd*ceil((double)byte_dcd/(double)phy->getSlotCapacity(dcd_mod, DL_)); 
      virtual_alloc[index_burst].alloc_type = 2;
      virtual_alloc[index_burst].cid = BROADCAST_CID;
      virtual_alloc[index_burst].n_cid = 1;
      virtual_alloc[index_burst].iuc = map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC();
      virtual_alloc[index_burst].preamble = false;
      virtual_alloc[index_burst].numslots = dcd_slots;
      virtual_alloc[index_burst].mod = dcd_mod;
      virtual_alloc[index_burst].byte = byte_dcd;
      virtual_alloc[index_burst].rep =  rep_dcd;
      virtual_alloc[index_burst].dl_ul = 0;
      virtual_alloc[index_burst].ie_type = 0;
      int add_ie_to_dlmap = increase_dl_map_ie(index_burst, total_dl_slots_pusc, 1);
      if (add_ie_to_dlmap <0 ) {
	debug10 ("Panic: not enough space for DCD\n");
	exit(1);
      } else {
	//debug10 ("DCD size :%d bytes, %d slots\n", ch->size(), dcd_slots);
      }
      index_burst++;
  }

//4. Virtual UCD
  if (getMac()->sendUCD || map->getUlSubframe()->getCCC()!= getMac()->ulccc_) {
      p = map->getUCD();
      ch = HDR_CMN(p);
      int byte_ucd = ch->size();
      int rep_ucd = 1;
      Ofdm_mod_rate ucd_mod = map->getDlSubframe()->getProfile (map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC())->getEncoding();
      int ucd_slots = (int) rep_ucd*ceil((double)byte_ucd/(double)phy->getSlotCapacity(ucd_mod, DL_)); 
      virtual_alloc[index_burst].alloc_type = 3;
      virtual_alloc[index_burst].cid = BROADCAST_CID;
      virtual_alloc[index_burst].n_cid = 1;
      virtual_alloc[index_burst].iuc = map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC();
      virtual_alloc[index_burst].preamble = false;
      virtual_alloc[index_burst].numslots = ucd_slots;
      virtual_alloc[index_burst].mod = ucd_mod;
      virtual_alloc[index_burst].byte = byte_ucd;
      virtual_alloc[index_burst].rep =  rep_ucd;
      virtual_alloc[index_burst].dl_ul = 0;
      virtual_alloc[index_burst].ie_type = 0;
      int add_ie_to_dlmap = increase_dl_map_ie(index_burst, total_dl_slots_pusc, 1);
      if (add_ie_to_dlmap <0 ) {
	debug10 ("Panic: not enough space for UCD\n");
	exit(1);
      } else {
	//debug10 ("UCD size :%d bytes, %d slots\n", ch->size(), ucd_slots);
      }
      index_burst++;
  }
 
  int check_dl_slots = check_overallocation(index_burst);

    // Commented by Barun : 21-Sep-2011
  //debug10 ("DL.broadcast messages (slots): (v)Fixed_DL_MAP :%d, (v)UL_MAP :%d, DCD :%d, UCD :%d, MaxDL :%d\n", virtual_alloc[0].numslots, virtual_alloc[1].numslots, virtual_alloc[2].numslots, virtual_alloc[3].numslots, total_dl_slots_pusc);
  //debug10 ("\t(bytes): (v)Fixed_DL_MAP :%f, (v)UL_MAP :%f, DCD :%f, UCD :%f\n", virtual_alloc[0].byte, virtual_alloc[1].byte, virtual_alloc[2].byte, virtual_alloc[3].byte);

  if (check_dl_slots > total_dl_slots_pusc) {
     debug10("Panic : not enough dl_slots\n");
     exit(1);
  } 

//5. Virtual Other broadcast
  if (mac_->getCManager()->get_connection (BROADCAST_CID, OUT_CONNECTION)->queueByteLength()>0) {
      Ofdm_mod_rate bc_mod = map->getDlSubframe()->getProfile (map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC())->getEncoding();

      int slot_br = 0;
      Connection *c_tmp;
      c_tmp = mac_->getCManager()->get_connection (BROADCAST_CID, OUT_CONNECTION);
      int real_bytes = 0;
      int i_packet = 0;
      Packet *np;
      //debug10 ("Retrive connection :%d, qlen :%d\n", c_tmp->get_cid(), c_tmp->queueLength());
      for (int j_p = 0; j_p<c_tmp->queueLength(); j_p++) {
        if ( (np = c_tmp->queueLookup(i_packet)) != NULL ) {
            int p_size = hdr_cmn::access(np)->size();
            // Commented by Barun : 21-Sep-2011
            //debug10 ("\t Other Broadcast CON CID :%d, packet-id :%d, q->byte :%d, q->len :%d, packet_size :%d, frag_no :%d, frag_byte :%d, frag_stat :%d, real_bytes :%d\n", c_tmp->get_cid(), i_packet, c_tmp->queueByteLength(), c_tmp->queueLength(), p_size, c_tmp->getFragmentNumber(), c_tmp->getFragmentBytes(), (int)c_tmp->getFragmentationStatus(), real_bytes );            
            i_packet++;
            int num_of_slots = (int) ceil((double)p_size/(double)phy->getSlotCapacity(bc_mod,DL_));
            real_bytes = real_bytes + (int) ceil((double)num_of_slots*(double)(phy->getSlotCapacity(bc_mod,UL_)));
        }
      }

      int bc_slots = (int) ceil((double)real_bytes/(double)phy->getSlotCapacity(bc_mod,DL_)); 
      virtual_alloc[index_burst].alloc_type = 4;
      virtual_alloc[index_burst].cid = BROADCAST_CID;
      virtual_alloc[index_burst].n_cid = 1;
      virtual_alloc[index_burst].iuc = map->getDlSubframe()->getProfile (DIUC_PROFILE_2)->getIUC();
      virtual_alloc[index_burst].preamble = false;
      virtual_alloc[index_burst].numslots = bc_slots;
      virtual_alloc[index_burst].byte = real_bytes;
      virtual_alloc[index_burst].rep =  1;
      virtual_alloc[index_burst].dl_ul = 0;
      virtual_alloc[index_burst].ie_type = 0;
      int add_ie_to_dlmap = increase_dl_map_ie(index_burst, total_dl_slots_pusc, 1);
      if (add_ie_to_dlmap <0 ) {
	debug10 ("Panic: not enough space for other Broadcast message\n");
	exit(1);
      }

        // Commented by Barun : 21-Sep-2011
      //debug10 ("DL.other_broadcast messages (slots): %d, (bytes): %f\n", virtual_alloc[index_burst].numslots, virtual_alloc[index_burst].byte);
      index_burst++;
    }
//End of Virtual allocation for all broadcast message

  int index_burst_before_data = index_burst;

/*
//This comment is used to map the allocation directly to the burst. Note that we don't map now but instead we will map after we allocate the data portion
//Add virtual allocation to physical DL burst
  int virtual_symbol_offset = DL_PREAMBLE;
  int virtual_subchannel_offset = 0;
  int virtual_accum_slots = 0;
  for (int i=0; i<index_burst; i++) {
      db = (DlBurst*) map->getDlSubframe()->getPdu ()->addBurst (nbdlbursts++);
      db->setCid (virtual_alloc[i].cid);
      db->setIUC (virtual_alloc[i].iuc);
      db->setPreamble(virtual_alloc[i].preamble); 
      db->setStarttime(virtual_symbol_offset); 
      db->setSubchannelOffset(virtual_subchannel_offset);

      int num_of_subchannel = virtual_alloc[i].numslots;
      int num_of_symbol = (int) ceil((double)(virtual_subchannel_offset + num_of_subchannel)/total_dl_subchannel_pusc)*2;

      db->setnumSubchannels(num_of_subchannel);
      db->setDuration(num_of_symbol); 			

      virtual_accum_slots += virtual_alloc[i].numslots;
//      int accum_duration = DL_PREAMBLE + ceil((double)virtual_accum_slots/(double)total_dl_subchannel_pusc);

      debug10("Add DL bursts: index :%d, cid :%d, iuc :%d, subchannel_offset :%d, symbol_offset :%d, num_slots (num_subchannel) :%d, num_symbol :%d, accum_slots :%d\n", i, virtual_alloc[i].cid, virtual_alloc[i].iuc, virtual_subchannel_offset, virtual_symbol_offset, virtual_alloc[i].numslots, num_of_symbol, virtual_accum_slots);

      virtual_subchannel_offset = (virtual_subchannel_offset + num_of_subchannel)%(total_dl_subchannel_pusc);
      virtual_symbol_offset += num_of_symbol - 2;
   }
*/

  int check_dl_duration = ceil((double)check_overallocation(index_burst)/(double)total_dl_subchannel_pusc);
  dlduration = check_dl_duration;
  int num_of_slots = 0;
  int num_of_subchannel = 0;
  int num_of_symbol = 0;
  int total_DL_num_subchannel = phy->getNumsubchannels(DL_);

  // from here start ofdma DL -- call the scheduler API- here DLMAP    ------------------------------------
  peer = mac_->getPeerNode_head();

  subchannel_offset = 0;
  mac802_16_dl_map_frame *dl_map;

  // Call dl_stage2 to allocate the downlink resource
  if (peer) {
    if(maxdlduration > dlduration) {
    // Commented by Barun : 21-Sep-2011
	//debug2 ("DL.Before going to dl-stage2 (data allocation), Frame Duration :%5.4f, PSduration :%e, symboltime :%e, nbPS :%d, rtg :%d, ttg :%d, PSleft :%d, nbSymbols :%d\n", mac_->getFrameDuration(), phy->getPS(), phy->getSymbolTime(), nbPS, mac_->phymib_.rtg, mac_->phymib_.ttg, nbPS_left, nbSymbols);
	//debug10 ("\tStartdld :%d, Enddld :%d, Dldsize :%d, Uldsize :%d, Numsubchannels (35 or 30) :%d \n", dlduration, maxdlduration, (maxdlduration-dlduration), maxulduration, phy->getNumsubchannels(/*phy->getPermutationscheme (), */DL_));

  // We do not consider the stripping here, we will do the stripping technique once we map the virtual allocation into burst/physical later
  // This is a previous dl_statge2 vertical stripping function.
  // mac802_16_dl_map_frame * dl_map =  dl_stage2(mac_->getCManager()->get_out_connection (), subchannel_offset, phy->getNumsubchannels(DL_),(maxdlduration-dlduration), dlduration, VERTICAL_STRIPPING, total_dl_slots_pusc);

  // This is a previous dl_stage2 horizontal stripping function
  // mac802_16_dl_map_frame * dl_map =  dl_stage2(mac_->getCManager()->get_out_connection (), subchannel_offset, phy->getNumsubchannels(DL_),(maxdlduration-dlduration-1), dlduration, HORIZONTAL_STRIPPING);
  // mac802_16_dl_map_frame * dl_map =  dl_stage2(mac_->getCManager()->get_out_connection (), phy->getNumsubchannels(DL_),(maxdlduration-dlduration-1), dlduration,VERTICAL_STRIPPING);
  // mac802_16_dl_map_frame * dl_map =  dl_stage2(mac_->getCManager()->get_out_connection (), phy->getNumsubchannels(/*phy->getPermutationscheme (), */DL_),dlduration, (dlduration+1),VERTICAL_STRIPPING);

	dl_map =  dl_stage2(mac_->getCManager()->get_out_connection (), subchannel_offset, phy->getNumsubchannels(DL_),(maxdlduration-dlduration), dlduration, VERTICAL_STRIPPING, total_dl_slots_pusc);
	number_dl_ie = (int) dl_map->nb_ies;

/*
// we don't add the allocation into burst here
	debug10 ("nb_iew :%d\n", dl_map->nb_ies);
	for (numie = 0; numie < (int) dl_map->nb_ies ; numie++) {
	    mac802_16_dlmap_ie  dlmap_ie = dl_map->ies[numie];

	    debug10 ("DL.1.addburst, #bursts %d, CID :%d, DIUC :%d, SymbolOffset :%d, #symbols :%d, SubchannelOffset :%d, #subchannels :%d\n", nbdlbursts, dlmap_ie.cid, dlmap_ie.diuc,dlmap_ie.symbol_offset, dlmap_ie.num_of_symbols, dlmap_ie.subchannel_offset, dlmap_ie.num_of_subchannels); 
	    addDlBurst (nbdlbursts++,dlmap_ie.cid, dlmap_ie.diuc,dlmap_ie.symbol_offset, dlmap_ie.num_of_symbols, dlmap_ie.subchannel_offset, dlmap_ie.num_of_subchannels); 
	}
	free (dl_map);
*/
      }
  }

  // Add all virtual allocation (brodcast and data portion) to physical DL burst
  // Note that #subchannels = #slots in this version
  int virtual_symbol_offset = DL_PREAMBLE;
  int virtual_subchannel_offset = 0;
  int virtual_accum_slots = 0;
  for (int i=0; i<index_burst; i++) {
      db = (DlBurst*) map->getDlSubframe()->getPdu ()->addBurst (nbdlbursts++);
      db->setCid (virtual_alloc[i].cid);
      db->setIUC (virtual_alloc[i].iuc);
      db->setPreamble(virtual_alloc[i].preamble); 
      db->setStarttime(virtual_symbol_offset); 
      db->setSubchannelOffset(virtual_subchannel_offset);

      int num_of_subchannel = virtual_alloc[i].numslots;
      int num_of_symbol = (int) ceil((double)(virtual_subchannel_offset + num_of_subchannel)/total_dl_subchannel_pusc)*2;

      db->setnumSubchannels(num_of_subchannel);
      db->setDuration(num_of_symbol); 			

      virtual_accum_slots += virtual_alloc[i].numslots;

        // Commented by Barun : 21-Sep-2011
      //debug10("DL.add bursts: index :%d, cid :%d, iuc :%d, subchannel_offset :%d, symbol_offset :%d, num_slots (num_subchannel) :%d, num_symbol :%d, accum_slots :%d\n", i, virtual_alloc[i].cid, virtual_alloc[i].iuc, virtual_subchannel_offset, virtual_symbol_offset, virtual_alloc[i].numslots, num_of_symbol, virtual_accum_slots);

      virtual_subchannel_offset = (virtual_subchannel_offset + num_of_subchannel)%(total_dl_subchannel_pusc);
      virtual_symbol_offset += num_of_symbol - 2;
   }

/*
//This comment is the previous version of adding DL burst
  if (number_dl_ie > 0) {
      for (numie = 0; numie < (int) dl_map->nb_ies ; numie++) {
           mac802_16_dlmap_ie  dlmap_ie = dl_map->ies[numie];

	   debug10 ("DL.1.addburst, #bursts %d, CID :%d, DIUC :%d, SymbolOffset :%d, #symbols :%d, SubchannelOffset :%d, #subchannels :%d\n", nbdlbursts, dlmap_ie.cid, dlmap_ie.diuc,dlmap_ie.symbol_offset, dlmap_ie.num_of_symbols, dlmap_ie.subchannel_offset, dlmap_ie.num_of_subchannels); 
	   addDlBurst (nbdlbursts++,dlmap_ie.cid, dlmap_ie.diuc,dlmap_ie.symbol_offset, dlmap_ie.num_of_symbols, dlmap_ie.subchannel_offset, dlmap_ie.num_of_subchannels); 
      }
      free (dl_map);
  }
*/

#ifdef DEBUG_WIMAX
  assert (dlduration <= maxdlduration);
#endif

/*
  // Add the End of map element (optional)
  // Note that, this if for OFDM only, we do not have this end of DL map for OFDMA
  db = (DlBurst*) map->getDlSubframe()->getPdu ()->addBurst (nbdlbursts);
  db->setIUC (DIUC_END_OF_MAP);
  debug2("\t maxdlduration is [%d] #_of_burst is [%d]\n",maxdlduration, map->getDlSubframe()->getPdu ()->getNbBurst());
  db->setStarttime (maxdlduration);  // richard - question - what is the duration of this and why this is used. 
  db->setSubchannelOffset (0); //Richard: changed 1->0
  db->setnumSubchannels (phy->getNumsubchannels(DL_));
  debug10 ("DL.EndofMAP: (addBurst_%d) maxdluration :%d, lastbursts :%d, nbulpdus :%d\n", nbdlbursts, maxdlduration, nbdlbursts, nbulpdus);

*/

  // Now transfert the packets to the physical bursts starting with broadcast messages
  // In this version, we can directly map burst information from virtual burst into physical burst.
  // 2D rectangular allocation will be considered in next version
  int b_data = 0;
  int max_data = 0;
  hdr_mac802_16 *wimaxHdr;
  double txtime2 = 0;
  int offset = 0;
  int subchannel_offset_wimaxhdr = 0;
  int num_burst_before_data = 0;
  Burst *b;
  int i_burst = 0;

  b = map->getDlSubframe()->getPdu ()->getBurst (i_burst);
  i_burst++;
  b_data  = 0;
  txtime2 = 0;
  offset  = 0;
  subchannel_offset_wimaxhdr = 0;
  max_data = phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding())-b_data;

  p = map->getDL_MAP();
  num_burst_before_data++;
  ch = HDR_CMN(p);
  offset = b->getStarttime( );
#ifdef SAM_DEBUG
  //debug2(" offset of DLMAP = %d \n", offset);
#endif
    // Commented by Barun : 21-Sep-2011
  //debug10 ("DL/UL.start transfer packets into burst => symbol offset of DLMAP_start at :%d, subchannel offset at :%d\n", offset, subchannel_offset_wimaxhdr);

//  debug2 ("the sizeof of DL-MAP is [%d]\n", ch->size());
  txtime = phy->getTrxTime (ch->size(), map->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
  txtime2 = phy->getTrxSymbolTime (ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
  ch->txtime() = txtime2;
  //ch->txtime() = txtime;
  txtime_s = (int)round (txtime2/phy->getSymbolTime ()); //in units of symbol
  ulduration = 0;

  Ofdm_mod_rate dlul_map_mod = map->getDlSubframe()->getProfile (b->getIUC())->getEncoding();
  //	debug2("-=-=-=-=-=-=- mod of this burst is [%d]\n", dlul_map_mod);
  num_of_slots = (int) ceil(ch->size()/phy->getSlotCapacity(dlul_map_mod,DL_)); //get the slots	needs.

  num_of_subchannel = num_of_slots;
  num_of_symbol = (int) ceil((double)(subchannel_offset_wimaxhdr + num_of_subchannel)/total_dl_subchannel_pusc)*2;
  
#ifdef DEBUG_WIMAX
  assert (b_data+ch->size() <= max_data);
#endif
  wimaxHdr = HDR_MAC802_16(p);
  if(wimaxHdr)
    {
      //wimaxHdr->phy_info.num_subchannels = b->getnumSubchannels();
      //wimaxHdr->phy_info.subchannel_offset = b->getSubchannelOffset ();
      //wimaxHdr->phy_info.num_OFDMSymbol = txtime_s;
//      wimaxHdr->phy_info.num_subchannels = num_of_subchannel;
      wimaxHdr->phy_info.num_subchannels = b->getnumSubchannels ();
      wimaxHdr->phy_info.subchannel_offset = b->getSubchannelOffset ();
//      wimaxHdr->phy_info.num_OFDMSymbol = num_of_symbol;
//      wimaxHdr->phy_info.OFDMSymbol_offset = offset;
      wimaxHdr->phy_info.num_OFDMSymbol = b->getDuration(); 			
      wimaxHdr->phy_info.OFDMSymbol_offset = b->getStarttime();
      wimaxHdr->phy_info.channel_index = 1; //broadcast packet
      wimaxHdr->phy_info.direction = 0;
    }
  ch->timestamp() = NOW; //add timestamp since it bypasses the queue
  b->enqueue(p);      //enqueue into burst
  // Commented by Barun : 21-Sep-2011
  //debug2("The length of the queue of burst is [%d]\n",b->getQueueLength_packets());
  //  b->setDuration(b->getDuration()-txtime_s);
//  b_data += ch->size();
  b_data += num_of_slots*phy->getSlotCapacity(dlul_map_mod,DL_);

 
  //debug2("old wimaxhdr subchannel offset is [%d]\n", subchannel_offset_wimaxhdr);
  subchannel_offset_wimaxhdr = (subchannel_offset_wimaxhdr + num_of_subchannel)%(total_dl_subchannel_pusc);

//  debug2("DL WimaxHdr:DL-MAP --- Mod[%d]\t size[%d]\t symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n", dlul_map_mod, ch->size(), offset, num_of_symbol, b->getSubchannelOffset(), num_of_subchannel);
  //debug2("DL/UL.wimaxHdr:DL-MAP --- Mod[%d]\t size[%d]\t symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n", dlul_map_mod, ch->size(), wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_subchannels);

  //debug2("new wimaxhdr subchannel offset is [%d]\n", subchannel_offset_wimaxhdr);
  offset += num_of_symbol-2;

#ifdef SAM_DEBUG
  //*debug2(" transferring management messages into the burst"); 
#endif

  b = map->getDlSubframe()->getPdu ()->getBurst (i_burst);
  i_burst++;
  b_data  = 0;
  txtime2 = 0;
  offset = b->getStarttime( );
  subchannel_offset_wimaxhdr = 0;
  max_data = phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding())-b_data;

  p = map->getUL_MAP();
  num_burst_before_data++;
  ch = HDR_CMN(p);
  txtime = phy->getTrxTime (ch->size(), map->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
  txtime2 = phy->getTrxSymbolTime (ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
  //ch->txtime() = txtime2;
  txtime_s = (int)round (txtime2/phy->getSymbolTime ()); //in units of symbol
  //ch->txtime() = txtime;
  ch->txtime() = txtime2;
#ifdef DEBUG_WIMAX
  assert (b_data+ch->size() <= max_data);
#endif

  dlul_map_mod = map->getDlSubframe()->getProfile (b->getIUC())->getEncoding();
  num_of_slots = (int) ceil(ch->size()/phy->getSlotCapacity(dlul_map_mod,DL_)); //get the slots	needs.
  num_of_subchannel = num_of_slots;
  num_of_symbol = (int) ceil((double)(subchannel_offset_wimaxhdr + num_of_subchannel)/total_dl_subchannel_pusc)*2;

  wimaxHdr = HDR_MAC802_16(p);
  if(wimaxHdr)
    {
/*
      wimaxHdr->phy_info.num_subchannels = num_of_subchannel;
      wimaxHdr->phy_info.subchannel_offset = subchannel_offset_wimaxhdr;
      wimaxHdr->phy_info.num_OFDMSymbol = num_of_symbol;
      wimaxHdr->phy_info.OFDMSymbol_offset = offset;
*/

      wimaxHdr->phy_info.subchannel_offset = b->getSubchannelOffset();
      wimaxHdr->phy_info.OFDMSymbol_offset = b->getStarttime();
      wimaxHdr->phy_info.num_subchannels = b->getnumSubchannels();
      wimaxHdr->phy_info.num_OFDMSymbol = b->getDuration();

//     debug2("DL WimaxHdr:UL-MAP --- Mod[%d]\t size[%d]\t symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n",
//	     dlul_map_mod, ch->size(), offset, num_of_symbol, subchannel_offset_wimaxhdr, num_of_subchannel);

    // Commented by Barun : 21-Sep-2011
  //debug10 ("DL/UL.wimaxHdr:UL-MAP --- Mod[%d]\t size[%d]\t symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n", dlul_map_mod, ch->size(), wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_subchannels);

      // if(c->getPeerNode())
      // wimaxHdr->phy_info.channel_index = c->getPeerNode()->getchannel();
      wimaxHdr->phy_info.channel_index = 1; //broadcast packet
      if (mac_->getNodeType()==STA_BS)
	wimaxHdr->phy_info.direction = 0;
      else 
	wimaxHdr->phy_info.direction = 1;
    } 
  ch->timestamp() = NOW; //add timestamp since it bypasses the queue
  b->enqueue(p);      //enqueue into burst
  // Commented by Barun : 21-Sep-2011
  //debug2("The length of the queue of burst is [%d]\n",b->getQueueLength_packets());
  //  b->setDuration(b->getDuration()-txtime_s); 
//  b_data += ch->size();
  b_data += num_of_slots*phy->getSlotCapacity(dlul_map_mod,DL_);
  //offset += txtime_s;
  offset += num_of_symbol-2;
  subchannel_offset_wimaxhdr = (subchannel_offset_wimaxhdr + num_of_subchannel)%(total_dl_subchannel_pusc);

  if (getMac()->sendDCD || map->getDlSubframe()->getCCC()!= getMac()->dlccc_) {
    b = map->getDlSubframe()->getPdu ()->getBurst (i_burst);
    i_burst++;
    b_data  = 0;
    txtime2 = 0;
    offset = b->getStarttime( );
    subchannel_offset_wimaxhdr = 0;
    max_data = phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding())-b_data;

    p = map->getDCD();
    num_burst_before_data++;
    ch = HDR_CMN(p);
    txtime = phy->getTrxTime (ch->size(), map->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
    txtime2 = phy->getTrxSymbolTime (ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
    //ch->txtime() = txtime;
    ch->txtime() = txtime2;
    txtime_s = (int)round (txtime2/phy->getSymbolTime ()); //in units of symbol
#ifdef DEBUG_WIMAX
    assert (b_data+ch->size() <= max_data);
#endif
    dlul_map_mod = map->getDlSubframe()->getProfile (b->getIUC())->getEncoding();
    num_of_slots = (int) ceil(ch->size()/phy->getSlotCapacity(dlul_map_mod,DL_)); //get the slots	needs.
//    num_of_subchannel = num_of_slots*2;
    num_of_subchannel = num_of_slots;
    num_of_symbol = (int) ceil((double)(subchannel_offset_wimaxhdr + num_of_subchannel)/total_DL_num_subchannel)*2;
    wimaxHdr = HDR_MAC802_16(p);
    if(wimaxHdr)
      {
/*
	//wimaxHdr->phy_info.num_subchannels = b->getnumSubchannels();
	//wimaxHdr->phy_info.subchannel_offset = b->getSubchannelOffset ();
	wimaxHdr->phy_info.num_subchannels = num_of_subchannel;
	wimaxHdr->phy_info.subchannel_offset = subchannel_offset_wimaxhdr;
	wimaxHdr->phy_info.num_OFDMSymbol = num_of_symbol;
	wimaxHdr->phy_info.OFDMSymbol_offset = offset;
	wimaxHdr->phy_info.channel_index = 1; //broadcast packet
*/

        wimaxHdr->phy_info.subchannel_offset = b->getSubchannelOffset();
        wimaxHdr->phy_info.OFDMSymbol_offset = b->getStarttime();
        wimaxHdr->phy_info.num_subchannels = b->getnumSubchannels();
        wimaxHdr->phy_info.num_OFDMSymbol = b->getDuration();
	wimaxHdr->phy_info.channel_index = 1; 

//	printf("DL WimaxHdr:DCD --- DCD size[%d]\t DCD mod[%d]\t symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n", ch->size(),dlul_map_mod,offset,num_of_symbol,subchannel_offset_wimaxhdr,num_of_subchannel);
        // Commented by Barun : 21-Sep-2011
        //debug10 ("DL/UL.wimaxHdr:DCD --- Mod[%d]\t size[%d]\t symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n", dlul_map_mod, ch->size(), wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_subchannels);

	// if(c->getPeerNode())
	// wimaxHdr->phy_info.channel_index = c->getPeerNode()->getchannel();
	if (mac_->getNodeType()==STA_BS)
	  wimaxHdr->phy_info.direction = 0;
	else 
	  wimaxHdr->phy_info.direction = 1;
      } 
    ch->timestamp() = NOW; //add timestamp since it bypasses the queue
    b->enqueue(p);      //enqueue into burst
    printf("The length of the queue of burst is [%d]\n",b->getQueueLength_packets());
    //  b->setDuration(b->getDuration()-txtime_s);
//    b_data += ch->size();
    b_data += num_of_slots*phy->getSlotCapacity(dlul_map_mod,DL_);

    //offset += txtime_s;
    offset += num_of_symbol-2;
    subchannel_offset_wimaxhdr = (subchannel_offset_wimaxhdr + num_of_subchannel)%(total_DL_num_subchannel);    
  }

  
  if (getMac()->sendUCD || map->getUlSubframe()->getCCC()!= getMac()->ulccc_) {
    b = map->getDlSubframe()->getPdu ()->getBurst (i_burst);
    i_burst++;
    b_data  = 0;
    txtime2 = 0;
    offset = b->getStarttime( );
    subchannel_offset_wimaxhdr = 0;
    max_data = phy->getMaxPktSize (b->getDuration(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding())-b_data;

    p = map->getUCD();
    num_burst_before_data++;
    ch = HDR_CMN(p);
    txtime = phy->getTrxTime (ch->size(), map->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
    txtime2 = phy->getTrxSymbolTime (ch->size(), mac_->getMap()->getDlSubframe()->getProfile (b->getIUC())->getEncoding());
    ch->txtime() = txtime2;
    txtime_s = (int)round (txtime2/phy->getSymbolTime ()); //in units of symbol
    //ch->txtime() = txtime;
    ch->txtime() = txtime2;
#ifdef DEBUG_WIMAX
    assert (b_data+ch->size() <= max_data);
#endif 

    dlul_map_mod = map->getDlSubframe()->getProfile (b->getIUC())->getEncoding();
    num_of_slots = (int) ceil(ch->size()/phy->getSlotCapacity(dlul_map_mod,DL_)); //get the slots	needs.
    num_of_subchannel = num_of_slots;
    num_of_symbol = (int) ceil((double)(subchannel_offset_wimaxhdr + num_of_subchannel)/total_DL_num_subchannel)*2;

    wimaxHdr = HDR_MAC802_16(p);
    if(wimaxHdr)
      {
/*
	//wimaxHdr->phy_info.num_subchannels = b->getnumSubchannels();
	//wimaxHdr->phy_info.subchannel_offset = b->getSubchannelOffset ();
	wimaxHdr->phy_info.num_subchannels = num_of_subchannel;
	wimaxHdr->phy_info.subchannel_offset = subchannel_offset_wimaxhdr;
	wimaxHdr->phy_info.num_OFDMSymbol = num_of_symbol;
	wimaxHdr->phy_info.OFDMSymbol_offset = offset;
	wimaxHdr->phy_info.channel_index = 1; //broadcast packet
*/

        wimaxHdr->phy_info.subchannel_offset = b->getSubchannelOffset();
        wimaxHdr->phy_info.OFDMSymbol_offset = b->getStarttime();
        wimaxHdr->phy_info.num_subchannels = b->getnumSubchannels();
        wimaxHdr->phy_info.num_OFDMSymbol = b->getDuration();
	wimaxHdr->phy_info.channel_index = 1; 

//	debug2("DL WimaxHdr:UCD --- UCD size[%d]\t symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n", ch->size(),offset,num_of_symbol,subchannel_offset_wimaxhdr,num_of_subchannel);
        //debug10 ("DL/UL.wimaxHdr:UCD --- Mod[%d]\t size[%d]\t symbol offset[%d]\t symbol num[%d]\t subchannel offset[%d]\t subchannel num[%d]\n", dlul_map_mod, ch->size(), wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_OFDMSymbol, wimaxHdr->phy_info.OFDMSymbol_offset, wimaxHdr->phy_info.num_subchannels);


	//if(c->getPeerNode())
	//wimaxHdr->phy_info.channel_index = c->getPeerNode()->getchannel();
	if (mac_->getNodeType()==STA_BS)
	  wimaxHdr->phy_info.direction = 0;
	else 
	  wimaxHdr->phy_info.direction = 1;
      } 
    ch->timestamp() = NOW; //add timestamp since it bypasses the queue
    b->enqueue(p);      //enqueue into burst
    // Commented by Barun : 21-Sep-2011
    //debug2("The length of the queue of burst is [%d]\n",b->getQueueLength_packets());
    // b->setDuration(b->getDuration()-txtime_s);
//    b_data += ch->size();
    b_data += num_of_slots*phy->getSlotCapacity(dlul_map_mod,DL_);

    //offset += txtime_s;
    offset += num_of_symbol-2;
    subchannel_offset_wimaxhdr = (subchannel_offset_wimaxhdr + num_of_subchannel)%(total_DL_num_subchannel);    
  }

  //b->setStarttime(offset);
  //Get other broadcast messages
  //Connection *c=mac_->getCManager ()->get_connection (b->getCid(),OUT_CONNECTION);
  //b_data += transfer_packets (c, b, b_data,subchannel_offset_wimaxhdr, offset);
  //b_data += transfer_packets1 (c, b, b_data);

  //Now get the other bursts
  // Commented by Barun : 21-Sep-2011
  //debug2("BS scheduler is going to handle other bursts (not DL/UL_MAP, DCD/UCD), #bursts = %d\n", map->getDlSubframe()->getPdu ()->getNbBurst());
  for (int index = num_burst_before_data ; index < map->getDlSubframe()->getPdu ()->getNbBurst(); index++) {
      Burst *b = map->getDlSubframe()->getPdu ()->getBurst (index);
      int b_data = 0;

      Connection *c=mac_->getCManager ()->get_connection (b->getCid(),OUT_CONNECTION);
      // Commented by Barun : 21-Sep-2011
      //debug2("DL/UL.other CID [%d]\n", b->getCid());
#ifdef DEBUG_WIMAX
      assert (c);
#endif    
      //Begin RPI
      if (c!=NULL) {
      // Commented by Barun : 21-Sep-2011
	  //debug10 ("DL/UL.check CID :%d, flag: FRAG :%d, PACK :%d, ARQ: %p\n", b->getCid(), c->isFragEnable(), c->isPackingEnable(), c->getArqStatus ()); 				
	  //debug10 ("DL/UL.before transfer_packets1 (other data) to burst_i :%d, CID :%d, b_data (bytes_counter) :%d\n", index, b->getCid(), b_data);
	  if (c->isFragEnable() && c->isPackingEnable() &&  (c->getArqStatus () != NULL) && (c->getArqStatus ()->isArqEnabled() == 1)) {
	     //debug2("DL/UL.BSSscheduler is goting to transfer packet with fragackarq.\n");
	     b_data = transfer_packets_with_fragpackarq (c, b, b_data); /*RPI*/
	  } else {	
	     b_data = transfer_packets1(c, b, b_data);
	  }
       }
       // Commented by Barun : 21-Sep-2011
       //debug10 ("\nDL/UL.after transfer_packets1 (other data) to burst_i :%d, CID :%d, b_data (update_counter) :%d\n", index, b->getCid(), b_data);
       //debug10 ("Dl/UL.the length of the queue of burst is [%d]\n", b->getQueueLength_packets());
  }//end loop ===> transfer bursts

  //Print the map
  //*debug2 ("\n==================BS %d Subframe============================\n", mac_->addr());
  //*mac_->getMap()->print_frame();
  //*debug2 ("===========================================================\n");

}

/**   
 * Add a downlink burst with the given information
 * @param burstid The burst number
 * @param c The connection to add
 * @param iuc The profile to use
 * @param dlduration current allocation status
 * @param the new allocation status
 */
/*       // rpi removed this dl burst ----------------------------------------------------------------------------
	 int BSScheduler::addDlBurst (int burstid, Connection *c, int iuc, int dlduration, int maxdlduration)
	 {
	 double txtime; //tx time for some data (in second)
	 int txtime_s;  //number of symbols used to transmit the data
	 OFDMPhy *phy = mac_->getPhy();

	 //add a burst for this node
	 DlBurst *db = (DlBurst*) mac_->getMap()->getDlSubframe()->getPdu ()->addBurst (burstid);
	 db->setCid (c->get_cid());
	 db->setIUC (iuc);
	 db->setStarttime (dlduration);

	 txtime = phy->getTrxSymbolTime (c->queueByteLength(), mac_->getMap()->getDlSubframe()->getProfile (db->getIUC())->getEncoding());
	 txtime += c->queueLength() * TX_GAP; //add small gaps between packets to send
	 txtime_s = (int) ceil(txtime/phy->getSymbolTime ()); //in units of symbol
	 if (txtime_s < maxdlduration-dlduration) {
	 db->setDuration (txtime_s);
	 dlduration += db->getDuration ()+1; //add 1 OFDM symbol between bursts
	 } else {
	 //fill up the rest
	 db->setDuration (maxdlduration-dlduration);
	 dlduration = maxdlduration;
	 }
	 return dlduration;
	 }

*/ // rpi removed this dl burst ----------------------------------------------------------------------------

// rpi added adddlburst for ofdma ------------------------------------------------------------------

/**
 * Add a downlink burst with the given information
 * @param burstid The burst number
 * @param c The connection to add
 * @param iuc The profile to use
 * @param dlduration current allocation status
 * @param the new allocation status
 */
void BSScheduler::addDlBurst (int burstid, int cid, int iuc, int ofdmsymboloffset, int numofdmsymbols, int subchanneloffset, int numsubchannels)
{
  DlBurst *db = (DlBurst*) mac_->getMap()->getDlSubframe()->getPdu ()->addBurst (burstid);
  db->setCid (cid);
  db->setIUC (iuc);
  db->setStarttime (ofdmsymboloffset);
  db->setDuration (numofdmsymbols);
  db->setSubchannelOffset (subchanneloffset);
  db->setnumSubchannels (numsubchannels);
  
  //*debug2("Data Burst:----symbol_offset[%d]\t symbol_num[%d]\t subchannel_offset[%d]\t subchannel_num[%d]\n",
	 //*ofdmsymboloffset, numofdmsymbols,subchanneloffset,numsubchannels);

}

int BSScheduler::doesMapExist(int cid, int *cid_list, int num_of_entries){
  for(int i = 0; i<num_of_entries; ++i){
    if(cid_list[i]==cid)
      return i;
  }
  return -1;
}
		 
//dl_stage2 (Virtual Allocation) for data allocation (all CIDs)
mac802_16_dl_map_frame * BSScheduler::dl_stage2(Connection *head, int input_subchannel_offset, int total_subchannels, int total_symbols, int symbol_start, int stripping, int total_dl_slots_pusc){
  struct mac802_16_dl_map_frame *dl_map;
  struct mac802_16_dlmap_ie *ies;
  Connection *con;
  int i, ie_index, temp_index;
  int num_of_symbols, num_of_subchannels, num_of_slots;
  double allocationsize;
  int freeslots;
  int symbol_offset = 0;
  int subchannel_offset = input_subchannel_offset;
  int subchannel_start = 0;
  int num_of_data_connections = 0;
  int total_dl_free_slots = 0;
  ConnectionType_t contype;
  SchedulingType_t schedtype;
  Mac802_16 *  mac_ = getMac ();
  Ofdm_mod_rate mod_rate;

  int slots_per_con[MAX_MAP_IE];
  int cid_list[MAX_MAP_IE];
  int diuc_list[MAX_MAP_IE];


  int leftOTHER_slots=0;
  int needmore_con[3]={0,0,0};
  int needmore_c=0;
  int ori_symbols = total_symbols;
  int req_slots_tmp1 = 0;
  int return_cid_tmp = 0;

  dl_map = (struct mac802_16_dl_map_frame *) malloc(sizeof(struct mac802_16_dl_map_frame));
  dl_map->type = MAC_DL_MAP;
  dl_map->bsid = mac_->addr(); // its called in the mac_ object

  ies = dl_map->ies;
  ie_index = 0; //0 is for the ul-map
  int temp_index_burst = index_burst;

  if ((total_symbols%2)==1) total_symbols=total_symbols-1;
  freeslots = total_subchannels * floor(total_symbols/2);
  total_dl_free_slots = freeslots;

  //debug10 ("DL2,****FRAME NUMBER**** :%d, Total_DL_Slots :%d, FreeSlots(TotalSub*TotalSym/2) :%d, TotalSub :%d, Ori_TotalSymbol :%d, TotalSymbol :%d, StartSymbol :%d\n", frame_number, total_dl_slots_pusc, total_dl_free_slots, total_subchannels, ori_symbols, total_symbols, symbol_start );

  frame_number++;

//1. (1st priority) Allocate management message (basic, primary, secondary) 
  con=head;
  while(con!=NULL){
    if(con->get_category()==CONN_DATA) ++num_of_data_connections;
    con = con->next_entry();
  }

  for(i=0;i<3;++i){
    con = head;
    if(i==0) 		contype = CONN_BASIC;
    else if(i==1) 	contype = CONN_PRIMARY;
    else 		contype = CONN_SECONDARY;
    while(con!=NULL) {
	if(con->get_category() == contype) {
	    allocationsize = con->queueByteLength();

  	    int con_byte = con->queueByteLength();
            int rep = 1;
	    if ( con_byte > 0 ) {
               Ofdm_mod_rate con_mod = mac_->getMap()->getDlSubframe()->getProfile (con->getPeerNode()->getDIUC())->getEncoding();
               int con_slots = (int) rep*ceil((double)con_byte/(double)mac_->getPhy()->getSlotCapacity(con_mod, DL_)); 
	       int virtual_alloc_exist = doesvirtual_allocexist(index_burst, con->get_cid());
	       int add_slots = 0;
	       if (virtual_alloc_exist > 0) {
		  int add_slots = overallocation_withoutdlmap (index_burst, total_dl_slots_pusc, con_slots);
		  int t_index = addslots_withoutdlmap(index_burst, con_byte, con_slots, con->get_cid());
	    	  //debug10 ("DL Add more slots into existing burst contype(%d), index :%d, cid :%d, byte :%d, slots :%f\n", contype, t_index, con->get_cid(), virtual_alloc[t_index].byte, virtual_alloc[t_index].numslots);
	       } else {
		  int add_slots = overallocation_withdlmap (index_burst, total_dl_slots_pusc, con_slots);
		  if (add_slots > 0) {
            	     virtual_alloc[index_burst].alloc_type = 5;
  	    	     virtual_alloc[index_burst].cid = con->get_cid();
  	    	     virtual_alloc[index_burst].n_cid = 1;
  	    	     //virtual_alloc[index_burst].iuc = getDIUCProfile(con_mod);
  	    	     virtual_alloc[index_burst].iuc = getDIUCProfile(OFDM_QPSK_1_2);
  	    	     virtual_alloc[index_burst].numslots = add_slots;
  	    	     virtual_alloc[index_burst].byte = (add_slots * mac_->getPhy()->getSlotCapacity(con_mod, DL_));
  	    	     virtual_alloc[index_burst].rep =  rep;
  	    	     virtual_alloc[index_burst].dl_ul = 0;
  	    	     virtual_alloc[index_burst].ie_type = 0;
  	    	     // Commented by Barun : 21-Sep-2011
	    	     //debug10 ("DL Add new burst contype(%d), index :%d, cid :%d, byte :%f, slots :%f\n", contype, index_burst, con->get_cid(), virtual_alloc[index_burst].byte, virtual_alloc[index_burst].numslots);
  	    	     index_burst++;
		  }
	      }
	    }//end if q > 0

        // Commented by Barun : 21-Sep-2011
	    //debug10 ("DL2.management msg, contype(%d), index :%d, CID :%d, Q-Bytes :%d, allocationsize :%f\n", contype, index_burst, con->get_cid(), con->queueByteLength(), con_byte);
	}
	con = con->next_entry();
    }//end con!=null
  }//end for loop

//2. Calculate #of active connections (ugs, ertps, rtps, nrtps, be) 
  int conn_per_schetype[6]={0,0,0,0,0,0};
  int con_ugs_all  =0; int con_ertps_all=0; int con_rtps_all =0;  int con_nrtps_all=0; int con_be_all   =0;

  for (i=0;i<5;++i) {
    con = head;
    if(i==0)            schedtype = SERVICE_UGS;
    else if(i==1)       schedtype = SERVICE_ertPS;
    else if(i==2)       schedtype = SERVICE_rtPS;    
    else if(i==3)       schedtype = SERVICE_nrtPS; 
    else                schedtype = SERVICE_BE;

    while(con!=NULL){
      if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype){
	if(schedtype==SERVICE_UGS) {
	  if (con->queueByteLength() > 0) conn_per_schetype[0]++;
	  con_ugs_all++;
	} else if (schedtype==SERVICE_ertPS) {
	  if (con->queueByteLength() > 0) conn_per_schetype[1]++;
	  con_ertps_all++;
	} else if (schedtype==SERVICE_rtPS) {
	  if (con->queueByteLength() > 0) conn_per_schetype[2]++;
	  con_rtps_all++;
	} else if (schedtype==SERVICE_nrtPS) {
	  if (con->queueByteLength() > 0) conn_per_schetype[3]++;
	  con_nrtps_all++;
	} else if (schedtype==SERVICE_BE) {
	  if (con->queueByteLength() > 0) conn_per_schetype[4]++;
	  con_be_all++;
	} else {
	  conn_per_schetype[5]++;
	}
      }
      con = con->next_entry();
    }
  }

  int active_conn_except_ugs = 0;
  for (int i=1; i<5; i++) active_conn_except_ugs =+ conn_per_schetype[i];
  
  //debug10 ("DL2, Active UGS <%d>, ertPS <%d>, rtPS <%d>, nrtPS <%d>, BE <%d>\n", conn_per_schetype[0],conn_per_schetype[1], conn_per_schetype[2], conn_per_schetype[3], conn_per_schetype[4]);
  //debug10 ("\tAll UGS <%d>, ertPS <%d>, rtPS <%d>, nrtPS <%d>, BE <%d>\n", con_ugs_all, con_ertps_all, con_rtps_all, con_nrtps_all, con_be_all);

//3. (2nd priority) Allocate UGS 
  int next_ie_index = ie_index;
  //***************
  leftOTHER_slots = freeslots;
  //UGS Scheduling
  for(i=0;i<5;++i){
    con = head;
    if(i==0)            schedtype = SERVICE_UGS;
    
    while (con!=NULL) {
      if (con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype){
        req_slots_tmp1 = 0;
        int grant_slots = 0;
        num_of_slots = 0; 
        mod_rate = mac_->getMap()->getDlSubframe()->getProfile(con->getPeerNode()->getDIUC())->getEncoding();

        if (schedtype==SERVICE_UGS) {
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
	  if (con->queueByteLength() > 0) {
#ifdef UGS_AVG
          allocationsize = (int) ceil((double)sfqos->getDataSize()/(double)sfqos->getPeriod());
#endif
#ifndef UGS_AVG
          int tmp_getpoll = con->getPOLL_interval();
          if ( (tmp_getpoll%sfqos->getPeriod())== 0 ) {
                allocationsize = ceil(sfqos->getDataSize());
                con->setPOLL_interval(0);
          } else {
                allocationsize = 0;
          }
          tmp_getpoll++;
          con->setPOLL_interval(tmp_getpoll);
#endif
	  } else {
	     allocationsize = 0;
	  }

//ARQ aware scheduling
	  int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
	  int t_bytes = 0;
	  double ori_grant = allocationsize;
	  if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
		t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
		if (t_bytes > 0) {
			allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
		}
		// Commented by Barun : 21-Sep-2011
		//debug10 ("\tARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
		allocationsize = allocationsize + 14;	//optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub
		
	  } 
//End ARQ aware scheduling

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));;
	  if (con->queueByteLength() <=0) { 
		allocationsize = 0;
		num_of_slots = 0;
	  } else {
		num_of_slots = grant_slots;
	  }

          Ofdm_mod_rate con_mod = mac_->getMap()->getDlSubframe()->getProfile (con->getPeerNode()->getDIUC())->getEncoding();
          int con_slots = num_of_slots; 
          int con_byte = allocationsize; 
	  int virtual_alloc_exist = doesvirtual_allocexist(index_burst, con->get_cid());
	  int add_slots = 0;
	  if (virtual_alloc_exist > 0) {
		int add_slots = overallocation_withoutdlmap (index_burst, total_dl_slots_pusc, con_slots);
		addslots_withoutdlmap(index_burst, con_byte, con_slots, con->get_cid());
	  } else {
		int add_slots = overallocation_withdlmap (index_burst, total_dl_slots_pusc, con_slots);
		if (add_slots > 0) {
            	   virtual_alloc[index_burst].alloc_type = 5;
  	    	   virtual_alloc[index_burst].cid = con->get_cid();
  	    	   virtual_alloc[index_burst].n_cid = 1;
  	    	   virtual_alloc[index_burst].iuc = getDIUCProfile(con_mod);
  	    	   virtual_alloc[index_burst].numslots = add_slots;
  	    	   virtual_alloc[index_burst].byte = (add_slots * mac_->getPhy()->getSlotCapacity(con_mod, DL_));
  	    	   virtual_alloc[index_burst].rep =  1;
  	    	   virtual_alloc[index_burst].dl_ul = 0;
  	    	   virtual_alloc[index_burst].ie_type = 0;
  	    	   index_burst++;
	   	}
	  }

        // Commented by Barun : 21-Sep-2011
	  //debug10 ("DL2.UGS, DataSize :%f, period :%d, numslots :%d, CID :%d, DIUC :%d\n", sfqos->getDataSize(), sfqos->getPeriod(), con_slots, con->get_cid(), con->getPeerNode()->getDIUC());	  
	  //debug10 ("\tQ-bytes :%d, grant-bytes :%ld\n", con->queueByteLength(), (long int)( con_slots*mac_->getPhy()->getSlotCapacity(mod_rate, DL_) ));

        }//end UGS
      }
      con = con->next_entry();
    }//end con != NULL
  }//end UGS


//4. (3rd priority) Allocate to Others 
//In this version, we validate the result only on best effort, we do not test rtPS, ertPS, and nrtPS yet.
//Fair allocation
//Find out how many connections can be supported given variable DL MAP
  int max_conn_dlmap = max_conn_withdlmap(index_burst, total_dl_slots_pusc);
  int active_shared_conn = active_conn_except_ugs;
  if (active_shared_conn > max_conn_dlmap) active_shared_conn = max_conn_dlmap;

  double weighted_isp = 1;
  int max_conn = MAX_CONN;
  int fair_share[max_conn];
  int dl_all_con=0;

  con_drr dl_con_data[MAX_CONN];
  con_data_alloc con_data[max_conn];
  con_data_alloc con_data_final[max_conn];

  bzero(fair_share, max_conn*sizeof(int));
  bzero(con_data, max_conn*sizeof(con_data_alloc));
  bzero(con_data_final, max_conn*sizeof(con_data_alloc));
  bzero(dl_con_data, max_conn*sizeof(con_drr));

  for (int i=0; i<MAX_CONN; i++) {
     dl_con_data[i].cid = -2;
     dl_con_data[i].quantum = 0;
     dl_con_data[i].counter = 0;
  }

//Find out #request_slots for each connection
 int j_con = 0;
 for (i=0; i<5; ++i){
     con = head;
     if(i==0)        schedtype = SERVICE_UGS;
     else if(i==1)   schedtype = SERVICE_ertPS;
     else if(i==2)   schedtype = SERVICE_rtPS;
     else if(i==3)   schedtype = SERVICE_nrtPS;
     else            schedtype = SERVICE_BE;
     if ( i==0 ) continue;      

     while(con != NULL) {

        if (con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype) {
	   Ofdm_mod_rate mod_rate = mac_->getMap()->getDlSubframe()->getProfile(con->getPeerNode()->getDIUC())->getEncoding();

           int withfrag = 0;
/*           if  (con->getFragmentBytes()>0) {
              withfrag = con->queueByteLength() - con->getFragmentBytes() + 4;
           } else {
              withfrag = con->queueByteLength();;
           }*/

		int length = 0;
		if (con->getArqStatus () != NULL && con->getArqStatus ()->isArqEnabled() == 1)
		{
			if(con->getArqStatus()->arq_retrans_queue_->byteLength()>0 ||con->getArqStatus()->arq_trans_queue_->byteLength() >0 )
			{
				//*debug2("ARQ retrans queue length is %d\n\n",con->getArqStatus()->arq_retrans_queue_->byteLength() );
				if(con->getArqStatus()->arq_retrans_queue_->byteLength()>0) 
					length =con->getArqStatus()->arq_retrans_queue_->byteLength();
				else
					length = con->getArqStatus()->arq_trans_queue_->byteLength();
			}
			else
			{
				length = con->queueByteLength();
			}
		}
		else
		{
			length = con->queueByteLength();
		}	

           if  (con->getFragmentBytes()>0) 
	    {
              withfrag = length - con->getFragmentBytes() + 4;
		
           } 
           else 
           {
              withfrag = length+4;
           }
	    //*debug2("ARQ retrans queue got allocation size %d\n\n", withfrag);
//ARQ aware scheduling
           int arq_enable_f = 0;
	   if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	   }
           int t_bytes = 0;
           double ori_grant = withfrag;
           if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)withfrag % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        withfrag = withfrag + (getMac()->arq_block_size_ - t_bytes);
                }
                //debug10 ("\t ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, withfrag, withfrag + (double)14);
                withfrag = withfrag + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

           }
//End ARQ

           int req_slots = ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_));
           if (req_slots > 0) {
                // Commented by Barun : 21-Sep-2011
              //debug10 ("Con-Request byte :%d, slotcapa :%d, reqslot :%d\n", con->queueByteLength(), mac_->getPhy()->getSlotCapacity(mod_rate, DL_), req_slots );
              Packet *np;
              if ( (np = con->queueLookup(0)) != NULL ) {
                 int p_size = hdr_cmn::access(np)->size();
                 //debug10 ("CON CID :%d, q->byte :%d, q->len :%d, packet_size :%d, frag_no :%d, frag_byte :%d, frag_stat :%d\n", con->get_cid(), con->queueByteLength(), con->queueLength(), p_size, con->getFragmentNumber(), con->getFragmentBytes(), (int)con->getFragmentationStatus() );
              }

              Ofdm_mod_rate t_mod_rate = mac_->getMap()->getDlSubframe()->getProfile(con->getPeerNode()->getDIUC())->getEncoding();
              con_data[j_con].cid = con->get_cid();
              con_data[j_con].direction = DL_;
              con_data[j_con].mod_rate = t_mod_rate;
              con_data[j_con].req_slots = req_slots;
              con_data[j_con].req_bytes = withfrag;
              con_data[j_con].grant_slots = 0;
              con_data[j_con].weight = 0;
              con_data[j_con].counter = 0;
              j_con++;
          } else {
/*
            for (int j=0; j<MAX_CONN; j++) {
              if (dl_con_data[j].cid == con->get_cid()) {
                 dl_con_data[j].counter = 0;
                 break;
              }
            }
*/
          }//end else
        }
        con = con->next_entry();
     }
 }

 //*if (j_con == active_conn_except_ugs) ;//debug10 ("DL2.other QoS flows OK :num_con :%d\n", j_con);
 //*else  debug10 ("Error Panic\n");

//Sorting and make sure #request slots is correct
 if (j_con>1)  {     
	for (int i=0; i<j_con; i++) { 
	    // Commented by Barun : 21-Sep-2011
		//debug10 ("DL2.other QoS flows, Req_slots Before i :%d, cid :%d, counter :%d, req_slots :%d, mod :%d\n", i, con_data[i].cid, con_data[i].counter, con_data[i].req_slots, (int)con_data[i].mod_rate); 		
	}
        bubble_sort(j_con, con_data, 0);
        for (int i=0; i<j_con; i++) { 
		//debug10 ("DL2.other QoS flows, Req_slots After i :%d, cid :%d, counter :%d, req_slots :%d, mod :%d\n", i, con_data[i].cid, con_data[i].counter, con_data[i].req_slots, (int)con_data[i].mod_rate); 
	}
 }

 int t_freeslots = 0;
 int j_need = j_con;
 if (j_need > max_conn_dlmap) j_need = max_conn_dlmap;
 if (j_need > 0 ) t_freeslots = freeslots_withdlmap_given_conn(index_burst, total_dl_slots_pusc, j_need);

//Algorithm is here
//Note that in this version, we do slot-fair allocation or different MCSs will get the same throughput
//Next version, we will include weighted fair allocation
 int j_count = j_need;
 for (int i=0; i<j_need; i++) {
        if ( (t_freeslots <=0) ) break;
        fair_share[i] = floor(t_freeslots/j_count);
        if (fair_share[i] >= con_data[i].req_slots) {
           con_data[i].grant_slots = con_data[i].req_slots;
        } else {
           con_data[i].grant_slots = fair_share[i];
        }
        // Commented by Barun : 21-Sep-2011
	//debug10 ("DL2.other QoS flows, Freeslots :%d, fairshare[%d] ;%d, grant_slots :%d\n", t_freeslots, i, fair_share[i], con_data[i].grant_slots);
        t_freeslots = t_freeslots - con_data[i].grant_slots;
	j_count--;
 }

 for (int i=0; i<j_need; i++) {
     //debug10 ("\tAfter fair share ->Con :%d, req_slots :%d, grant_slots :%d\n", con_data[i].cid, con_data[i].req_slots, con_data[i].grant_slots);
 }

//Assign granted slots to each connection (virtual burst)
 for (int i=0; i<j_need; i++) {
    if (con_data[i].grant_slots >=0 ) {
       Ofdm_mod_rate con_mod = con_data[i].mod_rate;
       int con_slots = con_data[i].grant_slots; 
       int con_byte = (con_slots * mac_->getPhy()->getSlotCapacity(con_mod, DL_));
       //debug10 ("OTHERS, i ;%d, numslots :%d, bytes :%d, CID :%d\n", i, con_slots, con_byte, con_data[i].cid);

       int virtual_alloc_exist = doesvirtual_allocexist(index_burst, con_data[i].cid);
       int add_slots = 0;
       if (virtual_alloc_exist > 0) {
	  int add_slots = overallocation_withoutdlmap (index_burst, total_dl_slots_pusc, con_slots);
	  addslots_withoutdlmap(index_burst, con_byte, con_slots, con_data[i].cid);
       } else {
	  int add_slots = overallocation_withdlmap (index_burst, total_dl_slots_pusc, con_slots);
	  if (add_slots > 0) {
       	     virtual_alloc[index_burst].alloc_type = 5;
      	     virtual_alloc[index_burst].cid = con_data[i].cid; 
	     virtual_alloc[index_burst].n_cid = 1;
  	     virtual_alloc[index_burst].iuc = getDIUCProfile(con_mod);
  	     virtual_alloc[index_burst].numslots = add_slots;
  	     virtual_alloc[index_burst].byte = (add_slots * mac_->getPhy()->getSlotCapacity(con_mod, DL_));
  	     virtual_alloc[index_burst].rep =  1;
  	     virtual_alloc[index_burst].dl_ul = 0;
  	     virtual_alloc[index_burst].ie_type = 0;
  	     index_burst++;
	   }
       }

     //debug10 ("DL2.other QoS flows, index :%d, grant_numslots :%d, grant_bytes :%d, CID :%d\n", i, con_slots, (long int)( con_slots*mac_->getPhy()->getSlotCapacity(con_data[i].mod_rate, DL_)), con_data[i].cid);
    }

 }

/*
// This comment is the previous allocation for all QoS classes but we do not consider the variable part of DL_MAP.
// Users can apply some portions of the code here for other QoSs.
// 4. (3rd priority) Allocate other flows (except UGS) fairly (BE only)
  for(i=0;i<5;++i){
    con = head;
    if(i==0)            schedtype = SERVICE_UGS;
    else if(i==1)       schedtype = SERVICE_ertPS;
    else if(i==2)       schedtype = SERVICE_rtPS;
    else if(i==3)       schedtype = SERVICE_nrtPS;
    else                schedtype = SERVICE_BE;
    
    while(con!=NULL){
      if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype){

        req_slots_tmp1 = 0;
        return_cid_tmp = 0;
        int grant_slots = 0;
        num_of_slots = 0; 

        return_cid_tmp = 0;
        temp_index = doesMapExist(con->get_cid(), cid_list, ie_index);
        if(temp_index < 0) return_cid_tmp = -1;
        else return_cid_tmp = con->get_cid();
        mod_rate = mac_->getMap()->getDlSubframe()->getProfile(con->getPeerNode()->getDIUC())->getEncoding();


        if(schedtype==SERVICE_UGS){
        } else {
//Fair allocation

	}//end else UGS

        if(schedtype == SERVICE_ertPS) {

	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
          if (con->queueByteLength() > 0) {
             allocationsize = (int) ceil((double)sfqos->getDataSize()/(double)sfqos->getPeriod());
          } else {
             allocationsize = 0;
          }

          int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          } 
	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));;

	  if (con->queueByteLength() <=0) num_of_slots = 0;
	  else num_of_slots = grant_slots;

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));
	  num_of_slots = grant_slots;

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("Fatal Error: There is not enough resource for ertPS\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;


	  debug10 ("DL.Check1.3.ertPS, DataSize :%f, period :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getDataSize(), sfqos->getPeriod(), grant_slots, con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
//	  debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofertPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[1]);
	  debug10 ("\tQ-bytes :%d, Grant-bytes :%ld\n", con->queueByteLength(), (long int)( grant_slots*mac_->getPhy()->getSlotCapacity(mod_rate, DL_) ));

        }
        
        
        if(schedtype == SERVICE_rtPS) {
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
          int withfrag = 0;
          if  (con->getFragmentBytes()>0) {
               withfrag = con->queueByteLength() - con->getFragmentBytes() + 2;
          } else {
               withfrag = con->queueByteLength();
          }
	  req_slots_tmp1 = (int) ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_));

	  if (req_slots_tmp1>0) allocationsize = (int) ceil((double)(sfqos->getMinReservedRate()*FRAME_SIZE)/8);
	  else  allocationsize = 0;

          int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          }

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));
	  num_of_slots = grant_slots;

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("Fatal Error: There is not enough resource for rtPS\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;

	  if (grant_slots<req_slots_tmp1) needmore_con[0]++;

	  debug10 ("DL.Check1.3.rtPS, MinReservedRate :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getMinReservedRate(), grant_slots, con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
//	  debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofrtPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[2]);
	  debug10 ("\tQ-bytes :%d, Grant-bytes :%ld\n", con->queueByteLength(), (long int)( grant_slots*mac_->getPhy()->getSlotCapacity(mod_rate, DL_) ));
        
        }


        if(schedtype == SERVICE_nrtPS) {
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
	  int withfrag = 0;
          if  (con->getFragmentBytes()>0) {
               withfrag = con->queueByteLength() - con->getFragmentBytes() + 2;
          } else {
               withfrag = con->queueByteLength();;
          }

	  req_slots_tmp1 = (int) ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_));

	  if (req_slots_tmp1>0) allocationsize = (int) ceil((sfqos->getMinReservedRate()*FRAME_SIZE)/8);
	  else  allocationsize = 0;

          int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          }

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));
	  num_of_slots = grant_slots;

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("Fatal Error: There is not enough resource for nrtPS\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;

	  if (grant_slots<req_slots_tmp1) needmore_con[1]++;

	  debug10 ("DL.Check1.3.nrtPS, MinReservedRate :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getMinReservedRate(), grant_slots, con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
//	  debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofnrtPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[3]);
	  debug10 ("\tQ-bytes :%d, Grant-bytes :%ld\n", con->queueByteLength(), (long int)( grant_slots*mac_->getPhy()->getSlotCapacity(mod_rate, DL_) ));

        }

        if(schedtype==SERVICE_BE){
	  int withfrag = 0;
          if  (con->getFragmentBytes()>0) {
               withfrag = con->queueByteLength() - con->getFragmentBytes() + 2;
          } else {
               withfrag = con->queueByteLength();;
          }

	  req_slots_tmp1 = (int) ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_));

	  if (req_slots_tmp1>0) needmore_con[2]++;

	  debug10 ("DL.Check1.3.BE, Peer-CID :%d, returnCID :%d, DIUC :%d\n", con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
	  debug10 ("\tQ-bytes :%d\n", con->queueByteLength());
	  con = con->next_entry();
	  continue;
        }

        if(num_of_slots> 0) {
	  temp_index = doesMapExist(con->get_cid(), cid_list, ie_index);

	  if(temp_index < 0){
	    temp_index = ie_index;
	    cid_list[temp_index] = con->get_cid();
	    slots_per_con[temp_index] = num_of_slots;
	    diuc_list[temp_index] = getDIUCProfile(mod_rate);
	    ++ie_index;

	    debug10 ("DL.Check1.3: MapNotExist New_CID :%d, return index :-1, cid_list[%d] :%d,  #entry (ie_index)++ :%d\n", con->get_cid(), temp_index, cid_list[temp_index], ie_index);
	    debug10 ("\tNumSlots1.2 :%d\n", num_of_slots);

	  } else if(temp_index<MAX_MAP_IE) {
	    slots_per_con[temp_index] += num_of_slots;

	    debug10 ("DL.Check1.3: MapExist Peer_CID :%d, return index :%d, cid_list[%d] :%d,  #entry (ie_index) :%d\n", con->get_cid(), temp_index, temp_index, cid_list[temp_index], ie_index);
	    debug10 ("\tNumSlots1.2 :%d\n", num_of_slots);

	  } else {
	    freeslots += num_of_slots;                      //return back the slots
	    leftOTHER_slots += num_of_slots;                //return back the slots
	  }

	  debug10 ("DL.Check1.3, First Assign (ugs/ertps/rtps/nrtps/no be): CID(%d), Schetype :%d, Numslots :%d, Freeleft :%d, StoreSlots[%d] :%d, mod_rate :%d, DIUC :%d\n", cid_list[temp_index], schedtype, num_of_slots, freeslots, temp_index, slots_per_con[temp_index], mod_rate, diuc_list[temp_index]);
        }

      }
      con = con->next_entry();
    }//end con != NULL

  }//end for loop 5 Qos
*/



/*
// This comment is to allocate all connections fairly
  //Assign left-over to rtPS, ertPS, and BE fairly
  int share_next_slots = 0;
  for (int i=0;i<3;i++) 
    needmore_c = needmore_c+needmore_con[i];
  if (needmore_c>0) 
    share_next_slots = (int) floor(leftOTHER_slots/needmore_c);

  int first_assign = 0;

  while (needmore_c>0 && freeslots>0) {

    share_next_slots = (int) floor(freeslots/needmore_c);

    debug10 ("DL.Check1.4, (Check still need more here): Needmore Conn :%d, Free :%d, Sharenext :%d\n", needmore_c, freeslots, share_next_slots);

    for(i=0;i<5;++i){
      con = head;
      if(i==0)        schedtype = SERVICE_UGS;
      else if(i==1)   schedtype = SERVICE_ertPS;
      else if(i==2)   schedtype = SERVICE_rtPS;
      else if(i==3)   schedtype = SERVICE_nrtPS;
      else            schedtype = SERVICE_BE;
    
      if ( (i==0) || (i==1) ) continue;

      first_assign = 0;
      while(con!=NULL) {
	debug2 ("Rich: Con %p cid=%d\n", con, con->get_cid());
	if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype) {

	  mod_rate = mac_->getMap()->getDlSubframe()->getProfile(con->getPeerNode()->getDIUC())->getEncoding();
	  temp_index = doesMapExist(con->get_cid(), cid_list, ie_index);
	  if(temp_index < 0) return_cid_tmp = -1;
	  else return_cid_tmp = temp_index;

	  int withfrag = 0;
          if  (con->getFragmentBytes()>0) {
               withfrag = con->queueByteLength() - con->getFragmentBytes() + 2;
          } else {
               withfrag = con->queueByteLength();;
          }
	  int req_slots = (int) ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_)); 

	  if ( (schedtype==SERVICE_rtPS) || (schedtype==SERVICE_nrtPS) ) {
	    if (return_cid_tmp == -1) {
	      debug10 ("DL.Check1.4.rtps/nrtps, No_CID(%d), n_Conn :%d, Free :%d\n", return_cid_tmp, needmore_c, freeslots);
	      con = con->next_entry();
	      continue;
	    }
	    if ( req_slots<=slots_per_con[return_cid_tmp] ) {
	      debug10 ("DL.Check1.4.rtPS/nrtPS, CID(%d), <=MinSlots: n_Conn :%d, REQ-SLOTS :%d, GRANT-1SLOTS :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, req_slots, slots_per_con[return_cid_tmp], freeslots);
	      con = con->next_entry();
	      continue;
	    }
	  }

	  int t_num_of_slots = 0;

	  if ( (schedtype==SERVICE_rtPS) || (schedtype==SERVICE_nrtPS) ) {
	    first_assign = slots_per_con[return_cid_tmp];

	    if (req_slots <= (first_assign + share_next_slots) ) {           
	      t_num_of_slots = req_slots;
	      slots_per_con[return_cid_tmp] = t_num_of_slots;
	      needmore_c--;
	      freeslots = freeslots - (req_slots-first_assign);
	      debug10 ("DL.Check1.4.rtps/nrtps, CID(%d), rtps/nrtpsNoNeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, t_num_of_slots, freeslots);
	    } else {
	      if (share_next_slots==0) {
		if (freeslots>0) { 
		  t_num_of_slots = (first_assign + 1);
		  freeslots = freeslots - 1;
		} else t_num_of_slots = first_assign;
	      } else {
		t_num_of_slots =  (first_assign + share_next_slots);            
		freeslots = freeslots - share_next_slots;
	      }
	      slots_per_con[return_cid_tmp] = t_num_of_slots;
	    }
	    debug10 ("DL.Check1.4.rtps/nrtps, CID(%d), rtps/nrtpsNeedMore: n_Conn :%d, Previous-Slots :%d, Grant-Slots :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, first_assign, t_num_of_slots, freeslots);
	  } else { //BE
	    if(req_slots<1) {
	      con = con->next_entry();
	      continue;
	    }
	    temp_index = doesMapExist(con->get_cid(), cid_list, ie_index);
	    if(temp_index < 0){
	      temp_index = ie_index;
	      cid_list[temp_index] = con->get_cid();
	      slots_per_con[temp_index] = 0;
	      diuc_list[temp_index] = getDIUCProfile(mod_rate);
	      ++ie_index;
	      debug10 ("DL.Check1.4.BE, CID(%d), Initial_BE: n_Conn :%d, REQ-SLOTS :%d, Free :%d\n", cid_list[temp_index], needmore_c, req_slots, freeslots);

	    } else {
	    }// Richard.

	    first_assign = slots_per_con[temp_index];
	    if (req_slots <= (first_assign + share_next_slots) ) {           
	      t_num_of_slots = req_slots;
	      slots_per_con[temp_index] = t_num_of_slots;
	      needmore_c--;
	      freeslots = freeslots - (req_slots-first_assign);

	      debug10 ("DL.Check1.4.BE, Peer-CID(%d), returnCID(%d), BENoNeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", con->get_cid(), cid_list[temp_index], needmore_c, t_num_of_slots, freeslots);
	    } else {
	      if (share_next_slots==0) {
		if (freeslots>0) { 
		  t_num_of_slots = (first_assign + 1);
		  freeslots = freeslots - 1;
		} else t_num_of_slots = first_assign;
	      } else {
		t_num_of_slots =  (first_assign + share_next_slots);            
		freeslots = freeslots - share_next_slots;
	      }
	      slots_per_con[temp_index] = t_num_of_slots;

	      debug10 ("DL.Check1.4.BE, Peer-CID(%d), returnCID(%d), BENeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", con->get_cid(), cid_list[temp_index], needmore_c, t_num_of_slots, freeslots);
	    }
	    //}//richard
	  }    //else BE

	}//else CONNDATA
	con = con->next_entry();
      }//while
    }//for

  }//while
*/

  int total_slots_sofar = check_overallocation(temp_index_burst);
  subchannel_offset = (total_slots_sofar%total_subchannels);
  symbol_offset = DL_PREAMBLE + floor(total_slots_sofar/total_subchannels)*2;
  // Commented by Barun : 21-Sep-2011
  //debug10 ("DL2.end of dl_stage2, Sum allocated slots :%d, Suboffset :%d, Symoffset :%d\n", total_slots_sofar, subchannel_offset, symbol_offset);
  
/*
// In this version, we do the stripping outside the dl_stage2
  if(stripping == VERTICAL_STRIPPING){
    for(i=0; i<index_burst-temp_index_burst; ++i){
      ies[i].cid = virtual_alloc[i+temp_index_burst].cid;
      num_of_slots = virtual_alloc[i+temp_index_burst].numslots;
      ies[i].diuc = virtual_alloc[i+temp_index_burst].iuc;
      ies[i].subchannel_offset = subchannel_offset;
      ies[i].symbol_offset = symbol_offset;
      num_of_subchannels = num_of_slots;
      num_of_symbols = (int) ceil((double)(subchannel_offset + num_of_subchannels)/total_subchannels)*2;
      ies[i].num_of_symbols = num_of_symbols;
      ies[i].num_of_subchannels = num_of_subchannels;

      debug10 ("DL.Check1.5: ie_index(MAX=60) :%d, ies[%d].cid = %d, #Slots :%d, DIUC :%d\n", ie_index, i, ies[i].cid, num_of_slots, ies[i].diuc);
      debug10 ("\tIE_Subchannel_offset (sub_offset<%d>+sub_start<%d>) :%d, #IE_Subchannel :%d\n", subchannel_offset, subchannel_start, ies[i].subchannel_offset, ies[i].num_of_subchannels);
      debug10 ("\tIE_Symbol_offset (sym_offset<%d>+sym_start<%d>) :%d, #Symbols [ceil((sub_offset<%d>+#Subchannel<%d>)/total_sub<%d>)*2] :%d\n", symbol_offset, symbol_start, ies[i].symbol_offset, subchannel_offset, num_of_subchannels, total_subchannels, ies[i].num_of_symbols);

      subchannel_offset = (subchannel_offset + num_of_subchannels)%(total_subchannels);
      symbol_offset += num_of_symbols - 2;
    }
  }
*/

/*
// In this version, we do the stripping outside the dl_stage2
  if(stripping == VERTICAL_STRIPPING){
    for(i=next_ie_index;i<ie_index;++i){
      ies[i].cid = cid_list[i];
      num_of_slots = slots_per_con[i];
      ies[i].diuc = diuc_list[i];
      ies[i].subchannel_offset = subchannel_offset + subchannel_start;
      ies[i].symbol_offset = symbol_offset + symbol_start;
//      debug10 ("1.#Sub :%d, #slot :%d, symbol :%d\n", num_of_subchannels, num_of_slots, num_of_symbols);
      num_of_subchannels = num_of_slots;
      num_of_symbols = (int) ceil((double)(subchannel_offset + num_of_subchannels)/total_subchannels)*2;
      ies[i].num_of_symbols = num_of_symbols;
//      debug10 ("2.Before #Sub :%d, #slot :%d, ies[%d].#sub :%d, .#sym :%d\n", num_of_subchannels, num_of_slots, i, ies[i].num_of_subchannels, ies[i].num_of_symbols);
      ies[i].num_of_subchannels = num_of_subchannels;
//      debug10 ("3.After #Sub :%d, #slot :%d, ies[%d].#sub :%d, .#sym :%d\n", num_of_subchannels, num_of_slots, i, ies[i].num_of_subchannels, ies[i].num_of_symbols);

      debug10 ("DL.Check1.5: ie_index(MAX=60) :%d, ies[%d].cid = %d, #Slots :%d, DIUC :%d\n", ie_index, i, ies[i].cid, num_of_slots, ies[i].diuc);
      debug10 ("\tIE_Subchannel_offset (sub_offset<%d>+sub_start<%d>) :%d, #IE_Subchannel :%d\n", subchannel_offset, subchannel_start, ies[i].subchannel_offset, ies[i].num_of_subchannels);
      debug10 ("\tIE_Symbol_offset (sym_offset<%d>+sym_start<%d>) :%d, #Symbols [ceil((sub_offset<%d>+#Subchannel<%d>)/total_sub<%d>)*2] :%d\n", symbol_offset, symbol_start, ies[i].symbol_offset, subchannel_offset, num_of_subchannels, total_subchannels, ies[i].num_of_symbols);

      subchannel_offset = (subchannel_offset + num_of_subchannels)%(total_subchannels);
      symbol_offset += num_of_symbols - 2;
    }
  }
*/


  ie_index = index_burst - temp_index_burst;

  dl_map->nb_ies = ie_index;
  return dl_map;
}

/* DL Scheduler, Added by Ritun, Modifed by Chakchai */
/*
//This is the previous version of dl_stage2 (do not consider the variable part of DL_MAP
//mac802_16_dl_map_frame * BSScheduler::dl_stage2(Connection *head, int total_subchannels, int total_symbols, int symbol_start, int  stripping){
mac802_16_dl_map_frame * BSScheduler::dl_stage2(Connection *head, int input_subchannel_offset,  int total_subchannels, int total_symbols, int symbol_start, int  stripping){
  struct mac802_16_dl_map_frame *dl_map;
  struct mac802_16_dlmap_ie *ies;
  Connection *con;
  int i, ie_index, temp_index;
  int num_of_symbols, num_of_subchannels, num_of_slots;
  double allocationsize;
  int freeslots;
  int symbol_offset = 0;
  //int subchannel_offset = 0;
  int subchannel_offset = input_subchannel_offset;
  int subchannel_start = 0;
  int num_of_data_connections = 0;
  ConnectionType_t contype;
  SchedulingType_t schedtype;
  Mac802_16 *  mac_ = getMac ();
  Ofdm_mod_rate mod_rate;

  int slots_per_con[MAX_MAP_IE];
  int cid_list[MAX_MAP_IE];
  int diuc_list[MAX_MAP_IE];

  int leftOTHER_slots=0;
  int needmore_con[3]={0,0,0};
  int needmore_c=0;
  int ori_symbols = total_symbols;
  int req_slots_tmp1 = 0;
  int return_cid_tmp = 0;

  dl_map = (struct mac802_16_dl_map_frame *) malloc(sizeof(struct mac802_16_dl_map_frame));
  dl_map->type = MAC_DL_MAP;
  dl_map->bsid = mac_->addr(); // its called in the mac_ object

  ies = dl_map->ies;
  ie_index = 0; //0 is for the ul-map

  //2symbols * 2 clusters = 4: -> 1 slot, since we use #sub_channel (which is 14*2 subcarriers), total_symbols -> horizontal
  //Check it again => can't be devided by 2
  if ((total_symbols%2)==1) total_symbols=total_symbols-1;

//Chakchai next version
//freeslots = (total_subchannels-subchannel_offset)+ (total_subchannels*(total_symbols-2)/2);
  freeslots = total_subchannels * total_symbols/2;

  debug10 ("DL.Check1.0, ****FRAME NUMBER**** :%d, FreeSlots(TotalSub*TotalSym/2) :%d, TotalSub :%d, Ori_TotalSymbol :%d, TotalSymbol :%d, StartSymbol :%d\n", frame_number, freeslots, total_subchannels, ori_symbols, total_symbols, symbol_start );

  frame_number++;
//  debug10 ("ARQ block :%d\n", getMac()->arq_block_size_);

  con=head;
  while(con!=NULL){
    if(con->get_category()==CONN_DATA)
      ++num_of_data_connections;
    con = con->next_entry();
  }

  for(i=0;i<3;++i){
    con = head;
    if(i==0) 		contype = CONN_BASIC;
    else if(i==1) 	contype = CONN_PRIMARY;
    else 		contype = CONN_SECONDARY;

    
    while(con!=NULL)
      {
	if(con->get_category() == contype)
	  {
	    debug2("xingting==  connection id [%d]\n",con->get_cid());
	    allocationsize = con->queueByteLength();
	    mod_rate = mac_->getMap()->getDlSubframe()->getProfile(con->getPeerNode()->getDIUC())->getEncoding();
	    num_of_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));

	    debug10 ("DL.Check1.1.contype(%d), CID :%d, Q-Bytes :%d, allocationsize :%f, #slots :%d\n", contype, con->get_cid(), con->queueByteLength(), allocationsize, num_of_slots);
	    debug10 ("\tfree slots :%d, left-over :%d\n", freeslots, freeslots-num_of_slots);

	    if(freeslots < num_of_slots) num_of_slots = freeslots;
	    freeslots -= num_of_slots;

	    if(ie_index<MAX_MAP_IE && num_of_slots > 0 && stripping == HORIZONTAL_STRIPPING)
	      {
		ies[ie_index].cid = con->get_cid();
		ies[ie_index].n_cid = 1;
		ies[ie_index].diuc = getDIUCProfile(mod_rate);
      		num_of_subchannels = num_of_slots;
      		ies[ie_index].subchannel_offset = subchannel_offset + subchannel_start;
      		ies[ie_index].symbol_offset = symbol_offset + symbol_start;
      		ies[ie_index].num_of_subchannels = num_of_subchannels;
      		num_of_symbols = (int) ceil( (double)(symbol_offset + num_of_slots) / (total_symbols) );
      		ies[ie_index].num_of_symbols = num_of_symbols;

      		subchannel_offset += num_of_symbols;
      		symbol_offset = (symbol_offset + num_of_slots) % (total_symbols);

		++ie_index;
	      }
	    else if(ie_index<MAX_MAP_IE && num_of_slots > 0 && stripping == VERTICAL_STRIPPING)
	      {
		ies[ie_index].cid = con->get_cid();
		ies[ie_index].n_cid = 1;
		ies[ie_index].diuc = getDIUCProfile(mod_rate);

		ies[ie_index].subchannel_offset = (subchannel_offset + subchannel_start) % total_subchannels;
		ies[ie_index].symbol_offset = symbol_offset + symbol_start;

		num_of_subchannels = num_of_slots;
		num_of_symbols = (int) ceil((double)(subchannel_offset + num_of_subchannels)/total_subchannels)*2;
		debug2 ("In DL scheduler, number of symbols = %d\n", num_of_symbols);
		ies[ie_index].num_of_symbols = num_of_symbols;
		ies[ie_index].num_of_subchannels = num_of_subchannels;

		debug10 ("DL.Check1.2: ie_index(MAX=60) :%d, ies[%d].cid :%d, #Slots :%d, UIUC :%d, IE_Subchannel_offset (sub_offset<%d>+sub_start<%d>) :%d\n", ie_index, i, ies[ie_index].cid, num_of_slots, ies[ie_index].diuc, subchannel_offset, subchannel_start, ies[ie_index].subchannel_offset);
		debug10 ("DL.Check1.2: IE_Symbol_offset (sym_offset<%d>+sym_start<%d>) :%d, .#Subchannel (=#slots) :%d, #Symbols [ceil((sub_offset<%d>+#Subchannel<%d>)/total_sub<%d>)*2] :%d\n", symbol_offset, symbol_start, ies[ie_index].symbol_offset, num_of_subchannels, subchannel_offset, num_of_subchannels, total_subchannels, num_of_symbols);

		subchannel_offset = (subchannel_offset + num_of_subchannels)%(total_subchannels);
		symbol_offset += num_of_symbols - 2;
		++ie_index;
	      }
	  }
	con = con->next_entry();
      }//end con!=null
  }//end for loop


  int next_ie_index = ie_index;
  //***************
  //Assign slot for ugs/ertps/rtps/nrtps/be
  //Find out how many connection for each type of traffic

  int conn_per_schetype[6]={0,0,0,0,0,0};
  conn_per_schetype[0]=0; conn_per_schetype[1]=0; conn_per_schetype[2]=0; conn_per_schetype[3]=0; conn_per_schetype[4]=0; conn_per_schetype[5]=0;

  int con_ugs_all=0;
  int con_ertps_all=0;
  int con_rtps_all=0;  
  int con_nrtps_all=0;
  int con_be_all=0;

  for(i=0;i<5;++i){
    con = head;
    if(i==0)            schedtype = SERVICE_UGS;
    else if(i==1)       schedtype = SERVICE_ertPS;
    else if(i==2)       schedtype = SERVICE_rtPS;    
    else if(i==3)       schedtype = SERVICE_nrtPS; 
    else                schedtype = SERVICE_BE;

    while(con!=NULL){
      if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype){
	if(schedtype==SERVICE_UGS) {
	  if (con->queueByteLength() > 0) conn_per_schetype[0]++;
	  con_ugs_all++;
	} else if (schedtype==SERVICE_ertPS) {
	  if (con->queueByteLength() > 0) conn_per_schetype[1]++;
	  con_ertps_all++;
	} else if (schedtype==SERVICE_rtPS) {
	  if (con->queueByteLength() > 0) conn_per_schetype[2]++;
	  con_rtps_all++;
	} else if (schedtype==SERVICE_nrtPS) {
	  if (con->queueByteLength() > 0) conn_per_schetype[3]++;
	  con_nrtps_all++;
	} else if (schedtype==SERVICE_BE) {
	  if (con->queueByteLength() > 0) conn_per_schetype[4]++;
	  con_be_all++;
	} else {
	  conn_per_schetype[5]++;
	}
      }
      con = con->next_entry();
    }
  }
  debug10 ("DL, Active UGS <%d>, ertPS <%d>, rtPS <%d>, nrtPS <%d>, BE <%d>, OTHER <%d>\n", conn_per_schetype[0],conn_per_schetype[1], conn_per_schetype[2], conn_per_schetype[3], conn_per_schetype[4], conn_per_schetype[5] );
  debug10 ("    All    UGS <%d>, ertPS <%d>, rtPS <%d>, nrtPS <%d>, BE <%d>\n", con_ugs_all, con_ertps_all, con_rtps_all, con_nrtps_all, con_be_all);

  leftOTHER_slots = freeslots;
  for(i=0;i<5;++i){
    con = head;
    if(i==0)            schedtype = SERVICE_UGS;
    else if(i==1)       schedtype = SERVICE_ertPS;
    else if(i==2)       schedtype = SERVICE_rtPS;
    else if(i==3)       schedtype = SERVICE_nrtPS;
    else                schedtype = SERVICE_BE;

    
    while(con!=NULL){
      if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype){

        req_slots_tmp1 = 0;
        return_cid_tmp = 0;
        int grant_slots = 0;
        num_of_slots = 0; 

        return_cid_tmp = 0;
        temp_index = doesMapExist(con->get_cid(), cid_list, ie_index);
        if(temp_index < 0) return_cid_tmp = -1;
        else return_cid_tmp = con->get_cid();
        mod_rate = mac_->getMap()->getDlSubframe()->getProfile(con->getPeerNode()->getDIUC())->getEncoding();


        if(schedtype==SERVICE_UGS){
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
	  if (con->queueByteLength() > 0) {

#ifdef UGS_AVG
          allocationsize = (int) ceil((double)sfqos->getDataSize()/(double)sfqos->getPeriod());
#endif
#ifndef UGS_AVG
          int tmp_getpoll = con->getPOLL_interval();
          if ( (tmp_getpoll%sfqos->getPeriod())== 0 ) {
                allocationsize = ceil(sfqos->getDataSize());
                con->setPOLL_interval(0);
          } else {
                allocationsize = 0;
          }
          tmp_getpoll++;
          con->setPOLL_interval(tmp_getpoll);
#endif

	  } else {
	     allocationsize = 0;
	  }

	  int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
	  int t_bytes = 0;
	  double ori_grant = allocationsize;
	  if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
		t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
		if (t_bytes > 0) {
			allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
		}
		debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
		allocationsize = allocationsize + 14;	//optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub
		
	  } 

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));;

	  if (con->queueByteLength() <=0) num_of_slots = 0;
	  else num_of_slots = grant_slots;

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("Fatal Error: There is not enough resource for UGS\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;

	  debug10 ("DL.Check1.3.UGS, DataSize :%f, period :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getDataSize(), sfqos->getPeriod(), grant_slots, con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
	  debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofUGS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[0]);
	  debug10 ("\tQ-bytes :%d, Grant-bytes :%ld\n", con->queueByteLength(), (long int)( grant_slots*mac_->getPhy()->getSlotCapacity(mod_rate, DL_) ));

        }

        if(schedtype == SERVICE_ertPS) {

	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
          if (con->queueByteLength() > 0) {
             allocationsize = (int) ceil((double)sfqos->getDataSize()/(double)sfqos->getPeriod());
          } else {
             allocationsize = 0;
          }

          int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          } 
	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));;

	  if (con->queueByteLength() <=0) num_of_slots = 0;
	  else num_of_slots = grant_slots;

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));
	  num_of_slots = grant_slots;

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("Fatal Error: There is not enough resource for ertPS\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;


	  debug10 ("DL.Check1.3.ertPS, DataSize :%f, period :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getDataSize(), sfqos->getPeriod(), grant_slots, con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
	  debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofertPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[1]);
	  debug10 ("\tQ-bytes :%d, Grant-bytes :%ld\n", con->queueByteLength(), (long int)( grant_slots*mac_->getPhy()->getSlotCapacity(mod_rate, DL_) ));

        }
        
        
        if(schedtype == SERVICE_rtPS) {
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
          int withfrag = 0;
          if  (con->getFragmentBytes()>0) {
               withfrag = con->queueByteLength() - con->getFragmentBytes() + 2;
          } else {
               withfrag = con->queueByteLength();
          }
	  req_slots_tmp1 = (int) ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_));

	  if (req_slots_tmp1>0) allocationsize = (int) ceil((double)(sfqos->getMinReservedRate()*FRAME_SIZE)/8);
	  else  allocationsize = 0;

          int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          }

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));
	  num_of_slots = grant_slots;

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("Fatal Error: There is not enough resource for rtPS\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;

	  if (grant_slots<req_slots_tmp1) needmore_con[0]++;

	  debug10 ("DL.Check1.3.rtPS, MinReservedRate :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getMinReservedRate(), grant_slots, con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
	  debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofrtPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[2]);
	  debug10 ("\tQ-bytes :%d, Grant-bytes :%ld\n", con->queueByteLength(), (long int)( grant_slots*mac_->getPhy()->getSlotCapacity(mod_rate, DL_) ));
        
        }


        if(schedtype == SERVICE_nrtPS) {
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
	  int withfrag = 0;
          if  (con->getFragmentBytes()>0) {
               withfrag = con->queueByteLength() - con->getFragmentBytes() + 2;
          } else {
               withfrag = con->queueByteLength();;
          }

	  req_slots_tmp1 = (int) ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_));

	  if (req_slots_tmp1>0) allocationsize = (int) ceil((sfqos->getMinReservedRate()*FRAME_SIZE)/8);
	  else  allocationsize = 0;

          int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          }

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, DL_));
	  num_of_slots = grant_slots;

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("Fatal Error: There is not enough resource for nrtPS\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;

	  if (grant_slots<req_slots_tmp1) needmore_con[1]++;

	  debug10 ("DL.Check1.3.nrtPS, MinReservedRate :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getMinReservedRate(), grant_slots, con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
	  debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofnrtPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[3]);
	  debug10 ("\tQ-bytes :%d, Grant-bytes :%ld\n", con->queueByteLength(), (long int)( grant_slots*mac_->getPhy()->getSlotCapacity(mod_rate, DL_) ));

        }

        if(schedtype==SERVICE_BE){
	  int withfrag = 0;
          if  (con->getFragmentBytes()>0) {
               withfrag = con->queueByteLength() - con->getFragmentBytes() + 2;
          } else {
               withfrag = con->queueByteLength();;
          }

	  req_slots_tmp1 = (int) ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_));

	  if (req_slots_tmp1>0) needmore_con[2]++;

	  debug10 ("DL.Check1.3.BE, Peer-CID :%d, returnCID :%d, DIUC :%d\n", con->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
	  debug10 ("\tQ-bytes :%d\n", con->queueByteLength());
	  con = con->next_entry();
	  continue;
        }

        freeslots = leftOTHER_slots;

        if(num_of_slots> 0) {
	  temp_index = doesMapExist(con->get_cid(), cid_list, ie_index);

	  if(temp_index < 0){
	    temp_index = ie_index;
	    cid_list[temp_index] = con->get_cid();
	    slots_per_con[temp_index] = num_of_slots;
	    diuc_list[temp_index] = getDIUCProfile(mod_rate);
	    ++ie_index;

	    debug10 ("DL.Check1.3: MapNotExist New_CID :%d, return index :-1, cid_list[%d] :%d,  #entry (ie_index)++ :%d\n", con->get_cid(), temp_index, cid_list[temp_index], ie_index);
	    debug10 ("\tNumSlots1.2 :%d\n", num_of_slots);

	  } else if(temp_index<MAX_MAP_IE) {
	    slots_per_con[temp_index] += num_of_slots;

	    debug10 ("DL.Check1.3: MapExist Peer_CID :%d, return index :%d, cid_list[%d] :%d,  #entry (ie_index) :%d\n", con->get_cid(), temp_index, temp_index, cid_list[temp_index], ie_index);
	    debug10 ("\tNumSlots1.2 :%d\n", num_of_slots);

	  } else {
	    freeslots += num_of_slots;                      //return back the slots
	    leftOTHER_slots += num_of_slots;                //return back the slots
	  }

	  debug10 ("DL.Check1.3, First Assign (ugs/ertps/rtps/nrtps/no be): CID(%d), Schetype :%d, Numslots :%d, Freeleft :%d, StoreSlots[%d] :%d, mod_rate :%d, DIUC :%d\n", cid_list[temp_index], schedtype, num_of_slots, freeslots, temp_index, slots_per_con[temp_index], mod_rate, diuc_list[temp_index]);
        }

      }
      con = con->next_entry();
    }//end con != NULL
  }//end for loop 5 Qos


  //Assign left-over to rtPS, ertPS, and BE fairly
  int share_next_slots = 0;
  for (int i=0;i<3;i++) 
    needmore_c = needmore_c+needmore_con[i];
  if (needmore_c>0) 
    share_next_slots = (int) floor(leftOTHER_slots/needmore_c);

  int first_assign = 0;

  while (needmore_c>0 && freeslots>0) {

    share_next_slots = (int) floor(freeslots/needmore_c);

    debug10 ("DL.Check1.4, (Check still need more here): Needmore Conn :%d, Free :%d, Sharenext :%d\n", needmore_c, freeslots, share_next_slots);

    for(i=0;i<5;++i){
      con = head;
      if(i==0)        schedtype = SERVICE_UGS;
      else if(i==1)   schedtype = SERVICE_ertPS;
      else if(i==2)   schedtype = SERVICE_rtPS;
      else if(i==3)   schedtype = SERVICE_nrtPS;
      else            schedtype = SERVICE_BE;
    
      if ( (i==0) || (i==1) ) continue;

      first_assign = 0;
      while(con!=NULL) {
	debug2 ("Rich: Con %p cid=%d\n", con, con->get_cid());
	if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype) {

	  mod_rate = mac_->getMap()->getDlSubframe()->getProfile(con->getPeerNode()->getDIUC())->getEncoding();
	  temp_index = doesMapExist(con->get_cid(), cid_list, ie_index);
	  if(temp_index < 0) return_cid_tmp = -1;
	  else return_cid_tmp = temp_index;

	  int withfrag = 0;
          if  (con->getFragmentBytes()>0) {
               withfrag = con->queueByteLength() - con->getFragmentBytes() + 2;
          } else {
               withfrag = con->queueByteLength();;
          }
	  int req_slots = (int) ceil((double)withfrag/(double)mac_->getPhy()->getSlotCapacity(mod_rate, DL_)); 

	  if ( (schedtype==SERVICE_rtPS) || (schedtype==SERVICE_nrtPS) ) {
	    if (return_cid_tmp == -1) {
	      debug10 ("DL.Check1.4.rtps/nrtps, No_CID(%d), n_Conn :%d, Free :%d\n", return_cid_tmp, needmore_c, freeslots);
	      con = con->next_entry();
	      continue;
	    }
	    if ( req_slots<=slots_per_con[return_cid_tmp] ) {
	      debug10 ("DL.Check1.4.rtPS/nrtPS, CID(%d), <=MinSlots: n_Conn :%d, REQ-SLOTS :%d, GRANT-1SLOTS :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, req_slots, slots_per_con[return_cid_tmp], freeslots);
	      con = con->next_entry();
	      continue;
	    }
	  }

	  int t_num_of_slots = 0;

	  if ( (schedtype==SERVICE_rtPS) || (schedtype==SERVICE_nrtPS) ) {
	    first_assign = slots_per_con[return_cid_tmp];

	    if (req_slots <= (first_assign + share_next_slots) ) {           
	      t_num_of_slots = req_slots;
	      slots_per_con[return_cid_tmp] = t_num_of_slots;
	      needmore_c--;
	      freeslots = freeslots - (req_slots-first_assign);
	      debug10 ("DL.Check1.4.rtps/nrtps, CID(%d), rtps/nrtpsNoNeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, t_num_of_slots, freeslots);
	    } else {
	      if (share_next_slots==0) {
		if (freeslots>0) { 
		  t_num_of_slots = (first_assign + 1);
		  freeslots = freeslots - 1;
		} else t_num_of_slots = first_assign;
	      } else {
		t_num_of_slots =  (first_assign + share_next_slots);            
		freeslots = freeslots - share_next_slots;
	      }
	      slots_per_con[return_cid_tmp] = t_num_of_slots;
	    }
	    debug10 ("DL.Check1.4.rtps/nrtps, CID(%d), rtps/nrtpsNeedMore: n_Conn :%d, Previous-Slots :%d, Grant-Slots :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, first_assign, t_num_of_slots, freeslots);
	  } else { //BE
	    if(req_slots<1) {
	      con = con->next_entry();
	      continue;
	    }
	    temp_index = doesMapExist(con->get_cid(), cid_list, ie_index);
	    if(temp_index < 0){
	      temp_index = ie_index;
	      cid_list[temp_index] = con->get_cid();
	      slots_per_con[temp_index] = 0;
	      diuc_list[temp_index] = getDIUCProfile(mod_rate);
	      ++ie_index;
	      debug10 ("DL.Check1.4.BE, CID(%d), Initial_BE: n_Conn :%d, REQ-SLOTS :%d, Free :%d\n", cid_list[temp_index], needmore_c, req_slots, freeslots);

	    } else {
	    }// Richard.

	    first_assign = slots_per_con[temp_index];
	    if (req_slots <= (first_assign + share_next_slots) ) {           
	      t_num_of_slots = req_slots;
	      slots_per_con[temp_index] = t_num_of_slots;
	      needmore_c--;
	      freeslots = freeslots - (req_slots-first_assign);

	      debug10 ("DL.Check1.4.BE, Peer-CID(%d), returnCID(%d), BENoNeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", con->get_cid(), cid_list[temp_index], needmore_c, t_num_of_slots, freeslots);
	    } else {
	      if (share_next_slots==0) {
		if (freeslots>0) { 
		  t_num_of_slots = (first_assign + 1);
		  freeslots = freeslots - 1;
		} else t_num_of_slots = first_assign;
	      } else {
		t_num_of_slots =  (first_assign + share_next_slots);            
		freeslots = freeslots - share_next_slots;
	      }
	      slots_per_con[temp_index] = t_num_of_slots;

	      debug10 ("DL.Check1.4.BE, Peer-CID(%d), returnCID(%d), BENeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", con->get_cid(), cid_list[temp_index], needmore_c, t_num_of_slots, freeslots);
	    }
	    //}//richard
	  }    //else BE

	}//else CONNDATA
	con = con->next_entry();
      }//while
    }//for

  }//while

  if(stripping == HORIZONTAL_STRIPPING){
    for(i=next_ie_index;i<ie_index;++i){
      ies[i].cid = cid_list[i];
      num_of_slots = slots_per_con[i];
      ies[i].diuc = diuc_list[i];

      num_of_subchannels = num_of_slots;
      ies[i].subchannel_offset = subchannel_offset + subchannel_start;
      ies[i].symbol_offset = symbol_offset + symbol_start;
      ies[i].num_of_subchannels = num_of_subchannels;
      num_of_symbols = (int) ceil( (double)(symbol_offset + num_of_slots) / (total_symbols) );
      ies[i].num_of_symbols = num_of_symbols;

      debug10 ("DL.Check1.5: ie_index(MAX=60) :%d, ies[%d].cid :%d, #Slots :%d, DIUC :%d\n", ie_index, i, ies[i].cid, num_of_slots, ies[i].diuc);
      debug10 ("\tIE_Subchannel_offset (sub_offset<%d>+sub_start<%d>) :%d, #IE_Subchannel :%d\n", subchannel_offset, subchannel_start, ies[i].subchannel_offset, ies[i].num_of_subchannels);
      debug10 ("\tIE_Symbol_offset (sym_offset<%d>+sym_start<%d>) :%d, #Symbols [ceil((sub_offset<%d>+#Subchannel<%d>)/total_sub<%d>)*3] :%d\n", symbol_offset, symbol_start, ies[i].symbol_offset, subchannel_offset, num_of_subchannels, total_subchannels, ies[i].num_of_symbols);
      subchannel_offset += num_of_symbols;
      symbol_offset = (symbol_offset + num_of_slots) % (total_symbols);

    }
  }

  if(stripping == VERTICAL_STRIPPING){
    for(i=next_ie_index;i<ie_index;++i){
      ies[i].cid = cid_list[i];
      num_of_slots = slots_per_con[i];
      ies[i].diuc = diuc_list[i];
      ies[i].subchannel_offset = subchannel_offset + subchannel_start;
      ies[i].symbol_offset = symbol_offset + symbol_start;
//      debug10 ("1.#Sub :%d, #slot :%d, symbol :%d\n", num_of_subchannels, num_of_slots, num_of_symbols);
      num_of_subchannels = num_of_slots;
      num_of_symbols = (int) ceil((double)(subchannel_offset + num_of_subchannels)/total_subchannels)*2;
      ies[i].num_of_symbols = num_of_symbols;
//      debug10 ("2.Before #Sub :%d, #slot :%d, ies[%d].#sub :%d, .#sym :%d\n", num_of_subchannels, num_of_slots, i, ies[i].num_of_subchannels, ies[i].num_of_symbols);
      ies[i].num_of_subchannels = num_of_subchannels;
//      debug10 ("3.After #Sub :%d, #slot :%d, ies[%d].#sub :%d, .#sym :%d\n", num_of_subchannels, num_of_slots, i, ies[i].num_of_subchannels, ies[i].num_of_symbols);

      debug10 ("DL.Check1.5: ie_index(MAX=60) :%d, ies[%d].cid = %d, #Slots :%d, DIUC :%d\n", ie_index, i, ies[i].cid, num_of_slots, ies[i].diuc);
      debug10 ("\tIE_Subchannel_offset (sub_offset<%d>+sub_start<%d>) :%d, #IE_Subchannel :%d\n", subchannel_offset, subchannel_start, ies[i].subchannel_offset, ies[i].num_of_subchannels);
      debug10 ("\tIE_Symbol_offset (sym_offset<%d>+sym_start<%d>) :%d, #Symbols [ceil((sub_offset<%d>+#Subchannel<%d>)/total_sub<%d>)*2] :%d\n", symbol_offset, symbol_start, ies[i].symbol_offset, subchannel_offset, num_of_subchannels, total_subchannels, ies[i].num_of_symbols);

      subchannel_offset = (subchannel_offset + num_of_subchannels)%(total_subchannels);
      symbol_offset += num_of_symbols - 2;
    }
  }



  dl_map->nb_ies = ie_index;
  return dl_map;
}
*/

/* returns the DIUC profile associated with a current modulation and coding scheme
 * Added by Ritun
 */
diuc_t BSScheduler::getDIUCProfile(Ofdm_mod_rate rate){
  Profile *p;

  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_1);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_1;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_2);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_2;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_3);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_3;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_4);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_4;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_5);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_5;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_6);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_6;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_7);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_7;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_8);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_8;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_9);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_9;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_10);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_10;
  p = getMac()->getMap()->getDlSubframe()->getProfile(DIUC_PROFILE_11);
  if(p->getEncoding() == rate)
    return DIUC_PROFILE_11;
  
  return DIUC_PROFILE_1;
}

/* Added by Ritun
 */
uiuc_t BSScheduler::getUIUCProfile(Ofdm_mod_rate rate){
  Profile *p;

  p = getMac()->getMap()->getUlSubframe()->getProfile(UIUC_PROFILE_1);
  if(p->getEncoding() == rate)
    return UIUC_PROFILE_1;
  p = getMac()->getMap()->getUlSubframe()->getProfile(UIUC_PROFILE_2);
  if(p->getEncoding() == rate)
    return UIUC_PROFILE_2;
  p = getMac()->getMap()->getUlSubframe()->getProfile(UIUC_PROFILE_3);
  if(p->getEncoding() == rate)
    return UIUC_PROFILE_3;
  p = getMac()->getMap()->getUlSubframe()->getProfile(UIUC_PROFILE_4);
  if(p->getEncoding() == rate)
    return UIUC_PROFILE_4;
  p = getMac()->getMap()->getUlSubframe()->getProfile(UIUC_PROFILE_5);
  if(p->getEncoding() == rate)
    return UIUC_PROFILE_5;
  p = getMac()->getMap()->getUlSubframe()->getProfile(UIUC_PROFILE_6);
  if(p->getEncoding() == rate)
    return UIUC_PROFILE_6;
  p = getMac()->getMap()->getUlSubframe()->getProfile(UIUC_PROFILE_7);
  if(p->getEncoding() == rate)
    return UIUC_PROFILE_7;
  p = getMac()->getMap()->getUlSubframe()->getProfile(UIUC_PROFILE_8);
  if(p->getEncoding() == rate)
    return UIUC_PROFILE_8;
  
  return UIUC_PROFILE_1;
}

//Modified by Chakchai
//Unlike dl_stage2, next version will be a wrap up version fo the UL scheduler with horizontal stripping
struct mac802_16_ul_map_frame * BSScheduler::ul_stage2(Connection *head, int total_subchannels, int total_symbols, int symbol_start, int stripping){

  struct mac802_16_ul_map_frame *ul_map;
  struct mac802_16_ulmap_ie *ies;
  Connection *con;
  int i, ie_index, temp_index;
  int num_of_slots, num_of_symbols, num_of_subchannels;
  double allocationsize;
  int freeslots;
  int symbol_offset = 0;
  int subchannel_offset = 0;
  int subchannel_start = 0;
  ConnectionType_t contype;
  SchedulingType_t schedtype;
  int slots_per_con[MAX_MAP_IE];
  int cid_list[MAX_MAP_IE];
  int uiuc_list[MAX_MAP_IE];
  int cdma_flag_list[MAX_MAP_IE];
  u_char cdma_code_list[MAX_MAP_IE];
  u_char cdma_top_list[MAX_MAP_IE];

  Ofdm_mod_rate mod_rate;

  int leftOTHER_slots=0;
  int needmore_con[3]={0,0,0};
  int needmore_c=0;
  int ori_symbols = total_symbols;
  int req_slots_tmp1 = 0;
  int return_cid_tmp = 0;
  num_of_slots=0;
  int tmp_cdma_code_top[CODE_SIZE][CODE_SIZE];

  for(int i=0;i<CODE_SIZE;i++){
    for(int j=0;j<CODE_SIZE;j++){
      tmp_cdma_code_top[i][j] = 0;
    }
  }

  if ((total_symbols%3)==1) total_symbols=total_symbols-1;
  else if ((total_symbols%3)==2) total_symbols=total_symbols-2;

  freeslots = total_subchannels * total_symbols/3;
  leftOTHER_slots = freeslots;

  //debug10 ("UL.Check1.0, ****FRAME NUMBER**** :%d, FreeSlots(TotalSub*TotalSym/3) :%d, TotalSub :%d, OriSymbol(maxul-ulduration) :%d, TotalSymbol :%d, StartSymbol :%d\n", frame_number, freeslots, total_subchannels, ori_symbols, total_symbols, symbol_start );

  ul_map = (struct mac802_16_ul_map_frame *) malloc(sizeof(struct mac802_16_ul_map_frame));
  ul_map->type = MAC_UL_MAP;

  ies = ul_map->ies;
  ie_index = 0;

  bzero(slots_per_con, MAX_MAP_IE*sizeof(int));
  bzero(cid_list, MAX_MAP_IE*sizeof(int));
  for (int i=0; i<MAX_MAP_IE; i++) {
      cdma_code_list[i] = 0;
      cdma_top_list[i] = 0;
      cdma_flag_list[i] = 0;
  }

//Check if cdma_init_ranging request
  con = head;
  while (con!=NULL){
    if(con->get_category() == CONN_INIT_RANGING){
        if (con->getCDMA() == 2) {
           int begin_code = 0;
           int begin_top = 0;
           int begin_flag = 0;
           for (int i = 0; i<MAX_SSID; i++) {
              begin_flag = con->getCDMA_SSID_FLAG(i);
              begin_code = con->getCDMA_SSID_CODE(i);
              begin_top = con->getCDMA_SSID_TOP(i);
              if (begin_flag == 0) continue;
              for (int j = i+1; j<MAX_SSID; j++) {
                if (con->getCDMA_SSID_FLAG(j) == 0) continue;
                if ( (begin_code == con->getCDMA_SSID_CODE(j)) && (begin_top == con->getCDMA_SSID_TOP(j)) ) {
                   //*debug10 ("=Collission CDMA_INIT_RNG_REQ (ssid i :%d and ssid j :%d), CDMA_flag i :%d and CDMA_flag j:%d, CDMA_code i :%d, CDMA_top i :%d\n", con->getCDMA_SSID_SSID(i), con->getCDMA_SSID_SSID(j), con->getCDMA_SSID_FLAG(i), con->getCDMA_SSID_FLAG(j), con->getCDMA_SSID_CODE(i), con->getCDMA_SSID_TOP(i));
                   con->setCDMA_SSID_FLAG(j, 0);
                   con->setCDMA_SSID_FLAG(i, 0);
                }
              }
           }
        }

        con->setCDMA(0);
        break;
    }
    con = con->next_entry();
  }

  con = head;
  while (con!=NULL){
    if(con->get_category() == CONN_INIT_RANGING){
      for (int i = 0; i<MAX_SSID; i++) {
         int cdma_flag = con->getCDMA_SSID_FLAG(i);
         if (cdma_flag > 0) {
            

/*
//0 to 6
debug10 (" bpsk1/2 :%d, qpsk1/2 :%d, qpsk3/4 :%d, 16qam1/2 :%d, 16qam3/4 :%d, 64qam2/3 :%d, 64qam3/4 :%d\n", OFDM_BPSK_1_2,   OFDM_QPSK_1_2,   OFDM_QPSK_3_4,  OFDM_16QAM_1_2, OFDM_16QAM_3_4,  OFDM_64QAM_2_3, OFDM_64QAM_3_4);
  
//5 to 11
debug10 (" uiuc_p1 :%d, uiuc_p2 :%d, uiuc_p3 :%d, uiuc_p4 :%d, uiuc_p5 :%d, uiuc_p6 :%d, uiuc_p7 :%d\n", UIUC_PROFILE_1, UIUC_PROFILE_2, UIUC_PROFILE_3, UIUC_PROFILE_4, UIUC_PROFILE_5, UIUC_PROFILE_6, UIUC_PROFILE_7);

//1 to 7
debug10 (" diuc_p1 :%d, diuc_p2 :%d, diuc_p3 :%d, diuc_p4 :%d, diuc_p5 :%d, diuc_p6 :%d, diuc_p7 :%d\n", DIUC_PROFILE_1, DIUC_PROFILE_2, DIUC_PROFILE_3, DIUC_PROFILE_4, DIUC_PROFILE_5, DIUC_PROFILE_6, DIUC_PROFILE_7);

for (int i = 0; i <7; i++ ) {
  debug10 ("mod_rate :%d, UIUC_Profile :%d\n", i, (int)getUIUCProfile((Ofdm_mod_rate)i) );
}
*/

            mod_rate = (Ofdm_mod_rate)1;
	   //mod_rate = mac_->getMap()->getUlSubframe()->getProfile(con->getPeerNode()->getDIUC()-DIUC_PROFILE_1+UIUC_PROFILE_1)->getEncoding();
//            allocationsize =  RNG_REQ_SIZE+GENERIC_HEADER_SIZE;
            allocationsize =  RNG_REQ_SIZE;
            int num_of_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
            con->setCDMA_SSID_FLAG(i,0);
            // Commented by Barun : 21-Sep-2011
            //debug10("=> Allocate init_ranging_msg opportunity for ssid :%d, code :%d, top :%d, size :%f\n", con->getCDMA_SSID_SSID(i), con->getCDMA_SSID_CODE(i), con->getCDMA_SSID_TOP(i), allocationsize);


            if(freeslots < num_of_slots) num_of_slots = freeslots;

            freeslots -= num_of_slots;

            if (num_of_slots> 0){
               int temp_index = ie_index++;
               cid_list[temp_index] = con->get_cid();
               slots_per_con[temp_index] = num_of_slots;
               uiuc_list[temp_index] = getUIUCProfile(mod_rate);
               cdma_code_list[temp_index] = con->getCDMA_SSID_CODE(i);
               cdma_top_list[temp_index] = con->getCDMA_SSID_TOP(i);
               cdma_flag_list[temp_index] = 1;
            }
	    con->setCDMA_SSID_FLAG(i, 0);
            con->setCDMA_SSID_CODE(i, 0);
            con->setCDMA_SSID_TOP(i, 0);
         }
      }
    }
    con = con->next_entry();
  }


  
//Check if cdma_bandwidth_ranging request
  for(i=0;i<4;++i){
    con = head;
    if(i==0) 	  contype = CONN_BASIC;
    else if(i==1) contype = CONN_PRIMARY;
    else if(i==2) contype = CONN_SECONDARY;
    else contype = CONN_DATA;

    while(con!=NULL){
      if(con->get_category() == contype){
	if (con->getCDMA() == 1) {
	  tmp_cdma_code_top[(int)con->getCDMA_code()][(int)con->getCDMA_top()]++;
	}
        if (con->getCDMA()>0) {
            // Commented by Barun : 21-Sep-2011
      	  //debug10 ("=Contype :%d, CDMA_flag :%d, CDMA_code :%d, CDMA_top :%d, CDMA_code_top++ :%d\n", contype, con->getCDMA(), con->getCDMA_code(), con->getCDMA_top(), tmp_cdma_code_top[(int)con->getCDMA_code()][(int)con->getCDMA_top()]);
	}
      }
      con = con->next_entry();
    }

  }//end for

  for(i=0;i<4;++i){
    con = head;
    if(i==0) 	  contype = CONN_BASIC;
    else if(i==1) contype = CONN_PRIMARY;
    else if(i==2) contype = CONN_SECONDARY;
    else contype = CONN_DATA;

    while(con!=NULL){
      if(con->get_category() == contype){
//      	debug10 ("=Contype :%d, CDMA_flag :%d, CDMA_code :%d, CDMA_top :%d\n", contype, con->getCDMA(), con->getCDMA_code(), con->getCDMA_top());
	if (con->getCDMA() == 1) {
	  if ( tmp_cdma_code_top[(int)con->getCDMA_code()][(int)con->getCDMA_top()] >1 ) {
	    con->setCDMA(0);
	    // Commented by Barun : 21-Sep-2011
	    //debug10 ("=Collission CDMA_BW_REQ, Contype :%d, CDMA_flag :%d, CDMA_code :%d, CDMA_top :%d, CDMA_code_top :%d\n", contype, con->getCDMA(), con->getCDMA_code(), con->getCDMA_top(), tmp_cdma_code_top[(int)con->getCDMA_code()][(int)con->getCDMA_top()]);
	  }
	}
      }
      con = con->next_entry();
    }

  }//end for 


  for(i=0;i<4;++i){
    con = head;
    if(i==0) 	  contype = CONN_BASIC;
    else if(i==1) contype = CONN_PRIMARY;
    else if(i==2) contype = CONN_SECONDARY;
    else contype = CONN_DATA;
    
    while(con!=NULL){
      if(con->get_category() == contype){

        mod_rate = (Ofdm_mod_rate)1;
        num_of_slots = 0;
        allocationsize = 0;
        
	if (con->getCDMA() == 1) {
	  allocationsize = GENERIC_HEADER_SIZE; 					//for explicit polling
	  int tmp_num_poll = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	  num_of_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	  // Commented by Barun : 21-Sep-2011
	  //debug10 ("\tUL.Check1.1.contype(%d), Polling CDMA :%f, numslots :%d, freeslot :%d, CID :%d\n", contype, allocationsize, num_of_slots, freeslots, con->get_cid());
	  con->setCDMA(0);
	}//end getCDMA

	if (freeslots < num_of_slots) num_of_slots = freeslots;

        freeslots -= num_of_slots;

	if(num_of_slots> 0){
//	  temp_index = doesMapExist(con->getPeerNode()->getBasic(IN_CONNECTION)->get_cid(), cid_list, ie_index); //conn, ie list, number of ies
	  temp_index = doesMapExist(con->get_cid(), cid_list, ie_index); //conn, ie list, number of ies
	  if(temp_index < 0){
	    temp_index = ie_index;
//	    cid_list[temp_index] = con->getPeerNode()->getBasic(IN_CONNECTION)->get_cid();
            cid_list[temp_index] = con->get_cid();
	    slots_per_con[temp_index] = num_of_slots;
	    uiuc_list[temp_index] = getUIUCProfile(mod_rate);
            cdma_code_list[temp_index] = con->getCDMA_code();
            cdma_top_list[temp_index] = con->getCDMA_top();
            cdma_flag_list[temp_index] = 1;
	    ++ie_index;
	  }
	  else if(temp_index < MAX_MAP_IE)	 slots_per_con[temp_index] += num_of_slots;
	  else	 				 freeslots += num_of_slots;

        // Commented by Barun : 21-Sep-2011
	  //debug10 ("UL.Check1.2, Polling CDMA (basic/pri/sec): CID(%d), Contype(B:5, P:6, S:7, D:8) :%d, Numslots :%d, Free :%d, allo :%e, StoreSlots[%d] :%d\n", cid_list[temp_index], contype, num_of_slots, freeslots, allocationsize, temp_index, slots_per_con[temp_index]);

	}
      }
      con = con->next_entry();
    }
  }


  //Assign to basic, primary, and secondary now
  for(i=0;i<3;++i){
    con = head;
    if(i==0) 	  contype = CONN_BASIC;
    else if(i==1) contype = CONN_PRIMARY;
    else	  contype = CONN_SECONDARY;
    
    while(con!=NULL){
      if(con->get_category() == contype){

	mod_rate = mac_->getMap()->getUlSubframe()->getProfile(con->getPeerNode()->getDIUC()-DIUC_PROFILE_1+UIUC_PROFILE_1)->getEncoding();
	if (mod_rate > OFDM_16QAM_3_4) {
	  mod_rate = OFDM_16QAM_3_4;
	}

//Chakchai Modified 22 May 2008
        num_of_slots = 0;
        allocationsize = 0;
//
        
	if (con->getBw() > 0 ) {
	  allocationsize = con->getBw();
	  num_of_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));

        // Commented by Barun : 21-Sep-2011
	  //debug10 ("\tBwreq Still>0 :%f, numslots :%d, freeslot :%d\n", allocationsize, num_of_slots, freeslots);
	} else {
	  if (con->getCDMA() == 1) {
	    allocationsize = GENERIC_HEADER_SIZE; 					//for explicit polling
	    int tmp_num_poll = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	    int still_setbw = (int) ceil(con->getBw()/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));

	    if (tmp_num_poll>still_setbw) {
	      num_of_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	      //*debug10 ("\tPolling CDMA :%f, numslots :%d, freeslot :%d\n", allocationsize, num_of_slots, freeslots);
	    } else {
	      //*debug10 ("\tStill set BW Not Polling\n");
	    }
	  }//end getCDMA
	  con->setCDMA(0);
	}

	if (num_of_slots>0) {
	    // Commented by Barun : 21-Sep-2011
	  //debug10 ("UL.Check1.1.contype(%d), allocationsize (con->getBW) :%f, #slots :%d\n", contype, allocationsize, num_of_slots);
	  //debug10 ("\tfree slots :%d, left-over :%d\n", freeslots, freeslots-num_of_slots);
	}

	//*if ( (allocationsize>0) || (num_of_slots>0) ) debug2 ("=> Allocation size %f, num of slots %d\n", allocationsize, num_of_slots);

	if (freeslots < num_of_slots) num_of_slots = freeslots;

        freeslots -= num_of_slots;

#ifdef BWREQ_PATCH
	//decrement bandwidth requested by allocation
	//debug2 ("\tCtrl CID= %d Req= %d Alloc= %d Left=%d\n", 
		//con->get_cid(), con->getBw(), 
		//num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_),
		//con->getBw()-(num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
	con->setBw(con->getBw()-(num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
#endif

	if(num_of_slots> 0){
	  temp_index = doesMapExist(con->getPeerNode()->getBasic(IN_CONNECTION)->get_cid(), cid_list, ie_index); //conn, ie list, number of ies
//          temp_index = doesMapExist(con->get_cid(), cid_list, ie_index); //conn, ie list, number of ies

	  if(temp_index < 0){
	    temp_index = ie_index;
	    cid_list[temp_index] = con->getPeerNode()->getBasic(IN_CONNECTION)->get_cid();
       //     cid_list[temp_index] = con->get_cid();

	    slots_per_con[temp_index] = num_of_slots;
	    uiuc_list[temp_index] = getUIUCProfile(mod_rate);
	    ++ie_index;
	  }
	  else if(temp_index < MAX_MAP_IE)	 slots_per_con[temp_index] += num_of_slots;
	  else	 				 freeslots += num_of_slots;

	  //debug10 ("UL.Check1.2, Init Assign (basic/pri/sec): CID(%d), Contype(B:5, P:6, S:7, D:8) :%d, Numslots :%d, Free :%d, allo :%e, StoreSlots[%d] :%d\n", cid_list[temp_index], contype, num_of_slots, freeslots, allocationsize, temp_index, slots_per_con[temp_index]);

	}
      }
      con = con->next_entry();
    }
  }


  leftOTHER_slots = freeslots;
  int conn_per_schetype[6]={0,0,0,0,0,0};
  conn_per_schetype[0]=0;
  conn_per_schetype[1]=0;
  conn_per_schetype[2]=0;
  conn_per_schetype[3]=0;
  conn_per_schetype[4]=0;
  conn_per_schetype[5]=0;

  int con_ertps_all=0;
  int con_rtps_all=0;
  int con_nrtps_all=0;
  int con_be_all=0;

  for(i=0;i<5;++i){
    con = head;
    if(i==0)            schedtype = SERVICE_UGS;
    else if(i==1)       schedtype = SERVICE_ertPS;
    else if(i==2)       schedtype = SERVICE_rtPS;
    else if(i==3)       schedtype = SERVICE_nrtPS;
    else                schedtype = SERVICE_BE;

    while(con!=NULL){
      if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype){
	mod_rate = mac_->getMap()->getUlSubframe()->getProfile(con->getPeerNode()->getDIUC()-DIUC_PROFILE_1+UIUC_PROFILE_1)->getEncoding();

	if(schedtype==SERVICE_UGS) {
	  conn_per_schetype[0]++;
	}
	else if (schedtype==SERVICE_ertPS) {
	  if (con->getBw() > 0) conn_per_schetype[1]++;
	  con_ertps_all++;

	} else if (schedtype==SERVICE_rtPS) {
	  if (con->getBw() > 0) conn_per_schetype[2]++;
	  con_rtps_all++;
	} else if (schedtype==SERVICE_nrtPS) {
	  if (con->getBw() > 0) conn_per_schetype[3]++;
	  con_nrtps_all++;
	} else if (schedtype==SERVICE_BE) {
	  if (con->getBw() > 0) conn_per_schetype[4]++;
	  con_be_all++;
	} else {
	  conn_per_schetype[5]++;
	}
      }
      con = con->next_entry();
    }
  }

    // Commented by Barun : 21-Sep-2011
  //debug10 ("UL, Active -UGS <%d>, -ertPS <%d>, -rtPS <%d>, -nrtPS <%d>, -BE <%d>, -OTHER <%d>\n", conn_per_schetype[0],conn_per_schetype[1], conn_per_schetype[2], conn_per_schetype[3], conn_per_schetype[4], conn_per_schetype[5] );
  //debug10 ("    All     UGS <%d>,  ertPS <%d>,  rtPS <%d>,  nrtPS <%d>,  BE <%d>\n", conn_per_schetype[0], con_ertps_all, con_rtps_all, con_nrtps_all, con_be_all);



  for(i=0;i<5;++i){
    con = head;
    if(i==0)            schedtype = SERVICE_UGS;
    else if(i==1)       schedtype = SERVICE_ertPS;
    else if(i==2)       schedtype = SERVICE_rtPS;
    else if(i==3)       schedtype = SERVICE_nrtPS;
    else                schedtype = SERVICE_BE;

    
    while(con!=NULL){
      if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype){
	//Richard: we must look up the rate again
	mod_rate = mac_->getMap()->getUlSubframe()->getProfile(con->getPeerNode()->getDIUC()-DIUC_PROFILE_1+UIUC_PROFILE_1)->getEncoding();
	if (mod_rate > OFDM_16QAM_3_4) {
	  mod_rate = OFDM_16QAM_3_4;
	}

        req_slots_tmp1 = 0;
        return_cid_tmp = 0;
        int grant_slots = 0;
        num_of_slots = 0; 

        return_cid_tmp = 0;
        temp_index = doesMapExist(con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), cid_list, ie_index);
        if(temp_index < 0) return_cid_tmp = -1;
        else return_cid_tmp = con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid();

        if(schedtype==SERVICE_UGS){
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
	

#ifdef UGS_AVG
          allocationsize = (int) ceil((double)sfqos->getDataSize()/(double)sfqos->getPeriod());
#endif
#ifndef UGS_AVG
          int tmp_getpoll = con->getPOLL_interval();
          if ( (tmp_getpoll%sfqos->getPeriod())== 0 ) {
                allocationsize = ceil(sfqos->getDataSize());
                con->setPOLL_interval(0);
          } else {
                allocationsize = 0;
 	  }
          tmp_getpoll++;
          con->setPOLL_interval(tmp_getpoll);
#endif
	 
          int arq_enable_f = 0;
	  if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) ) {
	     arq_enable_f = 1;
	  }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                //debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          }

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));

	  num_of_slots = grant_slots;

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("*** Fatal Error: There is not enough resource for UGS ***\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;


	  //debug10 ("UL.Check1.3.UGS, DataSize :%f, period :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getDataSize(), sfqos->getPeriod(), grant_slots, con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
	  //debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofUGS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[0]);

	}

        if(schedtype == SERVICE_ertPS) {

	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
	  req_slots_tmp1 = (int) ceil((double)con->getBw()/(double)mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	  int issue_pol = 0;

/*
	  if (req_slots_tmp1>0) allocationsize = (int) ceil(sfqos->getDataSize()/sfqos->getPeriod());
	  else  allocationsize = GENERIC_HEADER_SIZE; 					//for explicit polling
*/

          int tmp_getpoll = con->getPOLL_interval();
          if (req_slots_tmp1>0) {
                        allocationsize = ceil((double)sfqos->getDataSize()/(double)sfqos->getPeriod());
                        // Commented by Barun : 21-Sep-2011
                        //debug10 ("\tPoll_ertPS: No polling (bw-req>0), lastpoll :%d, period :%d\n", tmp_getpoll, sfqos->getPeriod());
                        con->setPOLL_interval(0);
          } else  {
                if ( (tmp_getpoll%sfqos->getPeriod())== 0 ) {
                        //debug10 ("\tPoll_ertPS(yes): Issues unicast poll, lastpoll :%d, period :%d\n", tmp_getpoll, sfqos->getPeriod());

                        allocationsize = GENERIC_HEADER_SIZE;                                        //for explicit polling
                        con->setPOLL_interval(0);
			issue_pol = 1;
                } else {
                        //debug10 ("\tPoll_ertPS(no): Don't issues unicast poll, lastpoll :%d, period :%d\n", tmp_getpoll, sfqos->getPeriod());
                        allocationsize = 0;
                }
                //debug10 ("\tPoll_ertPS: Current polling_counter :%d, update_polling :%d, period :%d\n", tmp_getpoll, tmp_getpoll+1, sfqos->getPeriod());
                tmp_getpoll++;
                con->setPOLL_interval(tmp_getpoll);
          }

          int arq_enable_f = 0;
          if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) && (issue_pol==0) ) {
             arq_enable_f = 1;
          }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                //debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          }

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	  num_of_slots = grant_slots;

#ifdef BWREQ_PATCH
	  //decrement bandwidth requested by allocation	  
	  //debug2 ("\tertPS: CID= %d Req= %d Alloc= %d Left=%d\n", 
		  //con->get_cid(), con->getBw(), 
		  //num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_),
		  //con->getBw()-(num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
	  con->setBw(con->getBw()-(num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
#endif

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("*** Fatal Error: There is not enough resource for ertPS ***\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;


        // Commented by Barun : 21-Sep-2011
	  //debug10 ("UL.Check1.3.ertPS, DataSize :%f, period :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d\n", sfqos->getDataSize(), sfqos->getPeriod(), grant_slots, con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC());
	  //debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofertPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[1]);

	}
	
        if(schedtype == SERVICE_rtPS) {
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
	  req_slots_tmp1 = (int) ceil((double)con->getBw()/(double)mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	  int issue_pol = 0;

/*
	  if (req_slots_tmp1>0) allocationsize = (int) ceil((sfqos->getMinReservedRate()*FRAME_SIZE)/8);
	  else  allocationsize = GENERIC_HEADER_SIZE; 					//for explicit polling
*/

          int tmp_getpoll = con->getPOLL_interval();
          if (req_slots_tmp1>0) {
                        allocationsize = ceil((sfqos->getMinReservedRate()*FRAME_SIZE)/8);
                        //debug10 ("\tPoll_rtPS: No polling (bw-req>0), lastpoll :%d, period :%d\n", tmp_getpoll, sfqos->getPeriod());
                        con->setPOLL_interval(0);
          } else  {
                if ( (tmp_getpoll % sfqos->getPeriod())== 0 ) {
                        //debug10 ("\tPoll_rtPS(yes): Issues unicast poll, lastpoll :%d, period :%d\n", tmp_getpoll, sfqos->getPeriod());

                        allocationsize = GENERIC_HEADER_SIZE;                                        //for explicit polling
                        con->setPOLL_interval(0);
			issue_pol = 1;
                } else {
                        //debug10 ("\tPoll_rtPS(no): Don't issue unicast poll, lastpoll :%d, period :%d\n", tmp_getpoll, sfqos->getPeriod());
                        allocationsize = 0;
                }
                //debug10 ("\tPoll_rtPS: Current polling_counter :%d, update_polling :%d, period :%d\n", tmp_getpoll, tmp_getpoll+1, sfqos->getPeriod());
                tmp_getpoll++;
                con->setPOLL_interval(tmp_getpoll);
          }

          int arq_enable_f = 0;
          if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) && (issue_pol==0) ) {
             arq_enable_f = 1;
          }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                //debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub
          }

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	  num_of_slots = grant_slots;

#ifdef BWREQ_PATCH
	  //decrement bandwidth requested by allocation
	  //debug2 ("\trtPS: CID= %d Req= %d Alloc= %d Left=%d\n", 
		  //con->get_cid(), con->getBw(), 
		  //num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_),
		  //con->getBw()-(num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
	  con->setBw(con->getBw()-(num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
#endif
	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("*** Fatal Error: There is not enough resource for rtPS\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;

	  if (grant_slots<req_slots_tmp1) needmore_con[0]++;

	  //debug10 ("UL.Check1.3.rtPS, MinReservedRate :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d, bw_req_header :%d, getBw :%d\n", sfqos->getMinReservedRate(), grant_slots, con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC(), GENERIC_HEADER_SIZE, con->getBw());
	  //debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofrtPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[2]);
	
	}


        if(schedtype == SERVICE_nrtPS) {
	  ServiceFlow *sf = con->get_serviceflow();
	  ServiceFlowQoS *sfqos = sf->getQoS();
	  int issue_pol = 0;
	  req_slots_tmp1 = (int) ceil((double)con->getBw()/(double)mac_->getPhy()->getSlotCapacity(mod_rate, UL_));

	  if (req_slots_tmp1>0) {
	    allocationsize = (int) ceil((sfqos->getMinReservedRate()*FRAME_SIZE)/8);
	  } else  {
	    if (con->getCDMA() == 1) {
	      if (GENERIC_HEADER_SIZE > con->getBw()) {
		allocationsize = GENERIC_HEADER_SIZE; 					//for explicit polling
		issue_pol = 1;
	      } 
	      con->setCDMA(0);
	    } else {
	      allocationsize = 0;
	    }
	  }

          int arq_enable_f = 0;
          if ((con->getArqStatus () != NULL) && (con->getArqStatus ()->isArqEnabled() == 1) && (issue_pol == 0)) {
             arq_enable_f = 1;
          }
          int t_bytes = 0;
          double ori_grant = allocationsize;
          if ( (arq_enable_f == 1) && (allocationsize > 0) ) {
                t_bytes = (int)allocationsize % (int)getMac()->arq_block_size_;
                if (t_bytes > 0) {
                        allocationsize = allocationsize + (getMac()->arq_block_size_ - t_bytes);
                }
                //debug10 (" ARQ enable CID :%d, arq_block :%d, requested_size :%f, arq_block_boundary_size :%f, all_header_included :%f\n", con->get_cid(), getMac()->arq_block_size_, ori_grant, allocationsize, allocationsize + (double)14);
                allocationsize = allocationsize + 14;   //optional => MAC+frag_sub+pack_sub+gm_sub+mesh_sub

          }

	  grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	  num_of_slots = grant_slots;

#ifdef BWREQ_PATCH
	  //decrement bandwidth requested by allocation
	  //debug2 ("UL.Check1.3.nrtPS: CID= %d Req= %d Alloc= %d Left=%d\n", 
		  //con->get_cid(), con->getBw(), 
		  //num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_),
		  //con->getBw()-(num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
	  con->setBw(con->getBw()-(num_of_slots*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
#endif

	  if (leftOTHER_slots<=num_of_slots) {
	    debug10 ("*** Fatal Error: There is not enough resource for nrtPS ***\n");
	    exit(1);
	  } else leftOTHER_slots=leftOTHER_slots-num_of_slots;

	  if (grant_slots<req_slots_tmp1) needmore_con[1]++;

	  //debug10 ("UL.Check1.3.nrtPS, MinReservedRate :%d, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d, getBw :%d\n", sfqos->getMinReservedRate(), grant_slots, con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC(), con->getBw());
	  //debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofnrtPS :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[3]);

	}

	if(schedtype==SERVICE_BE){
	  req_slots_tmp1 = (int) ceil((double)con->getBw()/(double)mac_->getPhy()->getSlotCapacity(mod_rate, UL_));

	  if (req_slots_tmp1>0) needmore_con[2]++;

	  //may effect QoS for other classes => may need more sophicicate scheduler
  	  if (con!=NULL && con->getCDMA() == 1) {
	    if (GENERIC_HEADER_SIZE > con->getBw()) {
	      allocationsize = GENERIC_HEADER_SIZE; 					//for explicit polling
	      grant_slots = (int) ceil(allocationsize/mac_->getPhy()->getSlotCapacity(mod_rate, UL_));
	      num_of_slots = grant_slots;
	    }
	    con->setCDMA(0);

	    if (leftOTHER_slots<=num_of_slots) {
	      debug10 ("*** Fatal Error: There is not enough resource for BE polling ***\n");
	      // exit(1);
	      num_of_slots = leftOTHER_slots;
	    } else leftOTHER_slots=leftOTHER_slots-num_of_slots;

	    //debug10 ("UL.Check1.3.BE polling, PRE-GRANT-SLOTS :%d, Peer-CID :%d, returnCID :%d, DIUC :%d, bw_req_header :%d, setCDMA :%d\n", grant_slots, con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), return_cid_tmp, con->getPeerNode()->getDIUC(), GENERIC_HEADER_SIZE, con->getCDMA());
	    //debug10 ("\tAllocatedSLots :%d, BeforeFree :%d, LeftforOTHER :%d, NumofBE :%d\n", num_of_slots, freeslots, leftOTHER_slots, conn_per_schetype[4]);

	  } 

	}//end BE

 	freeslots = leftOTHER_slots;

        if(num_of_slots> 0) {
	  temp_index = doesMapExist(con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), cid_list, ie_index);
//          temp_index = doesMapExist(con->get_cid(), cid_list, ie_index); //conn, ie list, number of ies

	  if(temp_index < 0){
	    temp_index = ie_index;
//	    cid_list[temp_index] = con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid();
            cid_list[temp_index] = con->get_cid();

	    slots_per_con[temp_index] = num_of_slots;
	    uiuc_list[temp_index] = getUIUCProfile(mod_rate);
	    ++ie_index;

	    //debug10 ("UL_Check1.3: MapNotExist New_CID :%d, return index :-1, cid_list[%d] :%d,  #entry (ie_index)++ :%d\n", con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), temp_index, cid_list[temp_index], ie_index);
	    //debug10 ("\tNumSlots1.2 :%d\n", num_of_slots);

	  } else if(temp_index<MAX_MAP_IE) {
	    slots_per_con[temp_index] += num_of_slots;

	    //debug10 ("UL.Check1.3: MapExist Peer_CID :%d, return index :%d, cid_list[%d] :%d,  #entry (ie_index) :%d\n", con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), temp_index, temp_index, cid_list[temp_index], ie_index);
	    //debug10 ("\tNumSlots1.2 :%d\n", num_of_slots);

	  } else {
	    freeslots += num_of_slots; 			//return back the slots
	    leftOTHER_slots += num_of_slots; 		//return back the slots
	  }

	  //debug10 ("UL.Check1.3, First Assign (ugs/ertps/rtps/nrtps/no be): CID(%d), Schetype :%d, Numslots :%d, Freeleft :%d, StoreSlots[%d] :%d, mod_rate :%d, UIUC :%d\n", cid_list[temp_index], schedtype, num_of_slots, freeslots, temp_index, slots_per_con[temp_index], mod_rate, uiuc_list[temp_index]);
        }



      }
      con = con->next_entry();
    }//end con != NULL
  }//end for loop 5 Qos


  //Allocate left-over to rtPS, nrtPS, and BE fairly
  int share_next_slots = 0;
  for (int i=0;i<3;i++) needmore_c = needmore_c+needmore_con[i];
  if (needmore_c>0) share_next_slots = (int) floor(leftOTHER_slots/needmore_c);

  int first_assign = 0;

  while (needmore_c>0 && freeslots>0) {

    share_next_slots = (int) floor(freeslots/needmore_c);

    //debug10 ("UL.Check1.4, (Check still need more here): Needmore Conn :%d, Free :%d, Sharenext :%d\n", needmore_c, freeslots, share_next_slots);

    for(i=0;i<5;++i){
      con = head;
      if(i==0)        schedtype = SERVICE_UGS;
      else if(i==1)   schedtype = SERVICE_ertPS;
      else if(i==2)   schedtype = SERVICE_rtPS;
      else if(i==3)   schedtype = SERVICE_nrtPS;
      else            schedtype = SERVICE_BE;
    
      if ( (i==0) || (i==1) ) continue;

      first_assign = 0;
      while(con!=NULL) {

	if(con->get_category() == CONN_DATA && con->get_serviceflow()->getScheduling() == schedtype) {

	  mod_rate = mac_->getMap()->getUlSubframe()->getProfile(con->getPeerNode()->getDIUC()-DIUC_PROFILE_1+UIUC_PROFILE_1)->getEncoding();
	  if (mod_rate > OFDM_16QAM_3_4) {
	    mod_rate = OFDM_16QAM_3_4;
	  }

	  temp_index = doesMapExist(con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), cid_list, ie_index);
//          temp_index = doesMapExist(con->get_cid(), cid_list, ie_index); //conn, ie list, number of ies

	  if(temp_index < 0) return_cid_tmp = -1;
	  else return_cid_tmp = temp_index;

	  int req_slots = 0; 
	  req_slots = (int) ceil((double)con->getBw()/(double)mac_->getPhy()->getSlotCapacity(mod_rate, UL_)); 

	  if ( (schedtype==SERVICE_rtPS) || (schedtype==SERVICE_nrtPS) ) {
	    if (return_cid_tmp == -1) {
	      //debug10 ("UL.Check1.4.rtps/nrtps, No_CID(%d), n_Conn :%d, Free :%d\n", return_cid_tmp, needmore_c, freeslots);
	      con = con->next_entry();
	      continue;
	    }
	    if ( req_slots<=slots_per_con[return_cid_tmp] ) {
	      //debug10 ("UL.Check1.4.rtPS/nrtPS, CID(%d), <=MinSlots: n_Conn :%d, REQ-SLOTS :%d, GRANT-1SLOTS :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, req_slots, slots_per_con[return_cid_tmp], freeslots);
	      con = con->next_entry();
	      continue;
	    }
	  }

	  int t_num_of_slots = 0;

	  if ( (schedtype==SERVICE_rtPS) || (schedtype==SERVICE_nrtPS) ) {
	    first_assign = slots_per_con[return_cid_tmp];

	    if (req_slots <= (first_assign + share_next_slots) ) {           
	      t_num_of_slots = req_slots;
	      slots_per_con[return_cid_tmp] = t_num_of_slots;
	      needmore_c--;
	      freeslots = freeslots - (req_slots-first_assign);
	      //debug10 ("UL.Check1.4.rtps/nrtps, CID(%d), rtps/nrtpsNoNeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, t_num_of_slots, freeslots);
	    } else {
	      if (share_next_slots==0) {
		if (freeslots>0) { 
		  t_num_of_slots = (first_assign + 1);
		  freeslots = freeslots - 1;
		} else t_num_of_slots = first_assign;
	      } else {
		t_num_of_slots =  (first_assign + share_next_slots);            
		freeslots = freeslots - share_next_slots;
	      }
	      slots_per_con[return_cid_tmp] = t_num_of_slots;
	    }
	    //debug10 ("UL.Check1.4.rtps/nrtps, CID(%d), rtps/nrtpsNeedMore: n_Conn :%d, Previous-Slots :%d, Grant-Slots :%d, Free :%d\n", cid_list[return_cid_tmp], needmore_c, first_assign, t_num_of_slots, freeslots);
	  } else { //BE
	    if(req_slots<1) {
	      con = con->next_entry();
	      continue;
	    }
	    temp_index = doesMapExist(con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), cid_list, ie_index);
//            temp_index = doesMapExist(con->get_cid(), cid_list, ie_index); //conn, ie list, number of ies

	    if(temp_index < 0){
	      temp_index = ie_index;
	      cid_list[temp_index] = con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid();
//              cid_list[temp_index] = con->get_cid();

	      slots_per_con[temp_index] = 0;
	      uiuc_list[temp_index] = getUIUCProfile(mod_rate);
	      ++ie_index;
	      //debug10 ("UL.Check1.4.BE, CID(%d), Initial_BE: n_Conn :%d, REQ-SLOTS :%d, Free :%d, getBW :%d\n", cid_list[temp_index], needmore_c, req_slots, freeslots, con->getBw());

	    } else {
	    }

	    first_assign = slots_per_con[temp_index];
	    int bw_assigned = 0; //amount we are assigning in the round
#ifdef BWREQ_PATCH
	    //req_slots is updated each round since we update remain Bw to allocate
	    if (req_slots <= share_next_slots ) {
	      bw_assigned = req_slots;
	      t_num_of_slots = req_slots;
	      slots_per_con[temp_index] += req_slots;
	      needmore_c--;
	      freeslots = freeslots - req_slots;
#else
	    if (req_slots <= (first_assign + share_next_slots) ) {
	      bw_assigned = req_slots;
	      t_num_of_slots = req_slots;
	      slots_per_con[temp_index] = t_num_of_slots;
	      needmore_c--;
	      freeslots = freeslots - (req_slots-first_assign);
#endif

	      //debug10 ("UL.Check1.4.BE, Peer-CID(%d), returnCID(%d), BENoNeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), cid_list[temp_index], needmore_c, t_num_of_slots, freeslots);
	    } else {
	      if (share_next_slots==0) {
		//there is less slot than connections left..assign slot 1 by 1.
		if (freeslots>0) { 
		  bw_assigned = 1;
		  t_num_of_slots = (first_assign + 1); //assign one more slot
		  freeslots = freeslots - 1;
		} else t_num_of_slots = first_assign;
	      } else {
		bw_assigned = share_next_slots;
		t_num_of_slots =  (first_assign + share_next_slots); //assign its share of slots
		freeslots = freeslots - share_next_slots;
	      }
	      slots_per_con[temp_index] = t_num_of_slots;

	      //debug10 ("UL.Check1.4.BE, Peer-CID(%d), returnCID(%d), BENeedMore: n_Conn :%d, Pre-Grant-Slots :%d, Free :%d\n", con->getPeerNode()->getBasic(OUT_CONNECTION)->get_cid(), cid_list[temp_index], needmore_c, t_num_of_slots, freeslots);
	    }

#ifdef BWREQ_PATCH
	    //update Bw requested left after allocation
	    //debug2 ("\tBE: CID= %d Req= %d Slot cap=%d slots= %d Alloc= %d Left=%d\n", 
		    //con->get_cid(), con->getBw(), mac_->getPhy()->getSlotCapacity(mod_rate, UL_), req_slots,
		    //bw_assigned*mac_->getPhy()->getSlotCapacity(mod_rate, UL_),
		    //con->getBw()-(bw_assigned*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));
	    con->setBw(con->getBw()-(bw_assigned*mac_->getPhy()->getSlotCapacity(mod_rate, UL_)));	    
#endif

	  }	//else BE

	}//else CONNDATA
	con = con->next_entry();
      }//while
    }//for

  }//while

  if(stripping == HORIZONTAL_STRIPPING){
    for(i=0;i<ie_index;++i){
      ies[i].cid = cid_list[i];
      num_of_slots = slots_per_con[i];
      ies[i].uiuc = uiuc_list[i];
      num_of_subchannels = num_of_slots;
      
      ies[i].subchannel_offset = subchannel_offset + subchannel_start;
      ies[i].symbol_offset = symbol_offset + symbol_start;
      ies[i].num_of_subchannels = num_of_subchannels;
      if (total_symbols>0) num_of_symbols = (int) ceil( (double)(symbol_offset + num_of_slots) / (total_symbols) );
      else num_of_symbols = 0;
      ies[i].num_of_symbols = num_of_symbols;

/*
      num_of_symbols = num_of_slots * 3;
      num_of_subchannels = (int) ceil((double)(symbol_offset + num_of_symbols)/total_symbols);
      ies[i].num_of_symbols = num_of_symbols;
      ies[i].num_of_subchannels = num_of_subchannels;
*/

      if (cdma_flag_list[i]>0) {
         ies[i].cdma_ie.subchannel = cdma_top_list[i];
         ies[i].cdma_ie.code = cdma_code_list[i];
      } else {
         ies[i].cdma_ie.subchannel = 0;
         ies[i].cdma_ie.code = 0;
      }

      //debug10 ("UL.Check1.5: ie_index(MAX=60) :%d, ies[%d].cid :%d, #Slots :%d, UIUC :%d\n", ie_index, i, ies[i].cid, num_of_slots, ies[i].uiuc);
      //debug10 ("\tIE_Subchannel_offset (sub_offset<%d>+sub_start<%d>) :%d, #IE_Subchannel :%d\n", subchannel_offset, subchannel_start, ies[i].subchannel_offset, ies[i].num_of_subchannels);
      //debug10 ("\tIE_Symbol_offset (sym_offset<%d>+sym_start<%d>) :%d, #Symbols [ceil((sub_offset<%d>+#Subchannel<%d>)/total_sub<%d>)*3] :%d\n", symbol_offset, symbol_start, ies[i].symbol_offset, subchannel_offset, num_of_subchannels, total_subchannels, ies[i].num_of_symbols);

      subchannel_offset += num_of_symbols;
      if (total_symbols>0) symbol_offset = (symbol_offset + num_of_slots) % (total_symbols);
      else symbol_offset = 0;

/*
      subchannel_offset += num_of_subchannels - 1;
      symbol_offset = (symbol_offset + num_of_symbols)%(total_symbols);
*/
    }
  }

  if(stripping == VERTICAL_STRIPPING){
    for(i=0;i<ie_index;++i){
      ies[i].cid = cid_list[i];
      num_of_slots = slots_per_con[i];
      ies[i].uiuc = uiuc_list[i];
      ies[i].subchannel_offset = subchannel_offset + subchannel_start;
      ies[i].symbol_offset = symbol_offset + symbol_start;
      num_of_subchannels = num_of_slots;
      num_of_symbols = (int) ceil((double)(subchannel_offset + num_of_subchannels)/total_subchannels)*3;
      ies[i].num_of_symbols = num_of_symbols;
      ies[i].num_of_subchannels = num_of_subchannels;

      if (cdma_flag_list[i]>0) {
         ies[i].cdma_ie.subchannel = cdma_top_list[i];
         ies[i].cdma_ie.code = cdma_code_list[i];
      } else {
         ies[i].cdma_ie.subchannel = 0;
         ies[i].cdma_ie.code = 0;
      }

      //debug10 ("UL.Check1.5: ie_index(MAX=60) :%d, ies[%d].cid :%d, #Slots :%d, UIUC :%d\n", ie_index, i, ies[i].cid, num_of_slots, ies[i].uiuc);
      //debug10 ("\tIE_Subchannel_offset (sub_offset<%d>+sub_start<%d>) :%d, #IE_Subchannel :%d\n", subchannel_offset, subchannel_start, ies[i].subchannel_offset, ies[i].num_of_subchannels);
      //debug10 ("\tIE_Symbol_offset (sym_offset<%d>+sym_start<%d>) :%d, #Symbols [ceil((sub_offset<%d>+#Subchannel<%d>)/total_sub<%d>)*3] :%d\n", symbol_offset, symbol_start, ies[i].symbol_offset, subchannel_offset, num_of_subchannels, total_subchannels, ies[i].num_of_symbols);

      subchannel_offset = (subchannel_offset + num_of_subchannels)%(total_subchannels);
      symbol_offset += num_of_symbols - 3;

    }
  }

  ul_map->nb_ies = ie_index;
  return ul_map;
}


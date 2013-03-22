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

#ifndef BSSCHEDULER_H
#define BSSCHEDULER_H

#include "wimaxscheduler.h"
#include "scanningstation.h"

#define INIT_DL_DURATION 20 //enough for DL_MAP, UL_MAP, DCD, UCD and some RNG-RSP
#define MIN_CONTENTION_SIZE 5 //minimum number of opportunity for allocation

//#define DEFAULT_DL_RATIO 0.3 //default ratio for downlink subframe

#define NUM_REALIZATIONS1 2000 
#define OTHER_NUM_DL_BURST 2 //including 1st DL burst and END_OF_MAP burst.
#define OTHER_NUM_UL_BURST 3 //including initial ranging, bw req and END_OF_MAP burst

// This structure is used for deficit round robin (next version)
struct con_data_alloc {
  int cid;
  int direction;
  Ofdm_mod_rate mod_rate;
  int req_slots;
  int req_bytes;
  int grant_slots;
  int grant_bytes;
  double weight;
  int counter;
};

struct con_drr {
  int cid;
  int quantum;
  int counter;
};


class Mac802_16BS;
class WimaxCtrlAgent;
/**
 * Class BSScheduler
 * Implement the packet scheduler on the BS side
 */ 
class BSScheduler : public WimaxScheduler
{

	
	
  //friend class SendTimer;
 public:
  /*
   * Create a scheduler
   */
  BSScheduler ();

//sam

  int Repetition_code_;


 int channel_gain[NUM_REALIZATIONS1] ;

//sam

  /*
   * Interface with the TCL script
   * @param argc The number of parameter
   * @param argv The list of parameters
   */
  int command(int argc, const char*const* argv);

  /**
   * Initializes the scheduler
   */
  virtual void init ();
 
  /**
   * This function is used to schedule bursts/packets
   */
  virtual void schedule ();

 protected:

  /**
   * Default modulation 
   */
  Ofdm_mod_rate default_mod_;

  /**
   * Number of transmission opportunity for initial ranging
   * and bw request (i.e contention slots)
   */
  int contention_size_; 
  int init_contention_size_; 
  int bw_req_contention_size_; 
  /**
   * Compute and return the bandwidth request opportunity size
   * @return The bandwidth request opportunity size
   */
  int getBWopportunity ();

  /**
   * Compute and return the initial ranging opportunity size
   * @return The initial ranging opportunity size
   */
  int getInitRangingopportunity ();  

  /**
   * Add a downlink burst with the given information
   * @param burstid The burst number
   * @param c The connection to add
   * @param iuc The profile to use
   * @param dlduration current allocation status
   * @param the new allocation status
   */
  int addDlBurst (int burstid, Connection * , int iuc, int dlduration, int maxdlduration);

  /**
   * Given number of bursts, calculate total number of occupied slots in downlink subframe
   * @return total number of occupied slots
   */
  int check_overallocation (int num_of_entries);

  /**
   * Check if there is a existing ie_map for the particular connection
   * @return connection id or -1 if not exist
   */
  int doesvirtual_allocexist(int num_of_entries, int cid);

  /**
   * Check number of available slots given additional slots for the particular allocation 
   * @return available slots (= requested slots if there is enough slots in downlink subframe)
   */
  int overallocation_withoutdlmap(int num_of_entries, int totalslots, int ownslots);

  /**
   * Check number of available slots given additional slots for the particular allocation with increase of dl_map_ie
   * @return available slots (= requested slots if there is enough slots in downlink subframe)
   */
  int overallocation_withdlmap(int num_of_entries, int totalslots, int ownslots);

  /**
   * Add granted slots to downlink subframe given cid
   * @return positive number if granted slots are added else 0
   */
  int addslots_withoutdlmap(int number_of_entries, int byte, int slots, int cid);

  /**
   * Find out the maximum burst the downlink subframe can support (the increase of dl_map_ie)
   * @return total number of available dl_map_ie
   */
  int max_conn_withdlmap(int num_of_entries, int totalslots);

  /**
   * Bubble Sort (sort_field = 0 or 1 low to high or high to low)
   */
  void bubble_sort (int arrayLength, con_data_alloc array[], int sort_field);

  /**
   * Compute number of available slots with the increase of #conn*dl_map_ie size
   * @return total number of availableo slots
   */
  int freeslots_withdlmap_given_conn(int num_of_entries, int totalslots, int newconn);

  /**
   * Add dl_map_ie size to dl_map
   * @return total number of occupied slots including new dl_map_ie
   */
  int increase_dl_map_ie(int num_of_entries, int totalslots, int num_ie);


//added rpi for OFDMA -------  
/**
 * Add a downlink burst with the given information
 * @param burstid The burst number
 * @param c The connection to add
 * @param iuc The profile to use
 * @param ofdmsymboloffset 
 * @param numofdmsymbol 
 * @param subchanneloffset
 * @param numsubchannels
 */
void addDlBurst (int burstid, int cid, int iuc, int ofdmsymboloffset, int numofdmsymbols, int subchanneloffset, int numsubchannels);

/* returns the DIUC profile associated with a current modulation and coding scheme */
diuc_t getDIUCProfile(Ofdm_mod_rate rate);

uiuc_t getUIUCProfile(Ofdm_mod_rate rate);

//ritun

//mac802_16_dl_map_frame * dl_stage2(Connection *head, int total_subchannels, int total_symbols, int symbol_start, int stripping);
 mac802_16_dl_map_frame * dl_stage2(Connection *head, int input_subchannel_offset,  int total_subchannels, int total_symbols, int symbol_start, int  stripping, int total_dl_slots_pusc);
// mac802_16_dl_map_frame * dl_stage2(Connection *head, int input_subchannel_offset,  int total_subchannels, int total_symbols, int symbol_start, int  stripping);

 mac802_16_ul_map_frame * ul_stage2(Connection *head, int total_subchannels, int total_symbols, int symbol_start, int stripping);

 int doesMapExist(int, int*, int);

//ritun

//int  GetInitialChannel();

// added rpi for ofdma - end


/**
 * get the dlmap. 
 * @param out connection list , number of ofdm symbols tht can be allocated, permutation scheme, CQI , ofdm symbol offset
 */

//mac802_16_dl_map_frame GetDLMap(mac_->getCManager()->get_out_connection (), int numofdmsymbols,int numsubchannels,Permutationscheme_, int CQI, int symboloffset);

/**
 * get the ulmap. 
 * @param in connection list , number of ofdm symbols tht can be allocated, permutation scheme, CQI , ofdm symbol offset
 */

//mac802_16_ul_map_frame GetULMap(mac_->getCManager()->get_in_connection (), int numofdmsymbols,int numsubchannels,Permutationscheme_, int CQI, int symboloffset);

// added rpi for ofdma 

 private:

  /**
   * Return the MAC casted to BSScheduler
   * @return The MAC casted to BSScheduler
   */
  Mac802_16BS* getMac();
   
  /**
   * The ratio for downlink subframe
   */
//  double dlratio_;   rpi removed , moved to wimax scheudler

  /**
   * The address of the next node for DL allocation
   */
  int nextDL_;
  
  /**
   * The address of the next node for UL allocation
   */
  int nextUL_;




};

#endif //BSSCHEDULER_H


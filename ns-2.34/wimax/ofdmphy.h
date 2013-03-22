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

#ifndef OFDMPHY_H
#define OFDMPHY_H

#include "wireless-phy.h"
#include "packet.h"


#define OFDM_PRECISION 1000000000 //Used to round up values (to ns)

/** Definition of supported permutation schemes-- added for OFDMA  */
enum Permutation_scheme {
  PUSC = 0,  /* Partial usage of Subcarriers */
  FUSC = 1,  /* Fully usage of Subcarriers */
  AMC = 2,   /* Contiguous Subcarriers */
  OPUSC = 3, /* Optional PUSC, uplink only */
  OFUSC = 4,  /* Optional FUSC, downlink only */
  PERM_LAST
};

/*
 * Definitions for physical layers from:
 * ftp://download.intel.com/technology/itj/2004/volume08issue03/art03_scalableofdma/vol8_art03.pdf
 */

/* Store information about different combinations 
 * of bandwidth and permutation
 * - Permutation
 * - Channel bandwidth 
 * - FFT size
 * - Number of used subcarriers
 * - Number of subchannels
 * - Number of subcarrier per subchannel per symbol (Average)
 * - Number of data subcarrier per subchannel per symbol (Average)
 * - Data slot width (subchannel)
 * - Data slot length (OFDM symbol)
 */
#define NB_OFDMA_PHY_PARAM 9
enum Ofdma_index_t {
  OFDMA_INDEX_PERM = 0,
  OFDMA_INDEX_BW,
  OFDMA_INDEX_FFT,
  OFDMA_INDEX_NUSED,
  OFDMA_INDEX_NBSUBCH,
  OFDMA_INDEX_NBSUBCAR,
  OFDMA_INDEX_NBDATASUBCAR,
  OFDMA_INDEX_SLOT_WIDTH,
  OFDMA_INDEX_SLOT_LENGTH
};

//Note: Currently comment 20MHz data since the propagation model
//      does not support it.
static const unsigned long DL_OFDMA_DATA [][NB_OFDMA_PHY_PARAM] = 
  {{PUSC, 5000000, 512, 421, 15, 28, 24, 1, 2},
   {PUSC, 10000000, 1024, 841, 30, 28, 24, 1, 2},
   /*{PUSC, 20000000, 2048, 1681, 60, 28, 24, 1, 2},*/
   {FUSC, 5000000, 512, 426, 8, 48, 48, 1, 1},
   {FUSC, 10000000, 1024, 851, 16, 48, 48, 1, 1},
   /*{FUSC, 20000000, 2048, 1703, 32, 48, 48, 1, 1},*/
   {OFUSC, 5000000, 512, 433, 8, 48, 48, 1, 1},
   {OFUSC, 10000000, 1024, 865, 16, 48, 48, 1, 1},
   /*{OFUSC, 20000000, 2048, 1729, 32, 48, 48, 1, 1},*/
   {AMC, 5000000, 512, 433, 8, 48, 48, 1, 1},
   {AMC, 10000000, 1024, 865, 16, 48, 48, 1, 1}
   /*{AMC, 20000000, 2048, 1729, 32, 48, 48, 1, 1}*/
  };

/*
 * Note for PUSC: The slot allocation is 1 Subchannel x 3 OFDM symbols
 *  Each subchannel is 6 tiles wide and each tile has 4 pilot subcarrier 
 *  and 8 data subcarrier. Therefore each slot has 24 pilot and 48 data 
 *  over 3 OFDM symbols. This makes an average of 8 pilot subcarrier and 
 *  16 data subcarrier per symbol (24 total). 
 * Note for OPUSC: The slot allocation is 1 Subchannel x 3 OFDM symbols
 *  Each subchannel is 6 tiles wide and each tile has 1 pilot subcarrier 
 *  and 8 data subcarrier. Therefore each slot has 6 pilot and 48 data 
 *  over 3 OFDM symbols. This makes an average of 2 pilot subcarrier and 
 *  16 data subcarrier per symbol (18 total). 
 */
//Note: Currently comment 20MHz data since the propagation model
//      does not support it.
static const unsigned long UL_OFDMA_DATA [][NB_OFDMA_PHY_PARAM] = 
  {{PUSC, 5000000, 512, 409, 17, 24, 16, 1, 3},
   {PUSC, 10000000, 1024, 841, 35, 24, 16, 1, 3},
   /*{PUSC, 20000000, 2048, 1681, 92, 24, 16, 1, 3},*/
   {OPUSC, 5000000, 512, 433, 24, 18, 16, 1, 3},
   {OPUSC, 10000000, 1024, 865, 48, 18, 16, 1, 3},
   /*{OPUSC, 20000000, 2048, 1729, 96, 18, 16, 1, 3},*/
   {AMC, 5000000, 512, 433, 8, 48, 48, 1, 1},
   {AMC, 10000000, 1024, 865, 16, 48, 48, 1, 1}
   /*{AMC, 20000000, 2048, 1729, 32, 48, 48, 1, 1}*/
  };

/** Definition of supported rate */
enum Ofdm_mod_rate {
  OFDM_BPSK_1_2,   /* Efficiency is 1 bps/Hz */
  OFDM_QPSK_1_2,   /* Efficiency is 2 bps/Hz */
  OFDM_QPSK_3_4,   /* Efficiency is 2 bps/Hz */
  OFDM_16QAM_1_2,  /* Efficiency is 4 bps/Hz */
  OFDM_16QAM_3_4,  /* Efficiency is 4 bps/Hz */
  OFDM_64QAM_2_3,  /* Efficiency is 6 bps/Hz */
  OFDM_64QAM_3_4   /* Efficiency is 6 bps/Hz */
};

/** used as a parameter for the direction of traffic-- added for OFDMA */
enum direction {
  DL_= 0,
  UL_= 1,
};    

/** Status of physical layer */
enum Ofdm_phy_state {
  OFDM_IDLE,  /* Module is not doing anything */
  OFDM_SEND,  /* Module is ready to send or sending */
  OFDM_RECV,  /* Module is can receive or is receiving */
  OFDM_RX2TX, /* Module is transitioning from receiving mode to sending mode */
  OFDM_TX2RX  /* Module is transitioning from sending mode to receiving mode */
};

/** 
 * Class OFDMPhy
 * Physical layer implementing OFDM
 */ 
class OFDMAPhy : public WirelessPhy {

 public:
  OFDMAPhy();

  /**
   * Change the frequency at which the phy is operating
   * @param freq The new frequency
   */
  void setFrequency (double freq);

  /**
   * Set the new modulation for the physical layer
   * @param modulation The new physical modulation
   */
  void  setModulation (Ofdm_mod_rate modulation);    
  
  /**
   * Return the current modulation
   */
  Ofdm_mod_rate  getModulation ();

  /** 
   * Set the permutation scheme for the physical layer
   * @param perm. The new permutation. 
   * @param dir The direction in which the permutation applies
   */    
  void setPermutationscheme (Permutation_scheme perm, direction dir) ;

  /** 
   * Return the permutation scheme 
   * @param dir The direction 
   */    
  Permutation_scheme getPermutationscheme (direction dir) ;

  /**
   * Set the new transmitting power
   * @param power The new transmitting power
   */

  void  setTxPower (double power);
  
  /**
   * Return the current transmitting power
   */
  double  getTxPower ();
  
  /**
   * calculate the FFT size (same of UL and DL)
   * @return the FFT size
   */
  int getFFT ();

  /** 
   * compute the number of subchannels in the given direction 
   * @param dir The direction
   * @return The number of subchannels in the given direction 
   */    
  int getNumsubchannels (direction dir) ;

  /**
   * Get the number of subcarrier per subchannel per symbol
   * @param dir The direction
   * @return The number of subcarrier per subchannel per symbol
   */
  int getNumSubcarrier (direction dir);

  /**
   * Get the number of data subcarrier per subchannel per symbol
   * @param dir The direction
   * @return The number of data subcarrier per subchannel per symbol
   */
  int getNumDataSubcarrier (direction dir);

  /**
   * Get the duration of a slot 
   * @param dir The direction
   * @return The duration of a slot
   */
  int getSlotLength (direction dir);

  /**
   * Get the number of subcarriers used
   * @param dir The direction
   * @return The duration of a slot
   */
  int getNUsed (direction dir);

  /**
   * Compute the transmission time for a packet of size sdusize, num subchannels allocated per OFDM symbol, direction, permuation scheme 
   * using the given modulation
   */
  double getTrxTime (int, Ofdm_mod_rate, int, direction);

  /**
   * Compute the transmission time using OFDM symbol as
   * minimum allocation for a packet of size sdusize and
   * using the given modulation, num subchannels, direction, scheme)
   */
  double getTrxSymbolTime (int, Ofdm_mod_rate, int, direction);


  /**
   * Compute the total number of subchannels taken for a packet of size sdusize and
   * using the given modulation, direction, scheme)
   * @param sdusize The packet size
   * @param mod The modulation used
   * @param dir The direction
   */
  int getNumSubchannels (int sdusize, Ofdm_mod_rate mod, direction dir);

  /**
   * Return the maximum size in bytes that can be sent for the given 
   * nb of symbols and modulation, and direction.
   * This primitive assumes all subcarriers are used. 
   */
  int getMaxPktSizeForSymbol (int nbsymbols, Ofdm_mod_rate mod, direction dir);

  /* 
   * Compute the maximum packet size that can be put in the allocation using
   * the given number of subchannel, modulation, and direction
   * @param numSubchannels The number of subchannel allocated
   * @param mod The modulation rate
   * param dir The direction
   */
  int getMaxPktSize (int numSubchannels, Ofdm_mod_rate mod, direction dir);

  /*
   * Return the number of bytes that can fit into an allocation of 1 subchannel * 1 OFDM symbol
   * @param mod The modulation used
   * @param dir The direction
   */
  int getMinAllocSize (Ofdm_mod_rate mod, direction dir);

  int getSlotCapacity(Ofdm_mod_rate rate, direction dir);

  int getMaxBlockSize(Ofdm_mod_rate rate);


  int getMCSIndex (Ofdm_mod_rate rate , int block_size );

  /**
   * Return the duration of a PS (physical slot), unit for allocation time.
   * Use Frame duration / PS to find the number of available slot per frame
   */
  inline double  getPS () { return (4/fs_); }
    
  /**
   * Return the OFDM symbol duration time
   */
  double getSymbolTime ();

  /**
   * Compute the transmission time for a packet of size sdusize and
   * using the given modulation
   */
  double getTrxTime (int, Ofdm_mod_rate);

  /**
   * Compute the transmission time using OFDM symbol as
   * minimum allocation for a packet of size sdusize and
   * using the given modulation
   */
  double getTrxSymbolTime (int, Ofdm_mod_rate);

  /**
   * Return the maximum size in bytes that can be sent for the given 
   * nb of symbols and modulation
   */
  int getMaxPktSize (double nbsymbols, Ofdm_mod_rate);

  /**	
   * Return the number of PS used by an OFDM symbol
   */
  inline int getSymbolPS () { return (int) ((1+g_)*getFFT())/4; }

  /** 
   * Set the mode for physical layer
   */
  void setMode (Ofdm_phy_state mode);

  /**
   * Activate node
   */
  void node_on ();
  
  /**
   * Deactivate node
   */
  void node_off ();

  /**
   * get num antenna 
   */
  int getNumAntenna();

  /**
   * get the MIMO receiver combining scheme
   */
  int getMIMORxScheme();

  /** 
   * Get the bit rate per symbol and subcarrier for the given modulation
   * @param mod The modulation
   */
  double getModulationRate (Ofdm_mod_rate mod);

 protected:

  /**
   * Update the sampling frequency. Called after changing frequency BW
   */
  void updateFs ();

  /* Overwritten methods for handling packets */
  void sendDown(Packet *p);
  
  int sendUp(Packet *p);

 private:

  /**
   * The frequency bandwidth (Hz)
   */
  double fbandwidth_;
   
  /**
   * The current modulation
   */
  Ofdm_mod_rate modulation_;

  /**
   * The current downlink permutation scheme 
   */
  Permutation_scheme dl_perm_; 

  /**
   * The current downlink permutation scheme 
   */
  Permutation_scheme ul_perm_;    

  /**
   * The current transmitting power
   */
  int tx_power_;

  /**
   * Ratio of CP time over useful time
   */
  double g_;

  /**
   * The sampling frequency 
   */
  double fs_;
   
  /**
   * The state of the OFDM
   */
  Ofdm_phy_state state_;
   
  /**
   * Indicates if the node is activated
   */
  bool activated_;

  /** 
   * Index in the lookup table for uplink
   */
  u_int32_t ul_index_;

  /**
   * Index in the lookup table for downlink
   */
  u_int32_t dl_index_;
      
   
};
#endif //OFDMPHY_H


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

#include "ofdmphy.h"
#include "mac802_16pkt.h"

//#define DEBUG_WIMAX

/**
 * Tcl hook for creating the physical layer
 */
static class OFDMAPhyClass: public TclClass {
public:
  OFDMAPhyClass() : TclClass("Phy/WirelessPhy/OFDMA") {}
  TclObject* create(int, const char*const*) {
    return (new OFDMAPhy);
  }
} class_OfdmaPhy;

OFDMAPhy::OFDMAPhy() : WirelessPhy()
{
  //bind attributes
  bind ("g_", &g_);

  //default modulation is BPSK
  modulation_ = OFDM_64QAM_3_4;
  ul_index_ = 0;
  dl_index_ = 0;
  
  Tcl& tcl = Tcl::instance();
  //Load bandwidth
  tcl.evalf("Mac/802_16 set fbandwidth_");
  fbandwidth_ = atof (tcl.result()); 
  //Load downlink permutation
  tcl.evalf("Mac/802_16 set dl_permutation_");
  setPermutationscheme ((Permutation_scheme) atoi (tcl.result()), DL_); 
  //Load uplink permutation
  tcl.evalf("Mac/802_16 set ul_permutation_");
  setPermutationscheme ((Permutation_scheme) atoi (tcl.result()), UL_);

  state_ = OFDM_IDLE;
  activated_ = true;

  updateFs ();
}

/*
 * Activate node
 */
void OFDMAPhy::node_on ()
{
  activated_ = true;
}

/*
 * Deactivate node
 */
void OFDMAPhy::node_off ()
{
  activated_ = false;
}


/**
 * Change the frequency at which the phy is operating
 * @param freq The new frequency
 */
void OFDMAPhy::setFrequency (double freq)
{
  freq_ = freq;
  lambda_ = SPEED_OF_LIGHT / freq_;
}

/**
 * Set the new modulation for the physical layer
 * @param mod The new modulation
 */
void OFDMAPhy::setModulation (Ofdm_mod_rate mod) {
  modulation_ = mod;
}
/**
 * Return the current modulation
 */
Ofdm_mod_rate OFDMAPhy::getModulation () {
  return modulation_;
}

/**
 * Set the new permutation scheme for the physical layer
 * @param perm The new permutation scheme
 * @param dir The direction in which the permutation applies
 */
void OFDMAPhy::setPermutationscheme (Permutation_scheme perm, direction dir) 
{  
  /* Currently only supports PUSC */
  if (perm != PUSC) {
    fprintf (stderr, "OFDMAPhy: error, model currently supports PUSC only\n");
  }

  u_int32_t i = 0;
  if (dir == UL_) {
    ul_perm_ = perm;
    u_int nbRow = sizeof (UL_OFDMA_DATA) / (NB_OFDMA_PHY_PARAM*sizeof (unsigned long));
    for (i = 0; i < nbRow ; i++) {
      if (((u_int32_t)ul_perm_) == UL_OFDMA_DATA[i][OFDMA_INDEX_PERM]
	  && fbandwidth_ == (double) UL_OFDMA_DATA[i][OFDMA_INDEX_BW]) {
	ul_index_ = i;
	break;
      }
    }
    if (i == nbRow) {
      fprintf (stderr, "OFDMAPhy: error did not find match for permutation and bw combination\n");
      exit (1);
    }
  }
  else {
    dl_perm_ = perm;
    u_int nbRow = sizeof (DL_OFDMA_DATA) / (NB_OFDMA_PHY_PARAM*sizeof (unsigned long));
    for (i = 0; i < nbRow ; i++) {
      if ((u_int32_t)dl_perm_ == DL_OFDMA_DATA[i][OFDMA_INDEX_PERM]
	  && fbandwidth_ == (double) DL_OFDMA_DATA[i][OFDMA_INDEX_BW]) {
	dl_index_ = i;
	break;
      }
    }
    if (i == nbRow) {
      fprintf (stderr, "OFDMAPhy: error did not find match for permutation and bw combination\n");
      exit (1);
    }
  }
}

/**
 * get the current permutation scheme
 * @param dir The direction 
 */
Permutation_scheme OFDMAPhy::getPermutationscheme (direction dir) {
  if (dir == UL_)
    return ul_perm_;
  return dl_perm_;
}

/**
 * calculate the FFT size (same of UL and DL)
 * @return the FFT size
 */
int OFDMAPhy::getFFT () {
  return (int) (UL_OFDMA_DATA[ul_index_][OFDMA_INDEX_FFT]);
}

/**
 * calculate the number of subchannels
 * @param dir The direction
 * @return The number of subchannel
 */
int OFDMAPhy::getNumsubchannels (direction dir) {
  if (dir == UL_)
    return UL_OFDMA_DATA[ul_index_][OFDMA_INDEX_NBSUBCH];
  else 
    return DL_OFDMA_DATA[dl_index_][OFDMA_INDEX_NBSUBCH];
}

/**
 * Get the number of subcarrier per subchannel per symbol
 * @param dir The direction
 * @return The number of subcarrier
 */
int OFDMAPhy::getNumSubcarrier (direction dir) {
  if (dir == UL_)
    return UL_OFDMA_DATA[ul_index_][OFDMA_INDEX_NBSUBCAR];
  else 
    return DL_OFDMA_DATA[dl_index_][OFDMA_INDEX_NBSUBCAR];
}

/**
 * Get the number of data subcarrier per subchannel per symbol
 * @param dir The direction
 * @return The numbe of data subcarrier
 */
int OFDMAPhy::getNumDataSubcarrier (direction dir) {
  if (dir == UL_)
    return UL_OFDMA_DATA[ul_index_][OFDMA_INDEX_NBDATASUBCAR];
  else 
    return DL_OFDMA_DATA[dl_index_][OFDMA_INDEX_NBDATASUBCAR];
}

/**
 * Get the number of data subcarrier per subchannel per symbol
 * @param dir The direction
 */
int OFDMAPhy::getSlotLength (direction dir) {
  if (dir == UL_)
    return UL_OFDMA_DATA[ul_index_][OFDMA_INDEX_SLOT_LENGTH];
  else 
    return DL_OFDMA_DATA[dl_index_][OFDMA_INDEX_SLOT_LENGTH];
}

/**
 * Get the total number of subcarriers per symbol
 * @param dir The direction
 */
int OFDMAPhy::getNUsed (direction dir) {
  if (dir == UL_)
    return UL_OFDMA_DATA[ul_index_][OFDMA_INDEX_NUSED];
  else 
    return DL_OFDMA_DATA[dl_index_][OFDMA_INDEX_NUSED];
}

/** 
 * Get the bit rate per symbol and subcarrier for the given modulation
 * @param mod The modulation
 */
double OFDMAPhy::getModulationRate (Ofdm_mod_rate mod)
{
  switch (mod) {
  case OFDM_BPSK_1_2:
    return 1.0 * 1 / 2;
  case OFDM_QPSK_1_2:
    return 2.0 * 1 / 2;    
  case OFDM_QPSK_3_4:
    return 2.0 * 3 / 4;
  case OFDM_16QAM_1_2:
    return 4.0 * 1 / 2;
  case OFDM_16QAM_3_4:
    return 4.0 * 3 / 4;
  case OFDM_64QAM_2_3:
    return 6.0 * 2 / 3;
  case OFDM_64QAM_3_4:
    return 6.0 * 3 / 4; 
  default:
    fprintf (stderr, "Error in  OFDMAPhy::getModulationRate modulation unknown\n");
    exit (1);
  }
  return 0.0;
}

/**
 * Compute the transmission time for a packet of size sdusize, num subchannels allocated per OFDM symbol, direction,
 * using the given modulation
 */
double OFDMAPhy::getTrxTime (int sdusize, Ofdm_mod_rate mod, int num_subchannels, direction dir)
{
  //we compute the number of symbols required
  int bpsymb = (int) floor((num_subchannels*getNumDataSubcarrier(dir)) * getModulationRate (mod));
  double txtime = (int) (sdusize*8*getSymbolTime ()/bpsymb);

#ifdef DEBUG_WIMAX
  printf ("\tOFDMAPhy::getTrxTime bpsymbol %d duration:%f\n", bpsymb, txtime);
#endif

  return txtime;
}

/**
 * Compute the transmission time using OFDM symbol as
 * minimum allocation for a packet of size sdusize and
 * using the given modulation, num subchannels, direction)
 * @param sdusize The packet size
 * @param mod The modulation used
 * @param dir The direction
 */
int OFDMAPhy::getNumSubchannels (int sdusize, Ofdm_mod_rate mod, direction dir) 
{
  //we compute the number of symbols required
  double bpsubchn = floor (getNumDataSubcarrier(dir)*getSlotLength(dir)*getModulationRate (mod));
  int nb_subchannels = (int) ceil(((double)sdusize*8)/bpsubchn);

#ifdef DEBUG_WIMAX
  printf("#_data_subcarrier[%d]\t slot_len[%d]\t modulation[%e]\t bpsubchn[%e]\n",
	 getNumDataSubcarrier(dir),getSlotLength(dir),getModulationRate (mod),bpsubchn);
  printf ("\tOFDMAPhy::getNumSubchannels bpsubchannel=%d Nb subchannel  (SDUsize_bits/bpsubchn):%d\n", (int) bpsubchn, nb_subchannels);
#endif

  return (nb_subchannels);
}

/* 
 * Compute the maximum packet size that can be put in the allocation using
 * the given number of subchannel, modulation, and direction
 * @param numSubchannels The number of subchannel allocated
 * @param mod The modulation rate
 * param dir The direction
 */ 
int OFDMAPhy::getMaxPktSize (int numSubchannels, Ofdm_mod_rate mod, direction dir) {
  //we compute the number of symbols required
  double bpsubchn = floor (getNumDataSubcarrier(dir)*getSlotLength(dir)*getModulationRate (mod));
  int max_size = (int) (floor((numSubchannels*bpsubchn)/8));

#ifdef DEBUG_WIMAX
  printf ("\tOFDMAPhy::getMaxPktSize nbSuchannel=%d mod=%d dir=%d MaxPacketSize %d\n", numSubchannels, mod,dir, max_size);
#endif

  return (max_size);
}

/*
 * Return the number of bytes that can fit into an allocation of 1 subchannel * 1 OFDM symbol
 * @param mod The modulation used
 * @param dir The direction
 */
int OFDMAPhy::getMinAllocSize (Ofdm_mod_rate mod, direction dir) {
  double bpsubchn = floor (getNumDataSubcarrier(dir)*getModulationRate (mod));
  int alloc_size = (int) (floor(bpsubchn/8));

#ifdef DEBUG_WIMAX
  printf ("\tOFDMAPhy::getMinAllocSize mod=%d dir=%d MinAllocSize %d\n", mod,dir, alloc_size);
#endif

  return alloc_size;
}

/**
 * Compute the transmission time using OFDM symbol as
 * minimum allocation for a packet of size sdusize and
 * using the given modulation, num subchannels, direction, scheme)
 */
double OFDMAPhy::getTrxSymbolTime (int sdusize, Ofdm_mod_rate mod, int num_subchannels, direction dir) {
  //we compute the number of symbols required
  double bpsymb = (int) floor((num_subchannels*getNumDataSubcarrier(dir)) * getModulationRate (mod));
  double nb_symbols = ceil(((double)sdusize*8)/bpsymb);

  //#ifdef DEBUG_WIMAX
  printf ("Nb symbols=%f\n", nb_symbols);
  //#endif

  return (nb_symbols*getSymbolTime ());
}

/**
 * Return the maximum size in bytes that can be sent for the given 
 * nb of symbols and modulation, and direction. 
 */
int OFDMAPhy::getMaxPktSizeForSymbol (int nbsymbols, Ofdm_mod_rate mod, direction dir) {

  //we compute the bits per symbol (using all used subcarriers)
  double bpsymb = (int) floor((getNUsed(dir)) * getModulationRate (mod));
  int max_size = (int) floor((nbsymbols*bpsymb)/8);

#ifdef DEBUG_WIMAX
  printf ("\tOFDMAPhy::getMaxPktSizemax nb symbols=%d mod=%d nb subcarriers= %d packet size = %d\n", nbsymbols, mod, getNUsed(dir), max_size);
#endif
  return max_size;
}

/**
 * Set the new transmitting power
 */
void OFDMAPhy::setTxPower (double power) {
  Pt_ = power;
}
/**
 * Return the current transmitting power
 */
double OFDMAPhy::getTxPower () {
  return getPt();
}

/** 
 * Update the PS information
 */
void OFDMAPhy::updateFs () {
  /* The PS=4*Fs with Fs=floor (n.BW/8000)*8000
   * and n=8/7 is channel bandwidth multiple of 1.75Mhz
   * n=86/75 is channel bandwidth multiple of 1.5Mhz
   * n=144/125 is channel bandwidth multiple of 1.25Mhz
   * n=316/275 is channel bandwidth multiple of 2.75Mhz
   * n=57/50 is channel bandwidth multiple of 2.0Mhz
   * n=8/7 for all other cases
   */
  double n; 
    
  if (fbandwidth_ != 10e+6) {
    fprintf (stderr, "Error: Currently only supports 10Mhz bandwidth\n");
    exit (1);
  }

  if (((int) (fbandwidth_ / 1.25)) * 1.25 == fbandwidth_
      || ((int) (fbandwidth_ / 1.5)) * 1.5 == fbandwidth_
      || ((int) (fbandwidth_ / 2.0)) * 2.0 == fbandwidth_
      || ((int) (fbandwidth_ / 2.75)) * 2.75 == fbandwidth_) {
    n = 28.0/25;
  } else {
    n = 8.0/7;
  }

  fs_ = floor (n*fbandwidth_/8000) * 8000;
#ifdef DEBUG_WIMAX
  printf ("Fs updated. Bw=%f, n=%f, new value is %e\n", fbandwidth_, n, fs_);
#endif
}

/*
 * Compute the transmission time for a packet of size sdusize and
 * using the given modulation when ALL subcarriers are used (OFDM type)
 * @param sdusize Size in bytes of the data to send
 * @param mod The modulation to use
 */
double OFDMAPhy::getTrxTime (int sdusize, Ofdm_mod_rate mod) {
  //we compute the number of symbols required
  int bpsymb = (int) floor((getNUsed(DL_)) * getModulationRate (mod));

#ifdef DEBUG_WIMAX
  printf ("Nb symbols=%f\n", ((double)sdusize*8)/bpsymb);
#endif

  return sdusize*8*getSymbolTime ()/bpsymb;
}

/*
 * Compute the transmission time for a packet of size sdusize and
 * using the given modulation
 * @param sdusize Size in bytes of the data to send
 * @param mod The modulation to use
 */
double OFDMAPhy::getTrxSymbolTime (int sdusize, Ofdm_mod_rate mod) {
  //we compute the number of symbols required
  int bpsymb = (int) floor((getNUsed(DL_)) * getModulationRate (mod));
  int nb_symbols;

#ifdef DEBUG_WIMAX
  printf ("Nb symbols=%d\n", (int) (ceil(((double)sdusize*8)/bpsymb)));
#endif

  nb_symbols = (int) (ceil(((double)sdusize*8)/bpsymb));
  return (nb_symbols*getSymbolTime ());
}

/*
 * Return the maximum size in bytes that can be sent for the given 
 * nb symbols and modulation
 */
int OFDMAPhy::getMaxPktSize (double nbsymbols, Ofdm_mod_rate mod)
{
  int bpsymb = (int) floor((getNUsed(DL_)) * getModulationRate (mod));

  return (int)(nbsymbols*bpsymb)/8;
}

/**
 * Return the OFDM symbol duration time
 */
double OFDMAPhy::getSymbolTime () 
{ 
  //printf ("fs=%e, Subcarrier spacing=%e\n", fs_, fs_/((double)NFFT));
  //return ceil( OFDM_PRECISION*(1+g_)*((double)NFFT)/fs_)/ OFDM_PRECISION;
  return ((1+g_)*((double)getFFT()))/fs_;
}


/*
 * Set the mode for physical layer
 * The Mac layer is in charge of know when to change the state by 
 * request the delay for the Rx2Tx and Tx2Rx
 */
void OFDMAPhy::setMode (Ofdm_phy_state mode)
{
  state_ = mode;
}

/* Redefine the method for sending a packet
 * Add physical layer information
 * @param p The packet to be sent
 */
void OFDMAPhy::sendDown(Packet *p)
{
  hdr_mac802_16* wph = HDR_MAC802_16(p);

  /* Check phy status */
  if (state_ != OFDM_SEND) {
#ifdef DEBUG_WIMAX
    printf ("Warning: OFDM not in sending state. Drop packet.\n");
#endif
    Packet::free (p);
    return;
  }

#ifdef DEBUG_WIMAX
  printf ("OFDM phy sending packet. Modulation is %d, cyclic prefix is %f\n", 
	  modulation_, g_);
#endif 

  wph->phy_info.freq_ = freq_;
  wph->phy_info.modulation_ = modulation_;
  wph->phy_info.g_ = g_;

  //the packet can be sent
  WirelessPhy::sendDown (p);
}

/* Redefine the method for receiving a packet
 * Add physical layer information
 * @param p The packet to be sent
 */
int OFDMAPhy::sendUp(Packet *p)
{
  hdr_mac802_16* wph = HDR_MAC802_16(p);

  if (!activated_){
#ifdef DEBUG_WIMAX
    printf ("drop packet because OFDM-phy not activated \n");
#endif
    return 0;
  }

  if (freq_ != wph->phy_info.freq_) {
#ifdef DEBUG_WIMAX
    printf ("drop packet because frequency is different (%f, %f)\n", freq_,wph->phy_info.freq_);
#endif 
    return 0;
  }
  /* Check phy status */

  if (state_ != OFDM_RECV) {
#ifdef DEBUG_WIMAX
    printf ("Warning: OFDM phy not in receiving state now in state[%d]. Drop packet.\n",state_);
#endif 
    return 0;
  }

#ifdef DEBUG_WIMAX
  printf ("OFDM phy receiving packet with mod=%d and cp=%f\n", wph->phy_info.modulation_,wph->phy_info.g_);
#endif 


  //the packet can be received
  return WirelessPhy::sendUp (p);
}


int OFDMAPhy::getSlotCapacity(Ofdm_mod_rate rate, direction dir){
  return (int) (getNumDataSubcarrier (dir) * getSlotLength (dir) * getModulationRate(rate))/8 ;
}

int OFDMAPhy::getMaxBlockSize(Ofdm_mod_rate rate){
  int size;
  switch(rate){
  case OFDM_BPSK_1_2:
    size = 10;
    break;
  case OFDM_QPSK_1_2:
    size = 10;
    break;
  case OFDM_QPSK_3_4:
    size = 6;
    break;
  case OFDM_16QAM_1_2:
    size = 5;
    break;
  case OFDM_16QAM_3_4:
    size = 3;
    break;
  case OFDM_64QAM_2_3:
    size = 2;
    break;
  case OFDM_64QAM_3_4:
    size = 2;
    break;
  default:
    size = 2;
    break;
  }

  return size;
}

int OFDMAPhy::getMCSIndex (Ofdm_mod_rate rate , int block_size ) {

  int index; 
  switch(rate){
  case OFDM_QPSK_1_2:
    if( block_size == 1) 
      index = 1;
    else if (block_size == 2)
      index = 2;
    else if (block_size == 3)
      index = 3;
    else if (block_size == 4)
      index = 4;
    else if (block_size == 5)
      index = 5; 
    else if (block_size == 6)
      index = 6;
    else if (block_size == 7)
      index = 6;	
    else if (block_size == 8)
      index = 7;
    else if (block_size == 9)
      index = 8;
    else if (block_size == 10)
      index = 9;       
    break;
  case OFDM_QPSK_3_4:
    if( block_size == 1) 
      index = 10;
    else if (block_size == 2)
      index = 11;
    else if (block_size == 3)
      index = 12;
    else if (block_size == 4)
      index = 13;
    else if (block_size == 5)
      index = 14; 
    else if (block_size == 6)
      index = 15;
    break;
  case OFDM_16QAM_1_2:
    if( block_size == 1) 
      index = 16;
    else if (block_size == 2)
      index = 17;
    else if (block_size == 3)
      index = 18;
    else if (block_size == 4)
      index = 19;
    else if (block_size == 5)
      index = 20;
    break;
  case OFDM_16QAM_3_4:
    if( block_size == 1) 
      index = 21;
    else if (block_size == 2)
      index = 22;
    else if (block_size == 3)
      index = 23;
    break;
    /*    case OFDM_64QAM_1_2:              64QAM1-2, and 5-6 not implemented currently
	  if( block_size == 1) 
	  index = 24;
	  else if (block_size == 2)
	  index = 25;
	  else if (block_size == 2)
	  index = 26;
	  break;
    */    
  case OFDM_64QAM_2_3:
    if( block_size == 1) 
      index = 27;
    else if (block_size == 2)
      index = 28;
    break;
  case OFDM_64QAM_3_4:
    if( block_size == 1) 
      index = 29;
    else if (block_size == 2)
      index = 30;
    break;
    /*     case OFDM_64QAM_5_6:
	   if( block_size == 1) 
	   index = 31;
	   else if (block_size == 2)
	   index = 32;
	   break; 
    */    default:
    index = 1;
    break;
  }

  return index;
}

/** RPI - To get num of antenna's per node in case of MIMO 
 */
int OFDMAPhy::getNumAntenna()
{
  return (((MobileNode*)node())->num_antenna());
} 

/* RPI - to get the receiver combining scheme in case of MIMO 
 */

int OFDMAPhy::getMIMORxScheme()
{
  return (((MobileNode*)node())->MIMO_Rx_scheme());
}

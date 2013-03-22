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

#ifndef WIMAX_SCHEDULER_H
#define WIMAX_SCHEDULER_H

#include "packet.h"
#include "mac802_16.h"
#include "framemap.h"
#include "neighbordb.h"

#define TX_GAP 0.000001 //Time in seconds between sending 2 packets (to avoid collision)

/**
 * Super class for schedulers (BS and MS schedulers)
 */ 
class WimaxScheduler : public TclObject 
{
public:
  /*
   * Create a scheduler
   */
  WimaxScheduler ();

  /*
   * Set the mac
   * @param mac The Mac where it is located
   */
  void setMac (Mac802_16 *mac);

  /**
   * Initializes the scheduler
   */
  virtual void init ();

  /**
   * Return the Mac layer
   */
  inline Mac802_16 *  getMac () { return mac_;}
    
  /**
   * Transfert the packets from the given connection to the given burst
   * @param con The connection
   * @param b The burst
   * @param b_data The amount of data in burst
   * @return the new burst occupation
   */
  int transfer_packets (Connection *c, Burst *b, int b_data, int subchannel_offset, int symbol_offset);

//Begin RPI
/**
   * Transfert the packets from the given connection to the given burst
   * @param con The connection
   * @param b The burst
   * @param b_data The amount of data in burst
   * @return the new burst occupation
   */
  int transfer_packets_with_fragpackarq (Connection *c, Burst *b, int b_data);
//End RPI

  /**
   * Transfert the packets from the given connection to the given burst
   * @param con The connection
   * @param b The burst
   * @param b_data The amount of data in burst
   * @return the new burst occupation
   */
  int transfer_packets1 (Connection *c, Burst *b, int b_data);



  /**
   * This function is used to schedule bursts/packets
   */
  virtual void schedule ();

protected:

  /**
   * The Mac layer
   */
  Mac802_16 * mac_;

  /**
   * The ratio for downlink subframe
   */
  double dlratio_;
  
private:



};
#endif //SCHEDULER_H


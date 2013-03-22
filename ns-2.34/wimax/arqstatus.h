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
 * @author  ksshar
 */
#ifndef ARQSTATUS_H
#define ARQSTATUS_H

#include "mac802_16timer.h"
#include "packet.h"
#include "queue.h"
#include "mac802_16pkt.h"

//#define MAX_TIMERS 100 /*RPI*/
 
class Arqstatus {
 public:

  /* 
   * Create a ArqStatus
   * @param 
   */
   Arqstatus ();
 
  /* 
   * Delete the ArqStatus
   * @param 
   */
   ~Arqstatus (); 
 
  /**
   * Get the Arq Status
   * The isArqEnabled
   * @return the value of is_arq_enabled_
   */
  inline u_char isArqEnabled() { return is_arq_enabled_; }

  /**
   * Get the value of Arq Retransmission Time
   * The Arq Retransmission Time
   * @return the value of arq_retrans_time_
   */
  inline double getRetransTime() { return arq_retrans_time_; } 

  /**
   * Get the value of Arq Maximum Window
   * The Arq Maximum Window
   * @return the value of arq_max_window_
   */
  inline u_int32_t getMaxWindow() { return arq_max_window_; }

  /**
   * Get the value of Arq Maximum Window
   * The Arq Maximum Window
   * @return the value of arq_max_window_
   */
  inline void setMaxWindow(u_int32_t arq_max_window) { arq_max_window_ = arq_max_window; }  

  /**
   * Get the value of Arq Maximum Sequence
   * The Arq Maximum Sequence
   * @return the value of arq_max_seq_
   */
  inline u_int32_t getMaxSeq() { return arq_max_seq_; }
 
  /**
   * Get the value of Acknowledgement Period
   * The Acknowledgement Period
   * @return the value of ack_period_
   */
  inline u_int8_t getAckPeriod() { return ack_period_; }

  /**
   * Set the value of Acknowledgement Period
   * The Acknowledgement Period
   * @return the value of ack_period_
   */
  inline void setAckPeriod(u_int8_t ack_period ) { ack_period_ = ack_period; }


  /**
   * Get the value of current Arq Sequence
   * The connection current Arq Sequence
   * @return the value of arq_curr_seq_
   */
  inline u_int32_t getCurrSeq() { return arq_curr_seq_; }

  /**
     *Cancel ARQ re-transmission timer.
     */
  void  cancelTimer() {arqRetransTimer->cancel();}


  /**
   * Set the Arq Status
   * The isArqEnabled
   * @return the value of is_arq_enabled_
   */
  inline void setArqEnabled(u_char is_arq_enabled) { is_arq_enabled_ = is_arq_enabled; }

  /**
   * Set the value of Arq Retransmission Time
   * The Arq Retransmission Time
   * @return the value of arq_retrans_time_
   */
  inline void setRetransTime(double arq_retrans_time) { arq_retrans_time_ = arq_retrans_time; }

   /**
   * Set the value of current Arq Sequence
   * The connection current Arq Sequence
   * @return the value of arq_curr_seq_
   */
  inline void setCurrSeq(u_int32_t arq_curr_seq) { arq_curr_seq_ = arq_curr_seq; }
  
  /**
   * Get the value of Current Window
   * The current window
   * @return the value of arq_curr_window_
   */
  inline u_int32_t getCurrWindow() { return arq_curr_window_; }

  /**
   * Set the value of Current Window
   * The current window
   * @return the value of arq_curr_window_
   */
  inline void setCurrWindow(u_int32_t arq_curr_window) { arq_curr_window_ = arq_curr_window; }

  /**
   * Get the value of acknowledgement counter
   * The acknowledgement counter
   * @return the value of ack_counter_
   */
  inline u_int8_t getAckCounter() { return ack_counter_; }
  
  /**
   * Set the value of acknowledgement counter
   * The acknowledgement counter
   * @return the value of ack_counter_
   */
  inline void setAckCounter(u_int8_t ack_counter) { ack_counter_ = ack_counter; }

  /**
   * Get the value of Ack Sequence
   * The Ack Sequence
   * @return the value of ack_seq_
   */
  inline u_int32_t getAckSeq() { return ack_seq_; }

  /**
   * Set the value of Ack Sequence
   * The Ack Sequence
   * @return the value of ack_seq_
   */
  inline void setAckSeq(u_int32_t ack_seq) { ack_seq_ = ack_seq; }

  /**
   * Get the value of  Last Ack Sequence
   * The Last Ack Sequence
   * @return the value of last_ack_sent_
   */
  inline u_int32_t getLastAckSent() { return last_ack_sent_; }

  /**
   * Set the value of  Last Ack Sequence
   * The Last Ack Sequence
   * @return the value of last_ack_sent_
   */
  inline void setLastAckSent(u_int32_t last_ack_sent) { last_ack_sent_ = last_ack_sent; }
  
  /**
   * The queue for holding ARQ transmission packets
   */
  PacketQueue*        arq_trans_queue_;  

  /**
   * The queue for holding retransmission packets in ARQ
   */
  PacketQueue*        arq_retrans_queue_; 

  /**
   * The queue to hold feedback payloads in ARQ
   */
   PacketQueue*        arq_feedback_queue_;   

  /**
   * The Timer array for arq retransmission
   */
  ARQTimer *arqRetransTimer;      /*RPI*/
//Begin RPI
  /**
   * The  ARQ Timer Handler Function
   */
  void arqTimerHandler();
//End RPI  
  /**
   * The method to perform ARQ sender side functions
   */
  void arqSend(Packet* p, Connection *connection, fragment_status status);      

  /**
   * The method to perform ARQ receive side functions
   */
  void arqReceive(Packet* p, Connection *connection, u_int8_t* inOrder);    

//Begin RPI
  /**
   * The method to perform ARQ receive side functions
   */
  void arqReceiveBufferTransfer(Packet* p, Connection *connection, u_int8_t* inOrder);    
        

  /**
   * The  method to handle ARQ feedback and related operations
   */
  void arqRecvFeedback(Packet* p, u_int16_t i, Connection* connection);              
//End RPI   

 private:
  
  /**
   *  isArqenabled: 0 - not enabled, 1- enabled
   */
  u_char        is_arq_enabled_;       

  /**
   *  ARQ retransmission time period - sender side
   */
  double        arq_retrans_time_; 
 
  /**
   *  ARQ ack sequence number
   */
  u_int32_t     ack_seq_;         

  /**
   *  ARQ ack timing counter
   */
  u_int8_t      ack_counter_;     

  /**
   *  ARQ current sequence number at sending side
   */
  u_int32_t     arq_curr_seq_;     

  /**
   *  ARQ ack timing period - set by user
   */
  u_int8_t      ack_period_;    

  /**
   *  Max sequence number - decided based on window size
   */ 
  u_int32_t     arq_max_seq_;  

  /** 
   * ARQ window size in bytes
   */
  u_int32_t     arq_max_window_; 

  /** 
   * ARQ: current ARQ window size for the flow, in bytes
   */
  u_int32_t     arq_curr_window_;    

  /** 
   * ARQ:  last ACK sent in a feedback IE, to eliminate repeat ACKs
   */
  u_int32_t     last_ack_sent_; 
  
};
#endif



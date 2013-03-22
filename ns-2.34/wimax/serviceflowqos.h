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

#ifndef SERVICEFLOWQOS_H
#define SERVICEFLOWQOS_H

#include "packet.h"
#include "queue.h"

/**
 * Class ServiceFlowQoS
 * Defines Qos requirements for the flows
 */ 
class ServiceFlowQoS {
  
 public:
  /**
   * Constructor
   * @param delay The maximum supported delay for the connection
   * @param datarate Average datarate
   * @param burstsize Size of each burst
   */
  ServiceFlowQoS (int delay, int datarate, int burstsize);
  
  /**
   * Return the maximum delay supported by the connection
   */
  inline double  getDelay () { return delay_; }
  
  /**
   * Return the average datarate
   */
  inline double  getDatarate () { return datarate_; }
  
  /**
   * Return the burst size
   */
  inline int  getBurstSize () { return burstsize_; }

  /**
   * Return the data size
   */
  inline double getDataSize () { return data_size_; }
   
  /**
   * Return the minReserved Rate
   */
  inline u_int32_t getMinReservedRate () { return min_reserved_traffic_rate_; }

  /**
   * Return the period
   */
  inline u_int16_t  getPeriod () { return period_; }

//start 

  /**
   * Return get the traffic priority
   */
  inline u_int8_t getTrafficPriority () { return traffic_priority_; }

  /**
   * Return the peak traffic rate 
   */
  inline u_int32_t getPeakTrafficRate () { return peak_traffic_rate_; }

  /**
   * Return the minimum reserved traffic rate
   */
  inline u_int32_t getMinReservedTrafficRate () { return min_reserved_traffic_rate_; }

  /**
   * Return the minimum tolearable traffic rate
   */
  inline u_int32_t getMinTolerableTrafficRate () { return min_tolerable_traffic_rate_; }

  /**
   * Return the request/transmit policy
   */
  inline u_int32_t getReqTransmitPolicy () { return reqtransmit_policy_; }

  /**
   * Return the jitter value
   */
  inline u_int32_t getJitter () { return jitter_; }

  /**
   * Return the SDU indicator
   */
  inline u_int8_t getSDUIndicator () { return sdu_indicator_; }

  /**
   * Return the SDU size
   */
  inline u_int8_t getSDUSize () { return sdu_size_; }

  /**
   * Return the max burst size
   */
  inline u_int32_t getMaxBurstSize () { return max_burst_size_; }

  /**
   * Return the value of SAID
   */
  inline u_int16_t getSAID () { return said_; } 

//end 
  
  
  /**
   * Return the ARQ Status
   */
  inline u_char  getIsArqEnabled () { return is_arq_enabled_; }
 
   /**
   * Return the ARQ retrans time
   */
  inline double getArqRetransTime () { return arq_retrans_time_; }

   /**
   * Return the ARQ maximum window
   */
  inline u_int32_t getArqMaxWindow () { return arq_max_window_; }
 
   /**
   * Return the ARQ Acknowledgement Period
   */
  inline u_int8_t getArqAckPeriod () { return ack_period_; }
  
  /**
   * Set the maximum delay supported by the connection
   * @param delay The new delay
   */
  inline void  setDelay (double delay) { delay_ = delay; }
  
  /**
   * Set the average datarate for the connection
   * @param datarate The average datarate
   */
  inline void  setDatarate (double datarate) { datarate_ = datarate; }
  
  /**
   * Set the burst size for the connection
   * @param size The number of byte sent for each burst
   */
  inline void  setBurstSize (int size) { burstsize_ = size; }

  /**
   * Set the data size
   */
  inline void  setDataSize (double data_size) { data_size_ = data_size; }

  /**
   * Set the period
   */
  inline void  setPeriod (u_int16_t period) { period_ = period; }

//start

  /**
   * Set the traffic priority 
   */
  inline void setTrafficPriority (u_int8_t traffic_priority) { traffic_priority_ = traffic_priority; } 

  /**
   * Set the peak traffic rate 
   */
  inline void setPeakTrafficRate (u_int32_t peak_traffic_rate) { peak_traffic_rate_ = peak_traffic_rate; } 

  /**
   * Set the minimum reserved traffic rate
   */
  inline void setMinReservedTrafficRate (u_int32_t min_reserved_traffic_rate) { min_reserved_traffic_rate_ = min_reserved_traffic_rate; } 

  /**
   * Set the request/transmit policy 
   */
  inline void setReqTransmitPolicy (u_int32_t reqtransmit_policy) { reqtransmit_policy_ = reqtransmit_policy; } 

  /**
   * Set the jitter value. 
   */
  inline void setJitter (u_int32_t jitter) { jitter_ = jitter; } 

  /**
   * Set the SDU indicator
   */
  inline void setSDUIndicator (u_int8_t sdu_indicator) { sdu_indicator_ = sdu_indicator; } 

  /**
   * Set the minimum tolerable traffic rate 
   */
  inline void setMinTolerableTrafficRate (u_int32_t min_tolerable_traffic_rate) { min_tolerable_traffic_rate_ = min_tolerable_traffic_rate; } 

  /**
   * Set the SDU size
   */
  inline void setSDUSize (u_int8_t sdu_size) { sdu_size_ = sdu_size; } 

  /**
   * Set the  max burst size
   */
  inline void setMaxBurstSize (u_int32_t max_burst_size) { max_burst_size_ = max_burst_size; } 

  /**
   * Set SAID 
   */
  inline void setSAID (u_int16_t said) { said_ = said; } 


//end

  /**
   * Set the ARQ Status
   */
  inline void  setIsArqEnabled (u_char is_arq_enabled) { is_arq_enabled_ = is_arq_enabled; }
 
   /**
   * Set the ARQ retrans time
   */
  inline void setArqRetransTime (double arq_retrans_time) { arq_retrans_time_ = arq_retrans_time; }

   /**
   * Set the ARQ maximum window
   */
  inline void setArqMaxWindow ( u_int32_t arq_max_window) { arq_max_window_ = arq_max_window; }
 
   /**
   * Return the ARQ Acknowledgement Period
   */
  inline void setArqAckPeriod (u_int8_t ack_period ) { ack_period_ = ack_period; }

  
 protected:

 private:
  /**
   * The maximum delay for this connection (in sec)
   */
   double delay_;
  /**
   * The average datarate
   */
   double datarate_;
  /**
   * The number of bytes per burst
   */
   int burstsize_;

   /**
    * The data size
    */
   double data_size_;

   /**
    * The Period
    */
   u_int16_t     period_;

    /**
    * Traffic Priority, 0 to 7—Higher numbers indicate higher priority, Default 0.
    */  
   
   u_int8_t      traffic_priority_; 

    /**
    * Maximum sustained traffic rate in bits per sec. 
    */  
    
   u_int32_t     peak_traffic_rate_;   

   /**
   * Maximum Traffic Burst in bytes
   */ 

   u_int32_t     max_burst_size_;

   /**
   * Minimum reserved traffic rate in bits per second 
   */  
 
   u_int32_t      min_reserved_traffic_rate_;

   /**
   *  Minimum tolerable traffic rate in bps   
   */
 
   u_int32_t      min_tolerable_traffic_rate_;     

   /**
   *  Request/transmission policy   
   */
 
   u_int32_t      reqtransmit_policy_;  

   /**
   * Tolerated Jitter  
   */
 
   u_int32_t      jitter_;     

    /**
   * Fixed-length versus variable-length SDU indicator
   */
 
   u_int8_t      sdu_indicator_;     

    /**
   *  SDU size 
   */
 
   u_int8_t      sdu_size_;     
  
   /**
   *  Target SAID
   */
 
   u_int16_t      said_;     
  

   /**	
   *  0 - not enabled, 1- enabled
   */
   u_char        is_arq_enabled_;       
   /**
   * ARQ retransmission time period - sender side
   */
   double        arq_retrans_time_; 
   /**
   *  ARQ window size in bytes
   */
   u_int32_t     arq_max_window_;  
   /**
   * ARQ ack timing period - set by user
   */
   u_int8_t      ack_period_;

};
#endif //SERVICEFLOWQOS_H


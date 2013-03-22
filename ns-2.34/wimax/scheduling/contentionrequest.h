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

#ifndef CONTENTIONREQUEST_H
#define CONTENTIONREQUEST_H

#include "mac802_16.h"
#include "mac802_16timer.h"

class ContentionSlot;
class RngContentionSlot;
class BwContentionSlot;
class ContentionTimer;
class Mac802_16;

class ContentionRequest;
/** Timer for backoff */
class WimaxBackoffTimer : public WimaxTimer {
 public:
  WimaxBackoffTimer(ContentionRequest *c, Mac802_16 *m) : WimaxTimer(m) {c_=c;}
  
  void	handle(Event *e);
  void pause(void);
  void resume(void);
 private:
  ContentionRequest *c_;
}; 

/** Timer for contention slot */
class ContCountTimer : public WimaxTimer {
 public:
  ContCountTimer(ContentionRequest *c, Mac802_16 *m) : WimaxTimer(m) {c_=c;}
  
  void	handle(Event *e);
 private:
  ContentionRequest *c_;
}; 


class ContentionRequest;
LIST_HEAD (contentionRequest, ContentionRequest);

/**
 * This class is used to manage contention opportunities
 * supports list
 */
class ContentionRequest
{
  friend class WimaxBackoffTimer;
 public:
  /**
   * Creates a contention slot for the given frame
   * @param s The contention slot 
   * @param p The packet to send
   */
  ContentionRequest (ContentionSlot *s, Packet *p);
  virtual ~ContentionRequest ();
  /**
   * Called when timeout expired
   */
  virtual void expire ();

  /**
   * Start the timeout timer
   */
  void starttimeout();

  /** 
   * Pause the backoff timer
   */
  void pause ();
  
  /**
   * Resume the backoff timer
   * @param duration The duration for resuming count down before going back
   *        to idle. The duration equals the allocation.
   */
  void resume (double duration);

  /// Chain element to the list
  inline void insert_entry_head(struct contentionRequest *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  /// Chain element to the list
  inline void insert_entry(ContentionRequest *elem) {
    LIST_INSERT_AFTER(elem, this, link);
  }

  /// Return next element in the chained list
  ContentionRequest* next_entry(void) const { return link.le_next; }

  /// Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

 protected:

  /**
   * The contention slot information
   */
  ContentionSlot *s_;

  /**
   * The backoff timer
   */
  WimaxBackoffTimer *backoff_timer_;

  /**
   * The timeout timer
   */
  ContentionTimer *timeout_timer_;

  /**
   * The count timer 
   */
  ContCountTimer *count_timer_;

  /**
   * Type of timer
   */
  timer_id type_; 

  /**
   * Value for timeout
   */
  double timeout_;

  /**
   * The current window size
   */
  int window_;

  /**
   * Number of retry
   */
  int nb_retry_;

  /** 
   * The scheduler to inform about timeout
   */
  Mac802_16 *mac_;

  /**
   * The packet to send when the backoff expires
   */
  Packet *p_;

  /**
   * Pointer to next in the list
   */
  LIST_ENTRY(ContentionRequest) link;
  //LIST_ENTRY(ContentionRequest); //for magic draw
};

/**
 * Class to handle ranging opportunities
 */
/*
class RangingRequest: public ContentionRequest 
{
 public:

  RangingRequest (ContentionSlot *s, Packet *p);

  void expire ();

 private:
};
*/

/**
 * Class to handle cdma-initial ranging opportunities
 */
class RngRequest: public ContentionRequest
{
 public:

  /**
   * Creates a contention slot for the given frame
   * @param frame The frame map
   */
    RngRequest (ContentionSlot *s, Packet *p, int cid, int length, int backoff, int timeout, int nbretry, int window, int code, int top, int flagtransmit, int flagnowtransmit, int addr);

  /**
   * Called when timeout expired
   */
  void expire ();

  /**
   * Get and set cdma initial ranging parameters, cid, addr, backoff, timeout, nbretry, code, top, flag_tranmit, flag_now_transmit, cdma-packet
   */
  inline int getCID ()     { return cid_; }
  inline int getADDR ()    { return addr_; }
  inline int getBACKOFF () { return s_backoff_; }
  inline int getTIMEOUT () { return s_timeout_; }
  inline int getNBRETRY () { return s_nbretry_; }
  inline int getWINDOW ()  { return s_window_; }
  inline int getCODE ()    { return s_code_; }
  inline int getTOP ()    { return s_top_; }
  inline int getFLAGTRANSMIT ()     { return s_flagtransmit_; }
  inline int getFLAGNOWTRANSMIT ()  { return s_flagnowtransmit_; }
  inline Packet * getPacket_P ()    { return pnew_; }
  inline Packet * getPacket_P_mac (){ return pnew_; }

  inline void setCID (int cid)         { cid_ = cid; }
  inline void setBACKOFF (int backoff) { s_backoff_ = backoff; }
  inline void setTIMEOUT (int timeout) { s_timeout_ = timeout; }
  inline void setNBRETRY (int nbretry) { s_nbretry_ = nbretry; }
  inline void setWINDOW (int window)   { s_window_ = window; }
  inline void setCODE (int code)       { s_code_ = code; }
  inline void setTOP (int top)       { s_top_ = top; }
  inline void setFLAGTRANSMIT  (int flagtransmit)       { s_flagtransmit_ = flagtransmit; }
  inline void setFLAGNOWTRANSMIT  (int flagnowtransmit) { s_flagnowtransmit_ = flagnowtransmit; }
  inline void setPacket_P_mac  (Packet * p)             { *pnew_ = *p; }

 private:
  /**
   * CDMA-INIT-RANGING variables
   */
  int cid_;
  int addr_;
  int s_backoff_;
  int s_timeout_;
  int s_nbretry_;
  int s_window_;
  int s_code_;
  int s_top_;
  int s_flagtransmit_;
  int s_flagnowtransmit_;
  Packet *pnew_;
  int size_;

};

/**
 * Class to handle cdma bandwidth request opportunities
 */
class BwRequest: public ContentionRequest 
{
 public:

  /**
   * Creates a contention slot for the given frame
   * @param frame The frame map 
   */
//  BwRequest (ContentionSlot *s, Packet *p, int cid, int length);
    BwRequest (ContentionSlot *s, Packet *p, int cid, int length, int backoff, int timeout, int nbretry, int window, int code, int top, int flagtransmit, int flagnowtransmit);

  /**
   * Called when timeout expired
   */
  void expire ();

  /**
   * Get and set cdma bandwidth request parameters, cid, addr, backoff, timeout, nbretry, code, top, flag_tranmit, flag_now_transmit, cdma-packet
   */
  inline int getCID ()     { return cid_; }
  inline int getBACKOFF () { return s_backoff_; }
  inline int getTIMEOUT () { return s_timeout_; }
  inline int getNBRETRY () { return s_nbretry_; }
  inline int getWINDOW ()  { return s_window_; }
  inline int getCODE ()    { return s_code_; }
  inline int getTOP ()    { return s_top_; }
  inline int getFLAGTRANSMIT ()     { return s_flagtransmit_; }
  inline int getFLAGNOWTRANSMIT ()  { return s_flagnowtransmit_; }
  inline Packet * getPacket_P ()    { return pnew_; }

  inline void setBACKOFF (int backoff) { s_backoff_ = backoff; }
  inline void setTIMEOUT (int timeout) { s_timeout_ = timeout; }
  inline void setNBRETRY (int nbretry) { s_nbretry_ = nbretry; }
  inline void setWINDOW (int window)   { s_window_ = window; }
  inline void setCODE (int code)       { s_code_ = code; }
  inline void setTOP (int top)       { s_top_ = top; }
  inline void setFLAGTRANSMIT  (int flagtransmit)       { s_flagtransmit_ = flagtransmit; }
  inline void setFLAGNOWTRANSMIT  (int flagnowtransmit) { s_flagnowtransmit_ = flagnowtransmit; }
  inline void setPacket_P  (Packet * p)                 { *pnew_ = *p; }

 private:
  /**
   * The CID for the request
   */
  int cid_;

  /**
   * CDMA-BW-REQ variables
   */
  int s_backoff_;
  int s_timeout_;
  int s_nbretry_;
  int s_window_;
  int s_code_;
  int s_top_;
  int s_flagtransmit_;
  int s_flagnowtransmit_;
  Packet *pnew_;

  /**
   * The size in bytes of the bandwidth requested
   */
  int size_;
};

#endif

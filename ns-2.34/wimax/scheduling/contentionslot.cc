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

#include "contentionslot.h"
#include <random.h>
#include <math.h>

/*
 * Creates a contention slot for the given frame
 * @param frame The frame map 
 */
ContentionSlot::ContentionSlot (FrameMap *map) 
{
  assert (map);
  map_ = map;
}

/**
 * Destructor
 */
ContentionSlot::~ContentionSlot() {}

/*
 * Set the initial contention slot window size
 * @param backoff_start the initial contention slot window size
 */
void ContentionSlot::setBackoff_start( int backoff_start ) 
{ 
  backoff_start_ = backoff_start;
}

/*
 * Set the final contention slot window size
 * @param backoff_stop the final contention slot window size 
 */
void ContentionSlot::setBackoff_stop( int backoff_stop ) 
{ 
  backoff_stop_ = backoff_stop;
}

/**
 * Resume the timers for the requests
 */
void ContentionSlot::resumeTimers (double duration) {}

/**
 * Pause the timers for the requests
 */
void ContentionSlot::pauseTimers () {}

/**** Methods for Ranging Contention slot ****/

/*
 * Creates a contention slot for the given frame
 * @param frame The frame map 
 */
/*
RngContentionSlot::RngContentionSlot (FrameMap *map) : ContentionSlot (map)
{
  request_ = NULL;
}
*/

/**
 * Destructor
 */
/*
RngContentionSlot::~RngContentionSlot() 
{
  if (request_)
    delete request_;
}
*/


/*
 * Add a ranging request
 * @param p The packet to be sent during the ranging opportunity
 */
/*
void RngContentionSlot::addRequest (Packet *p)
{
  assert (request_ == NULL);
  request_ = new RangingRequest (this, p);
}
*/


/*
 * Remove the pending request
 */
/*
void RngContentionSlot::removeRequest ()
{
  //assert (request_);
  if (request_) {
    delete request_;
    request_ = NULL;
  }
}
*/

/**
 * Resume the timers for the requests
 */
/*
void RngContentionSlot::resumeTimers (double duration)
{
  if (request_)
    request_->resume(duration);
}
*/

/**
 * Pause the timers for the requests
 */
/*
void RngContentionSlot::pauseTimers ()
{
  if (request_)
    request_->pause();
}
*/

/**** Methods for CDMA-INITIAL Contention slot ****/
/*
 * Creates a contention slot for the given frame
 * @param frame The frame map
 */
RngContentionSlot::RngContentionSlot (FrameMap *map) : ContentionSlot (map)
{
  LIST_INIT (&request_list_);
}

/**
 * Destructor
 */
RngContentionSlot::~RngContentionSlot() {}

void RngContentionSlot::addRequest (Packet *p, int cid, int len, int backoff, int timeout, int nbretry, int window, int code, int top, int flagtransmit, int flagnowtransmit, int addr)
{
  RngRequest *b = new RngRequest (this, p, cid, len, backoff, timeout, nbretry, window, code, top, flagtransmit, flagnowtransmit, addr);
  b->insert_entry_head (&request_list_);
}


void RngContentionSlot::removeRequest_mac (int addr)
{
  RngRequest *b = getRequest_mac (addr);
  if (b!=NULL) {
    b->remove_entry ();
    delete b;
  }
}

/*
 * Remove all pending request
 */
void RngContentionSlot::removeRequests ()
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)request_list_.lh_first) {
    c->remove_entry();
    delete c;
  }
}

/*
 * Return request for cdma intial ranging request
 */
RngRequest* RngContentionSlot::getRequest_mac (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c;
  }
  return NULL;
}

/*
 * Return packet for cdma initial ranging request
 */
Packet * RngContentionSlot::getPacket_P_mac (int addr)
{  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getPacket_P_mac();
  }
  return NULL;
}

/*
 * Return cid for cdma intial ranging request
 */
int RngContentionSlot::getCID (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getCID();
  }
  return -1;
}

/*
 * Return backoff for cdma intial ranging request
 */
int RngContentionSlot::getBACKOFF (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getBACKOFF();
  }
  return -1;
}

/*
 * Return timeout for cdma intial ranging request
 */
int RngContentionSlot::getTIMEOUT (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getTIMEOUT();
  }
  return -1;
}

/*
 * Return number of retry for cdma intial ranging request
 */
int RngContentionSlot::getNBRETRY (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getNBRETRY();
  }
  return -1;
}

/*
 * Return window for cdma intial ranging request
 */
int RngContentionSlot::getWINDOW (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getWINDOW();
  }
  return -1;
}

/*
 * Return code for cdma intial ranging request
 */
int RngContentionSlot::getCODE (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getCODE();
  }
  return -1;
}

/*
 * Return top for cdma intial ranging request
 */
int RngContentionSlot::getTOP (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getTOP();
  }
  return -1;
}

/*
 * Return flag_transmit for cdma intial ranging request
 */
int RngContentionSlot::getFLAGTRANSMIT (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getFLAGTRANSMIT();
  }
  return -1;
}

/*
 * Return flag_now_transmit (within a current frame) for cdma intial ranging request
 */
int RngContentionSlot::getFLAGNOWTRANSMIT (int addr)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) return c->getFLAGNOWTRANSMIT();
  }
  return -1;
}

/*
 * Set backoff for cdma intial ranging request
 */
void RngContentionSlot::setBACKOFF (int addr, int backoff)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setBACKOFF(backoff);
  }
}

/*
 * Set cid for cdma intial ranging request
 */
void RngContentionSlot::setCID (int addr, int cid)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setCID(cid);
  }
}

/*
 * Set timeout for cdma intial ranging request
 */
void RngContentionSlot::setTIMEOUT (int addr, int timeout)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setTIMEOUT(timeout);
  }
}

/*
 * Set number of retry for cdma intial ranging request
 */
void RngContentionSlot::setNBRETRY (int addr, int nbretry)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setNBRETRY(nbretry);
  }
}

/*
 * Set window for cdma intial ranging request
 */
void RngContentionSlot::setWINDOW (int addr, int window)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setWINDOW(window);
  }
}

/*
 * Set code for cdma intial ranging request
 */
void RngContentionSlot::setCODE (int addr, int code)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setCODE(code);
  }
}

/*
 * Set top for cdma intial ranging request
 */
void RngContentionSlot::setTOP (int addr, int top)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setTOP(top);
  }
}

/*
 * Set flag_transmit for cdma intial ranging request
 */
void RngContentionSlot::setFLAGTRANSMIT (int addr, int flagtransmit)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setFLAGTRANSMIT(flagtransmit);
  }
}

/*
 * Set flag_now_transmit (within a current frame) for cdma intial ranging request
 */
void RngContentionSlot::setFLAGNOWTRANSMIT (int addr, int flagnowtransmit)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setFLAGNOWTRANSMIT(flagnowtransmit);
  }
}

/*
 * Set packet for cdma intial ranging request
 */
void RngContentionSlot::setPacket_P_mac (int addr, Packet * p)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
    if (c->getADDR()==addr) c->setPacket_P_mac(p);
  }
}

/**
 * Resume the timers for the requests
 */
void RngContentionSlot::resumeTimers (double duration)
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
      c->resume(duration);
  }
}

/**
 * Pause the timers for the requests
 */
void RngContentionSlot::pauseTimers ()
{
  for (RngRequest *c = (RngRequest *)request_list_.lh_first; c ; c=(RngRequest *)(c->next_entry())) {
      c->pause();
  }
}

/**** Methods for CDAM Bandwidth Contention slot ****/
/*
 * Creates a contention slot for the given frame
 * @param frame The frame map 
 */
BwContentionSlot::BwContentionSlot (FrameMap *map) : ContentionSlot (map)
{
  LIST_INIT (&request_list_);
}

/**
 * Destructor
 */
BwContentionSlot::~BwContentionSlot() {}

/*
 * Add a bandwidth request
 * @param p The packet to be sent during the ranging opportunity
 * @param cid The CID of the bandwidth request
 * @param len The size in bytes of the bandwidth request
 */
/*
void BwContentionSlot::addRequest (Packet *p, int cid, int len)
{
  assert (getRequest (cid)==NULL);
  BwRequest *b = new BwRequest (this, p, cid, len);
  b->insert_entry_head (&request_list_);
}
*/
void BwContentionSlot::addRequest (Packet *p, int cid, int len, int backoff, int timeout, int nbretry, int window, int code, int top, int flagtransmit, int flagnowtransmit)
{
  assert (getRequest (cid)==NULL);
  BwRequest *b = new BwRequest (this, p, cid, len, backoff, timeout, nbretry, window, code, top, flagtransmit, flagnowtransmit);
  b->insert_entry_head (&request_list_);
}


/*
 * Remove the pending request
 */
void BwContentionSlot::removeRequest (int cid)
{
  BwRequest *b = getRequest (cid);
  if (b!=NULL) {
    b->remove_entry ();
    delete b;
  }
}

/*
 * Remove all pending request
 */
void BwContentionSlot::removeRequests ()
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)request_list_.lh_first) {
    c->remove_entry();
    delete c;
  }
}


/*
 * Get the request for the given CID
 * @param cid The CID for the request
 */
BwRequest* BwContentionSlot::getRequest (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c;
  }
  return NULL;
}

/*
 * Return packet for cdma bw-req given cid
 */
Packet * BwContentionSlot::getPacket_P (int cid)
{  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getPacket_P(); 
  }
  return NULL;
}

/*
 * Return backoff for cdma bw-req given cid
 */
int BwContentionSlot::getBACKOFF (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getBACKOFF();
  }
  return -1;
}   
    
/*
 * Return timeout for cdma bw-req given cid
 */
int BwContentionSlot::getTIMEOUT (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getTIMEOUT();
  }
  return -1;
}

/*
 * Return number of retry for cdma bw-req given cid
 */
int BwContentionSlot::getNBRETRY (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getNBRETRY();
  }
  return -1;
}

/*
 * Return window for cdma bw-req given cid
 */
int BwContentionSlot::getWINDOW (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getWINDOW();
  }
  return -1;
}

/*
 * Return code for cdma bw-req given cid
 */
int BwContentionSlot::getCODE (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getCODE();
  }
  return -1;
}

/*
 * Return top for cdma bw-req given cid
 */
int BwContentionSlot::getTOP (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getTOP();
  }
  return -1;
}

/*
 * Return flag_transmit for cdma bw-req given cid
 */
int BwContentionSlot::getFLAGTRANSMIT (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getFLAGTRANSMIT();
  }
  return -1;
}

/*
 * Return flag_now_transmit (within a current frame) for cdma bw-req given cid
 */
int BwContentionSlot::getFLAGNOWTRANSMIT (int cid)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid)
      return c->getFLAGNOWTRANSMIT();
  }
  return -1;
}

/*
 * Set backoff for cdma bw-req given cid
 */
void BwContentionSlot::setBACKOFF (int cid, int backoff)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setBACKOFF(backoff);
    }
  }
}

/*
 * Set timeout for cdma bw-req given cid
 */
void BwContentionSlot::setTIMEOUT (int cid, int timeout)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setTIMEOUT(timeout);
    }
  }
}

/*
 * Set number of retry for cdma bw-req given cid
 */
void BwContentionSlot::setNBRETRY (int cid, int nbretry)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setNBRETRY(nbretry);
    }
  }
}

/*
 * Set window for cdma bw-req given cid
 */
void BwContentionSlot::setWINDOW (int cid, int window)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setWINDOW(window);
    }
  }
}

/*
 * Set code for cdma bw-req given cid
 */
void BwContentionSlot::setCODE (int cid, int code)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setCODE(code);
    }
  }
}

/*
 * Set top for cdma bw-req given cid
 */
void BwContentionSlot::setTOP (int cid, int top)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setTOP(top);
    }
  }
}

/*
 * Set flag_transmit for cdma bw-req given cid
 */
void BwContentionSlot::setFLAGTRANSMIT (int cid, int flagtransmit)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setFLAGTRANSMIT(flagtransmit);
    }
  }
}

/*
 * Set flag_now_transmit (within a current frame) for cdma bw-req given cid
 */
void BwContentionSlot::setFLAGNOWTRANSMIT (int cid, int flagnowtransmit)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setFLAGNOWTRANSMIT(flagnowtransmit);
    }
  }
}

/*
 * Set packet for cdma bw-req given cid
 */
void BwContentionSlot::setPacket_P (int cid, Packet * p)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
    if (c->getCID()==cid) {
      c->setPacket_P(p);
    }
  }
}

/**
 * Resume the timers for the requests
 */
void BwContentionSlot::resumeTimers (double duration)
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
      c->resume(duration);
  }
}

/**
 * Pause the timers for the requests
 */
void BwContentionSlot::pauseTimers ()
{
  for (BwRequest *c = (BwRequest *)request_list_.lh_first; c ; c=(BwRequest *)(c->next_entry())) {
      c->pause();
  }
}

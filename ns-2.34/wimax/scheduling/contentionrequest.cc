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

//#include "contentionrequest.h"
#include "contentionslot.h"
#include "framemap.h"
#include "wimaxscheduler.h"
#include "random.h"
#include "mac802_16SS.h"

/*
 * Handling function for WimaxFrameTimer
 * @param e The event that occured
 */
void WimaxBackoffTimer::handle(Event *e)
{
  busy_ = 0;
  paused_ = 0;
  stime = 0.0;
  rtime = 0.0;

  HDR_CMN(c_->p_)->timestamp() = NOW; //add timestamp since it bypasses the queue
  mac->transmit (c_->p_->copy());

  //start timeout trigger
  c_->starttimeout ();
}

void WimaxBackoffTimer::pause()
{
	Scheduler &s = Scheduler::instance();

	//the caculation below make validation pass for linux though it
	// looks dummy

	double st = s.clock();
	double rt = stime;
	double sr = st - rt;

	assert(busy_ && ! paused_);

	paused_ = 1;
	rtime -= sr;

	assert(rtime >= 0.0);

	s.cancel(&intr);
}

void WimaxBackoffTimer::resume()
{
	Scheduler &s = Scheduler::instance();

	assert(busy_ && paused_);

	paused_ = 0;
	stime = s.clock();

	assert(rtime >= 0.0);
       	s.schedule(this, &intr, rtime);
}

/*
 * Handling function for WimaxFrameTimer
 * @param e The event that occured
 */
void ContCountTimer::handle(Event *e)
{
  //clear state
  busy_ = 0;
  paused_ = 0;
  stime = 0.0;
  rtime = 0.0;

  c_->pause ();
}

/*
 * Creates a contention slot for the given frame
 * @param s The contention slot 
 * @param p The packet to send
 */
ContentionRequest::ContentionRequest (ContentionSlot *s, Packet *p)
{
  assert (s);
  assert (p);
  s_=s;
  mac_ = s_->map_->getMac();
  window_ = s_->getBackoff_start();
  nb_retry_ = 0;
  p_=p;
  backoff_timer_ = new WimaxBackoffTimer (this, mac_);
  count_timer_ = new ContCountTimer (this, mac_);
  timeout_timer_ = new ContentionTimer (this);
  int result = Random::random() % ((int)(pow (2, window_)+1));
  
/////////We do not use this anymore => counter-based instead
  int ps_per_symbol = floor(mac_->getPhy()->getSymbolTime()/mac_->getPhy()->getPS());
  int contention_size_ = 5;

  if (s_->getSize() == 116) {
//  	int contention_size_ = s_->get_init_contention_Size();
  	int which_frame = floor(result/contention_size_);

//  	backoff_timer_->start (result*(ps_per_symbol*2/5)*mac_->getPhy()->getPS());
//        backoff_timer_->start (which_frame*ps_per_symbol*2*mac_->getPhy()->getPS());
//still OFDM
  	backoff_timer_->start (result*s_->getSize()*mac_->getPhy()->getPS());
//  	debug2 (" SS %d, At %f Start Contention_request (Initial Ranging) in %e, Backoff_rand(rand() mod (pow(2,%d)+1)) :%d, Contention_size :%d, PS :%e\n", mac_->addr(), NOW, result*s_->getSize()*mac_->getPhy()->getPS(), window_, result, contention_size_, mac_->getPhy()->getPS());

//  	debug2 (" SS %d, At %f Start Contention_request (Initial Ranging) in %e, Symbol time :%e, Contention Size :%d, Backoff_rand(ran() mod (pow(2,%d)+1)) :%d, Packet Size :%d, PS :%e, which_frame :%d\n", mac_->addr(), NOW, which_frame*ps_per_symbol*2*mac_->getPhy()->getPS(), mac_->getPhy()->getSymbolTime(), contention_size_, window_, result, s_->getSize(), mac_->getPhy()->getPS(), which_frame);
  } else {
//  	int contention_size_ = s_->get_bw_req_contention_Size();
  	int which_frame = ceil(result/contention_size_);

        backoff_timer_->start (which_frame*ps_per_symbol*mac_->getPhy()->getPS());
//  	debug2 (" SS %d, At %f Start Contention_request (BW-REQ Ranging) in %e, Symbol time :%e, Contention Size :%d, Backoff_rand(ran() mod (pow(2,%d)+1)) :%d, Packet Size :%d, PS :%e, which_frame :%d\n", mac_->addr(), NOW, which_frame*ps_per_symbol*mac_->getPhy()->getPS(), mac_->getPhy()->getSymbolTime(), contention_size_, window_, result, s_->getSize(), mac_->getPhy()->getPS(), which_frame);
  }
//////////

/*
//roll back to previous versions
  debug2 (" SS %d, At %f Start Contention_request (Ranging and BW) in %e (backoff*content_size*getPS), Backoff_rand(ran() mod (pow(2,%d)+1)) :%d, Contention_size :%d, PS :%e\n", mac_->addr(), NOW, result*s_->getSize()*mac_->getPhy()->getPS(),window_, result,s_->getSize(), mac_->getPhy()->getPS());

  backoff_timer_->start (result*s_->getSize()*mac_->getPhy()->getPS());
*/

  backoff_timer_->pause();
}

ContentionRequest::~ContentionRequest ()
{
  //printf ("canceling timeout\n");
  //the timeout timer need not be triggered 
  //this can happen when the STA received bw allocation
  //when it is not waiting for one (or it's still in backoff)
  if (timeout_timer_->status()==TIMER_PENDING)
    timeout_timer_->cancel();
  if (backoff_timer_->busy())
    backoff_timer_->stop();
  delete backoff_timer_;
  if (count_timer_->busy())
    count_timer_->stop();
  delete count_timer_;
  if (timeout_timer_->status()== TIMER_IDLE)
    delete timeout_timer_; //cannot delete it the timer is executing (Note: small memory leak to fix)
  assert (p_);
  Packet:: free (p_);
}

/*
 * Called when timeout expired
 */
void ContentionRequest::expire ()
{

}

/*
 * Called when timeout expired
 */
void ContentionRequest::starttimeout ()
{
  timeout_timer_->sched (timeout_);
}

/* 
 * Pause the backoff timer
 */
void ContentionRequest::pause () 
{
  if (backoff_timer_->busy() && !backoff_timer_->paused())
    backoff_timer_->pause(); 
}

/*
 * Resume the backoff timer
 * @param duration The duration for resuming count down before going back
 *        to idle. The duration equals the allocation.
 */
void ContentionRequest::resume (double duration) 
{ 
  if (backoff_timer_->paused() && timeout_timer_->status()==TIMER_IDLE)
    backoff_timer_->resume(); 
  count_timer_->start (duration);
}

/*
 * Creates a contention slot for the given frame
 * @param frame The frame map 
 */
/*
RangingRequest::RangingRequest (ContentionSlot *s, Packet *p) : ContentionRequest (s,p)
{
  type_ = WimaxT3TimerID;
  timeout_ = mac_->macmib_.t3_timeout;
}
*/


RngRequest::RngRequest (ContentionSlot *s, Packet *p, int cid, int len, int backoff, int timeout, int nbretry, int window, int code, int top, int flagtransmit, int flagnowtransmit, int addr) : ContentionRequest (s,p)
{
  type_ = WimaxT3TimerID;
  timeout_ = mac_->macmib_.t3_timeout;
  cid_ = cid;
  addr_ = addr;
  size_ = len;
  s_backoff_ = backoff;
  s_timeout_ = timeout;
  s_nbretry_ = nbretry;
  s_window_ = window;
  s_code_ = code;
  s_top_ = top;
  s_flagtransmit_ = flagtransmit;
  s_flagnowtransmit_ = flagnowtransmit;
  pnew_ = p; 
}

/*
 * Called when timeout expired
 */
void RngRequest::expire ()
{
  mac_->debug ("Ranging request expires\n");
  if (nb_retry_ == (int)mac_->macmib_.contention_rng_retry) {
    //max retries reached, inform the scheduler
    mac_->expire (type_);
  } else {
    if (window_ < s_->getBackoff_stop())
      window_++;
    nb_retry_++;
    int result = Random::random() % ((int)(pow (2, window_)+1));

/*
//////////We do not use it anymore => counter-based 
  int contention_size_ = 5;
//Modifed later
//  int contention_size_ = s_->get_init_contention_Size();
  int ps_per_symbol = floor(mac_->getPhy()->getSymbolTime()/mac_->getPhy()->getPS());
  int which_frame = floor(result/contention_size_);

//  backoff_timer_->start (result*(ps_per_symbol*2/5)*mac_->getPhy()->getPS());
    backoff_timer_->start (which_frame*ps_per_symbol*2*mac_->getPhy()->getPS());

  debug2 (" Start Init Ranging Contention in %e, Contention Size :%d, Backoff_rand(ran() mod (pow(2,%d)+1)) :%d, Packet Size :%d, window :%d, new_nb_retry :%d, which_frame :%d\n", which_frame*ps_per_symbol*2*mac_->getPhy()->getPS(), contention_size_, window_, result, s_->getSize(), window_, nb_retry_, which_frame);
/////////
*/

//roll back to previous version
    mac_->debug (" Start Init Ranging Contention in %e (backoff :%d, size :%d, ps :%e)\n", result*s_->getSize()*mac_->getPhy()->getPS(),result,s_->getSize(),mac_->getPhy()->getPS());
    backoff_timer_->start (result*s_->getSize()*mac_->getPhy()->getPS());
//
    backoff_timer_->pause();
  }
}

/*
 * Creates a contention slot for the given frame
 * @param frame The frame map 
 */
/*
BwRequest::BwRequest (ContentionSlot *s, Packet *p, int cid, int len) : ContentionRequest (s,p)
{
  type_ = WimaxT16TimerID;
  timeout_ = mac_->macmib_.t16_timeout;
  cid_ = cid;
  size_ = len;
}
*/
BwRequest::BwRequest (ContentionSlot *s, Packet *p, int cid, int len, int backoff, int timeout, int nbretry, int window, int code, int top, int flagtransmit, int flagnowtransmit) : ContentionRequest (s,p)
{ 
  type_ = WimaxT16TimerID;
  timeout_ = mac_->macmib_.t16_timeout;
  cid_ = cid;
  size_ = len; 
  s_backoff_ = backoff;
  s_timeout_ = timeout;
  s_nbretry_ = nbretry;
  s_window_ = window;
  s_code_ = code;
  s_top_ = top;
  s_flagtransmit_ = flagtransmit;
  s_flagnowtransmit_ = flagnowtransmit;
  pnew_ = p;
}

/*
 * Called when timeout expired
 */ 
void BwRequest::expire ()
{
  printf ("At %f in Mac %d Bw request expires (%d/%d)\n", NOW, mac_->addr(), nb_retry_, (int)mac_->macmib_.request_retry);
  if (nb_retry_ == (int)mac_->macmib_.request_retry) {
    //max retries reached, delete the pdu that were waiting
    Connection *c = mac_->getCManager()->get_connection (cid_, true);
    int len = 0;
    debug2 ("Dropping packet because bw req exceeded\n");
    while (len < size_) {
      Packet *p = c->dequeue();
      assert (p);
      len += HDR_CMN(p)->size();
      //We want to know when the packet is dropped. Create a new entry
      ((Mac802_16SS*)mac_)->drop(p, "BWR");
      //Packet::free (p);
    }
    //must remove the request from the list
    ((BwContentionSlot*)s_)->removeRequest (cid_);
  } else {
    if (window_ < s_->getBackoff_stop())
      window_++;
    nb_retry_++;
    int result = Random::random() % ((int)(pow (2, window_)+1));

/////////We do not use it anymore => counter-based
    int contention_size_ = 5;
//Modifed later
//    int contention_size_ = s_->get_bw_req_contention_Size();
    int ps_per_symbol = floor(mac_->getPhy()->getSymbolTime()/mac_->getPhy()->getPS());

    int which_frame = floor(result/contention_size_);
//    backoff_timer_->start (result*(ps_per_symbol/5)*mac_->getPhy()->getPS());
    backoff_timer_->start (which_frame*ps_per_symbol*mac_->getPhy()->getPS());

    debug2 (" Start BW-REQ Ranging Contention in %e, Contention Size :%d, Backoff_rand(ran() mod (pow(2,%d)+1)) :%d, Packet Size :%d, window :%d, new_nb_retry :%d, which_frame :%d\n", which_frame*ps_per_symbol*mac_->getPhy()->getPS(), contention_size_, window_, result, s_->getSize(), window_, nb_retry_, which_frame);
/////////

/*
//roll back to previous version
    debug2 ("Start BW contention in %f(backoff=%d, size=%d, ps=%f)\n", result*s_->getSize()*mac_->getPhy()->getPS(),result,s_->getSize(),mac_->getPhy()->getPS());
    backoff_timer_->start (result*s_->getSize()*mac_->getPhy()->getPS());
*/
    backoff_timer_->pause();
  }
}

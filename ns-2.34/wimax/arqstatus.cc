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
#include "arqstatus.h"
#include "packet.h"
#include "mac802_16.h"

/* Maximum Sequence number for ARQ*/
#define MAX_SEQ 2048 

/* 
 * Create an Arq Status object  
 * @param 
 */
Arqstatus::Arqstatus (): arq_retrans_time_(0), ack_seq_(0), ack_counter_(0),
			 arq_curr_seq_(0), ack_period_(0), arq_max_seq_(0),
			 arq_max_window_(0), arq_curr_window_(0), last_ack_sent_(0) 
{
  arq_trans_queue_ = new PacketQueue;
  arq_retrans_queue_ = new PacketQueue;
  arq_feedback_queue_ = new PacketQueue;
  arq_max_seq_ = MAX_SEQ;
	
}

/* 
 * Create an Arq Status object  
 * @param 
 */
Arqstatus::~Arqstatus ()
{
  if (arq_trans_queue_) delete arq_trans_queue_;
  if (arq_retrans_queue_) delete arq_retrans_queue_;
  if (arq_feedback_queue_) delete arq_feedback_queue_;

  delete arqRetransTimer;  /*RPI*/
}

void Arqstatus::arqTimerHandler ()
{
  Packet *ph, *p, *pac;
  bool dont_queue = false, dont_queue_retrans = false;
  Connection* connection = arqRetransTimer-> getConnection ();
  debug2("ARQ Timer Expire for Connection Id: %d \n", connection->get_cid());
  arq_trans_queue_->resetIterator ();
  // send all packets with active timers to retransmission queue
  ph = arq_trans_queue_->getNext();

  debug2 ("  ARQ: con %d qlength=%d arqTx=%d arqRtx=%d\n", connection->get_cid(), connection->get_queue ()->length(), arq_trans_queue_->length(), arq_retrans_queue_->length());

  while (ph) {
    connection->get_queue ()->resetIterator ();
    p = connection->get_queue ()->getNext ();
    while(p)
      {
	if(connection->isPackingEnable ())
	  {
	    if(HDR_MAC802_16(p)->pack_subheader.sn == HDR_MAC802_16(ph)->pack_subheader.sn){
	      debug2("  ARQ Timer handler, NOT Enqueueing packet in Retransmission queue Seq No : %d\n ",HDR_MAC802_16(p)->pack_subheader.sn);	
	      dont_queue = true;
	      break;	
	    }		
	  }
	else 
	  {
	    if(HDR_MAC802_16(p)->frag_subheader.sn == HDR_MAC802_16(ph)->frag_subheader.sn){
	      dont_queue = true;
	      break;	
	    }
	  }
	p = connection->get_queue ()->getNext (); 
      }

    arq_retrans_queue_->resetIterator ();
    pac = arq_retrans_queue_->getNext();
    while(pac)
      {
	if(connection->isPackingEnable ())
	  {
	    if(HDR_MAC802_16(pac)->pack_subheader.sn == HDR_MAC802_16(ph)->pack_subheader.sn){
	      debug2("  ARQ Timer handler, NOT Enqueueing packet in Retransmission queue, already in Retrans queue Seq No : %d\n ",HDR_MAC802_16(pac)->pack_subheader.sn);	
	      dont_queue_retrans = true;
	      break;	
	    }		
	  }
	else 
	  {
	    if(HDR_MAC802_16(pac)->frag_subheader.sn == HDR_MAC802_16(ph)->frag_subheader.sn){
	      dont_queue_retrans = true;
	      break;	
	    }
	  }
	pac =  arq_retrans_queue_->getNext (); 
      }
		
    if((!dont_queue) && (!dont_queue_retrans)){
      debug2("  ARQ Timer handler, Enqueueing packet in Retransmission queue Seq No : %d \n",HDR_MAC802_16(ph)->pack_subheader.sn);
      arq_retrans_queue_->enque(ph->copy());
    }
    dont_queue = false;
    dont_queue_retrans = false;
    ph = arq_trans_queue_->getNext();
  }
  //schedule timer based on retransmission time setting
  debug2 ("  ARQ: con %d qlength=%d arqTx=%d arqRtx=%d\n", connection->get_cid(), connection->get_queue ()->length(), arq_trans_queue_->length(), arq_retrans_queue_->length());
  arqRetransTimer->resched(arq_retrans_time_);
  debug2("  ARQ Timer Re-Set\n");			
}

void  Arqstatus::arqSend(Packet* p, Connection *connection, fragment_status status)
{
  hdr_mac802_16 * mh = HDR_MAC802_16(p);
  int cid = connection->get_cid ();
  // If Packing is enabled the ARQ and Fragmentation information is contained in the packing subheader
  if(connection->isPackingEnable () == true){ 
    mh->pack_subheader.sn = arq_curr_seq_;
    mh->pack_subheader.fc = status;
    debug2("ARQ Send in packing subheader: cid:%d curr  seq:%d fragment_status : %d\n",cid, mh->pack_subheader.sn, status);
  } else {
    mh->frag_subheader.sn = arq_curr_seq_;
    mh->frag_subheader.fc = status;
    debug2("ARQ Send in fragmentation subheader: cid:%d curr  seq:%d fragment_status : %d\n",cid, mh->frag_subheader.sn, status);
  }
  arq_curr_seq_++;
  if (arq_curr_seq_ == arq_max_seq_) {
    arq_curr_seq_ = 0;
  }
  // Add packet to tranmitted queue
  arq_trans_queue_->enque(p->copy());
  debug2("ARQ Send: added to  ARQ Transmission queue, length: %d \n",arq_trans_queue_->length());
}


void Arqstatus::arqReceive(Packet* p, Connection *connection, u_int8_t* inOrder)
{

  // Read the header to find out the flow the packet belongs to
  hdr_mac802_16 * mh = HDR_MAC802_16(p);
  u_int32_t flowno = mh->header.cid;
  u_int32_t seqno;
  if(connection->isPackingEnable () == true){  
    seqno = mh->pack_subheader.sn;
  } 
  else {
    seqno = mh->frag_subheader.sn;
  }
  debug2("ARQ: Receive: ack_seq_ = %d ; seqno = %d ; ack_counter_ = %d ; ack_period_ = %d\n", ack_seq_, seqno, ack_counter_, ack_period_);
  // Read sequence number and check for in order packet
  // if out of order, free the packet and reset inOrder variable
  if (ack_seq_ != seqno) {
    //if((((seqno - ack_seq_) > 0) && ((seqno - ack_seq_) <= arq_max_window_)) 
    //|| ((seqno >= 0 && seqno < arq_max_window_) && (ack_seq_ < MAX_SEQ && ack_seq_ >= (MAX_SEQ - arq_max_window_))))
    if ( (((seqno - ack_seq_) > 0) && ((seqno - ack_seq_) <= arq_max_window_))
	 || ((seqno - ack_seq_< 0) && (seqno+MAX_SEQ - ack_seq_ <= arq_max_window_)) )
      {
	*inOrder = 0;
	debug2("ARQ receive: out of order - in arq receive ack_seq_ : %d seq_no. in packet : %d\n", ack_seq_, seqno);
      }
    else
      {
	*inOrder = 2;
	debug2("ARQ receive: No need to buffer packet - in arq receive ack_seq_ : %d seq_no. in packet : %d\n", ack_seq_, seqno);
      }	  
  } 
  // if in order, increment the feedback sequence number
  if (ack_seq_ == seqno) { 
    ack_seq_++;
    if (ack_seq_ == arq_max_seq_) {
      ack_seq_ = 0;
    }
    debug2("ARQ Receive: in order: flow:%d \t seq no: %d\t ack_seq=%d\n",flowno,seqno,ack_seq_); 
  }
  //Begin RPI
  // increment the ACK counter and check if time to create and enque feedback IE  
  ack_counter_ ++;
  if (ack_counter_ == ack_period_) { 				// if time to send ACK
    ack_counter_ = 0;
    if ((*inOrder == 1)) {  				//Case of Cumulative acknowledgement
      //if((((ack_seq_ - seqno) > 0) && ((ack_seq_ -  seqno) <= arq_max_window_)) || ((ack_seq_ >= 0 && ack_seq_ < arq_max_window_) && (seqno < MAX_SEQ && seqno >= (MAX_SEQ - arq_max_window_))))
      if ( (((ack_seq_ - seqno) > 0) && ((ack_seq_ - seqno) <= arq_max_window_))
	   || ((ack_seq_- seqno < 0) && (ack_seq_+MAX_SEQ - seqno <= arq_max_window_)) )
	{
	  debug2("ARQ Receive: Enqueueing a cumulative acknowledgement \n");
	  // Let the cumulative acknowledgement always occupy the first slot and here we will fill only ARQ IE information
	  Packet* arqfb = arq_feedback_queue_->tail ();
	  if(arqfb && (HDR_MAC802_16(arqfb)->num_of_acks <= MAX_NUM_ACK)) {
	    arq_feedback_queue_->remove(arqfb);                    // Remove the packet and queue it later	
	    hdr_mac802_16 * gh = HDR_MAC802_16(arqfb); 
	    gh->arq_ie[0].cid = flowno;
	    gh->arq_ie[0].last = 0;				 // this is the last IE
	    gh->arq_ie[0].ack_type = 1;				 // cumulative ACK
	    gh->arq_ie[0].fsn = ack_seq_ - 1;		                 // sequence number in ACK
	    arq_feedback_queue_->enque(arqfb);
	    debug2("ARQ Receive: ACK queued, queue length: %d flow:%d fsn:%d\n",arq_feedback_queue_->length(),flowno,gh->arq_ie[0].fsn);
	  }
	  else {
	    Packet* arqfb = Packet::alloc();
	    debug2("ARQ : Creating a new acknowledgement packet \n");	
	    hdr_mac802_16 * gh = HDR_MAC802_16(arqfb); 
	    gh->arq_ie[0].cid = flowno;
	    gh->arq_ie[0].last = 0;				 // this is the last IE
	    gh->arq_ie[0].ack_type = 1;				 // cumulative ACK
	    gh->arq_ie[0].fsn = ack_seq_ - 1;		         // sequence number in ACK
	    gh->num_of_acks = 1; 	                                 //
	    arq_feedback_queue_->enque(arqfb);
	    debug2("ARQ Receive: ACK queued, queue length: %d flow:%d fsn: %d \n",arq_feedback_queue_->length(),flowno, gh->arq_ie[0].fsn);
	  }
	  return;
	}  	
    } else if(*inOrder == 0){
      last_ack_sent_ = seqno;	 			 // set last ACK sent var to present ACK seq no
      debug2("ARQ Receive: Enqueueing a selective acknowledgement \n");
      Packet* arqfb = arq_feedback_queue_->tail ();
      if(arqfb && (HDR_MAC802_16(arqfb)->num_of_acks <= MAX_NUM_ACK)) {
	arq_feedback_queue_->remove(arqfb);                    // Remove the packet and queue it later	
	hdr_mac802_16 * gh = HDR_MAC802_16(arqfb);
	int number_of_acks = gh->num_of_acks;	 
	gh->arq_ie[number_of_acks].cid = flowno;
	gh->arq_ie[number_of_acks].last = 0;				 	 // this is the last IE
	gh->arq_ie[number_of_acks].ack_type = 2;				 // Selective ACK
	gh->arq_ie[number_of_acks].fsn = last_ack_sent_;		         // sequence number in ACK
	gh->num_of_acks = gh->num_of_acks + 1;	
	arq_feedback_queue_->enque(arqfb);
	debug2("ARQ Receive: ACK queued, queue length: %d flow:%d\n",arq_feedback_queue_->length(),flowno);
      }
      else {
	Packet* arqfb = Packet::alloc();
	hdr_mac802_16 * gh = HDR_MAC802_16(arqfb); 
	gh->arq_ie[0].cid = flowno;
	gh->arq_ie[0].last = 0;				 // this is the last IE
	gh->arq_ie[0].ack_type = 1;				 // cumulative ACK
	gh->arq_ie[0].fsn = ack_seq_ - 1;		         // sequence number in ACK
	gh->num_of_acks = 1; 	                                 //
	int number_of_acks = gh->num_of_acks;	 
	gh->arq_ie[number_of_acks].cid = flowno;
	gh->arq_ie[number_of_acks].last = 0;				 	 // this is the last IE
	gh->arq_ie[number_of_acks].ack_type = 2;				 // Selective ACK
	gh->arq_ie[number_of_acks].fsn = last_ack_sent_;		         // sequence number in ACK
	gh->num_of_acks = gh->num_of_acks + 1;		
	arq_feedback_queue_->enque(arqfb);
	debug2("ARQ Receive: ACK queued, queue length: %d flow:%d\n",arq_feedback_queue_->length(),flowno);
      }
    } 
  }
  debug2("Exiting ARQ Receive\n"); 
}
//End RPI

//Begin RPI
void Arqstatus::arqReceiveBufferTransfer(Packet* p, Connection *connection, u_int8_t* inOrder)
{

  // Read the header to find out the flow the packet belongs to
  hdr_mac802_16 * mh = HDR_MAC802_16(p);
  u_int32_t flowno = mh->header.cid;
  u_int32_t seqno;
  if(connection->isPackingEnable () == true){  
    seqno = mh->pack_subheader.sn;
  } 
  else {
    seqno = mh->frag_subheader.sn;
  }
  debug2("ARQ cid=%d receive: type_arqfb:%d\n",flowno, mh->header.type_arqfb);
  debug2("ack_seq_ = %d ; seqno = %d ; ack_counter_ = %d ; ack_period_ = %d\n", ack_seq_, seqno, ack_counter_, ack_period_);
  // Read sequence number and check for in order packet
  // if out of order, free the packet and reset inOrder variable
  if (ack_seq_ != seqno) {
    //if((((seqno - ack_seq_) > 0) && ((seqno - ack_seq_) <= arq_max_window_)) || ((seqno >= 0 && seqno < arq_max_window_) && (ack_seq_ < MAX_SEQ && ack_seq_ >= (MAX_SEQ - arq_max_window_))))
    if ( (((seqno - ack_seq_) > 0) && ((seqno - ack_seq_) <= arq_max_window_))
	 || ((seqno - ack_seq_ < 0) && (seqno+MAX_SEQ - ack_seq_ <= arq_max_window_)) )
      {
	*inOrder = 0;
	debug2("ARQ cid=%d receive: out of order - in arq receive ack_seq_ : %d seq_no. in packet : %d\n",flowno, ack_seq_, seqno);
      }
    else
      {
	*inOrder = 2;
	debug2("ARQ cid=%d receive: Should remove the packet to clean queue ack_seq_ : %d seq_no. in packet : %d\n",flowno, ack_seq_, seqno);
      }
  } 
  // if in order, increment the feedback sequence number
  if (ack_seq_ == seqno) { 
    ack_seq_++;
    if (ack_seq_ == arq_max_seq_) {
      ack_seq_ = 0;
    }
    debug2("ARQ cid=%d Receive: in order: flow:%d \t seq no: %d\n",flowno ,flowno,seqno); 
  }
  // increment the ACK counter and check if time to create and enque feedback IE  
  ack_counter_ ++;
  if (ack_counter_ == ack_period_) { 				// if time to send ACK
    if ((*inOrder == 1)) {  				//Case of Cumulative acknowledgement
      ack_counter_ = 0;				
      //if((((ack_seq_ - seqno) > 0) && ((ack_seq_ -  seqno) <= arq_max_window_)) || ((ack_seq_ >= 0 && ack_seq_ < arq_max_window_) && (seqno < MAX_SEQ && seqno >= (MAX_SEQ - arq_max_window_))))
      if ( (((ack_seq_ - seqno) > 0) && ((ack_seq_ - seqno) <= arq_max_window_))
	   || ((ack_seq_- seqno < 0) && (ack_seq_+MAX_SEQ - seqno <= arq_max_window_)) )
	{
	  debug2("ARQ cid=%d Receive: Enqueueing a cumulative acknowledgement \n",flowno);
	  // Let the cumulative acknowledgement always occupy the first slot and here we will fill only ARQ IE information
	  Packet* arqfb = arq_feedback_queue_->tail ();
	  if(arqfb && (HDR_MAC802_16(arqfb)->num_of_acks <= MAX_NUM_ACK)) {
	    arq_feedback_queue_->remove(arqfb);                    // Remove the packet and queue it later	
	    hdr_mac802_16 * gh = HDR_MAC802_16(arqfb); 
	    gh->arq_ie[0].cid = flowno;
	    gh->arq_ie[0].last = 0;				 // this is the last IE
	    gh->arq_ie[0].ack_type = 1;				 // cumulative ACK
	    gh->arq_ie[0].fsn = ack_seq_ - 1;		                 // sequence number in ACK
	    arq_feedback_queue_->enque(arqfb);
	    debug2("ARQ cid=%d Receive: ACK queued, queue length: %d flow:%d\n",flowno,arq_feedback_queue_->length(),flowno);
	  }
	  else {
	    Packet* arqfb = Packet::alloc();
	    hdr_mac802_16 * gh = HDR_MAC802_16(arqfb); 
	    gh->arq_ie[0].cid = flowno;
	    gh->arq_ie[0].last = 0;				 // this is the last IE
	    gh->arq_ie[0].ack_type = 1;				 // cumulative ACK
	    gh->arq_ie[0].fsn = ack_seq_ - 1;		         // sequence number in ACK
	    gh->num_of_acks = 1; 	                                 //
	    arq_feedback_queue_->enque(arqfb);
	    debug2("ARQ cid=%d Receive: ACK queued, queue length: %d flow:%d\n",flowno,arq_feedback_queue_->length(),flowno);
	  }
	  return;
	}  	
    } else {
      ack_counter_ = 0;				 // set counter back to zero
    } 
  }
  debug2("Exiting ARQ cid=%d Receive\n",flowno); 
}
//End RPI
//Begin RPI
void  Arqstatus::arqRecvFeedback(Packet* p, u_int16_t i, Connection* connection) 
{
  // Read the header to find the flow the feedback is for
  hdr_mac802_16 *mh = HDR_MAC802_16(p);
  arq_fb_ie ie = mh->arq_ie[i];
  u_int16_t flowno = ie.cid;
  u_char  ack_type = ie.ack_type;
  u_int32_t seq_recd = ie.fsn;
  u_int32_t sn;
  //debug2("ARQ Feedback: feedback recd for flow:%d and seq:%d ack_type : %d\n",flowno,seq_recd, ack_type);
  debug2 ("  ARQ Feedback: feedback recd for con %d qlength=%d arqTx=%d arqRtx=%d\n", connection->get_cid(), connection->get_queue ()->length(), arq_trans_queue_->length(), arq_retrans_queue_->length());

  if(ack_type == 1)
    ack_seq_ = seq_recd;
  // deque packets for which feedback received from transmission queue, free packet
  arq_trans_queue_->resetIterator ();
  while (arq_trans_queue_->length() != 0) {
    Packet* ph = (arq_trans_queue_)->getNext ();
    if (!ph) {
      // If one complete iteration is over, we can break the loop  
      break;
    }
    hdr_mac802_16 * gh = HDR_MAC802_16(ph);
    if(connection->isPackingEnable () == true){ 
      sn = gh->pack_subheader.sn;
    }
    else {
      sn = gh->frag_subheader.sn;
    }
    debug2("ARQ Feedback: packet has seq:%d\t and recd seq:%d (arq_curr_window_=%d, arq_max_window_=%d, MAX_SEQ=%d)\n",sn,seq_recd, arq_curr_window_, arq_max_window_, MAX_SEQ);
    //if ((ack_type == 1) && ((((seq_recd - sn) >= 0) && ((seq_recd - sn) < arq_max_window_)) || ((seq_recd >= 0 && seq_recd < arq_max_window_) && (sn < MAX_SEQ && (sn >= (MAX_SEQ - arq_max_window_)))))){	    
    if ((ack_type == 1) && 
	( (((seq_recd - sn) >= 0) && ((seq_recd - sn) < arq_max_window_))
	  || ((seq_recd - sn < 0) && (seq_recd+MAX_SEQ - sn < arq_max_window_))) ){	
      arq_trans_queue_->remove(ph);
      Packet::free(ph);	
      debug2("ARQ Feedback:dequeued1 packet with seq:%d\n",sn);
    } 
    else if((ack_type == 2) && (seq_recd == sn)) {
      arq_trans_queue_->remove(ph);
      Packet::free(ph);	
      debug2("ARQ Feedback:dequeued2 packet with seq:%d\n",sn);
    }
  }

  // deque packets for which feedback received from retransmission queue, free packet- This case can happen sometimes
  arq_retrans_queue_->resetIterator ();
  while (arq_retrans_queue_->length() != 0) {
    Packet* ph = (arq_retrans_queue_)->getNext ();
    if (!ph) {
      // If one complete iteration is over, we can break the loop  
      break;
    }
    hdr_mac802_16 * gh = HDR_MAC802_16(ph);
    if(connection->isPackingEnable () == true){ 
      sn = gh->pack_subheader.sn;
    }
    else {
      sn = gh->frag_subheader.sn;
    }
    debug2("ARQ Feedback: packet has seq:%d\t and recd seq:%d\n",sn,seq_recd);
    //if ((ack_type == 1) && ((((seq_recd - sn) >= 0) && ((seq_recd - sn) < arq_max_window_))  || ((seq_recd >= 0 && seq_recd < arq_max_window_) && (sn < MAX_SEQ && (sn >= (MAX_SEQ - arq_max_window_)))))){	
    if ((ack_type == 1) && 
	( (((seq_recd - sn) >= 0) && ((seq_recd - sn) < arq_max_window_))
	  || ((seq_recd - sn < 0) && (seq_recd+MAX_SEQ - sn < arq_max_window_))) ){	
      arq_retrans_queue_->remove(ph);
      Packet::free(ph);  	
      debug2("ARQ Feedback:dequeued packet from retrans queue with seq:%d\n",sn);
    } 
    else if((ack_type == 2) && (seq_recd == sn)) {
      arq_retrans_queue_->remove(ph);
      Packet::free(ph); 	
      debug2("ARQ Feedback:dequeued packet from retrans queue with seq:%d\n",sn);
    }
  }

}
// End RPI

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

#include "connection.h"
#include "connectionmanager.h"
#include "mac802_16.h"

static int basicIndex = BASIC_CID_START;
static int primaryIndex = PRIMARY_CID_START;
static int transportIndex = TRANSPORT_SEC_CID_START;
static int multicastIndex = MULTICAST_CID_START;
/*RPI for PACK*/
/**
 * Constructor used by BS to automatically assign CIDs
 * @param type The connection type. 
 */
Connection::Connection (ConnectionType_t type) : peer_(0), 
						 frag_status_(FRAG_NOFRAG), 
						 frag_nb_(0), 
						 frag_byte_proc_(0),
						 frag_enable_(true),
						 pack_enable_(true),	
						 requested_bw_(0),
						 requested_cdma_(0),
						 requested_cdma_code_(0),
						 requested_cdma_top_(0),
						 poll_int_(0),
						 bw_req_queue_(0)
{
   switch (type) {
   case CONN_INIT_RANGING:
     cid_ = INITIAL_RANGING_CID;
     break;
   case CONN_AAS_INIT_RANGING:
     cid_ = AAS_INIT_RANGIN_CID;
     break;
   case CONN_PADDING:
     cid_ = PADDING_CID;
     break;
   case CONN_BROADCAST:
     cid_ = BROADCAST_CID;
     break;
   case CONN_MULTICAST_POLLING:
     cid_ = multicastIndex++;
     assert (multicastIndex <= MULTICAST_CID_STOP);
     break;
   case CONN_BASIC:
     cid_ = basicIndex++;
     assert (basicIndex <= BASIC_CID_STOP);
     break;
   case CONN_PRIMARY:
     cid_ = primaryIndex++;
     assert (primaryIndex <= PRIMARY_CID_STOP);
       break;
   case CONN_SECONDARY:
   case CONN_DATA:
     cid_ = transportIndex++;
     assert (transportIndex <= TRANSPORT_SEC_CID_STOP);
     break;
   default:
     fprintf (stderr, "Unsupported connection type\n");
     exit (1);
   }
   type_ = type;
   queue_ = new PacketQueue();
   arqstatus_ = NULL;
   initCDMA_SSID();
}

/**
 * Constructor used by SSs when the CID is already known
 * @param type The connection type
 * @param cid The connection cid
 */
Connection::Connection (ConnectionType_t type, int cid) : peer_(0), 
							  frag_status_(FRAG_NOFRAG), 
							  frag_nb_(0), 
							  frag_byte_proc_(0),
							  frag_enable_(true),
							  pack_enable_(true),	
							  requested_bw_(0),
						 	  requested_cdma_(0),
						          requested_cdma_code_(0),
						          requested_cdma_top_(0),
						          poll_int_(0),
							  bw_req_queue_(0)
{
  cid_ = cid;
  type_ = type;
  queue_ = new PacketQueue();
  arqstatus_ = NULL;
  initCDMA_SSID();
}

/**
 * Destructor
 */
Connection::~Connection ()
{
  if (arqstatus_) delete arqstatus_ ;
  flush_queue ();
}

/**
 * Set the connection manager
 * @param manager The Connection manager 
 */
void Connection::setManager (ConnectionManager *manager)
{
  manager_ = manager;
}

/**
 * Enqueue the given packet
 * @param p The packet to enqueue
 */
void Connection::enqueue (Packet * p) 
{
  //Mark the timestamp for queueing delay
  HDR_CMN(p)->timestamp() = NOW;
  queue_->enque (p);
}

//Begin RPI
/**
 * Enqueue the given packet at the head
 * @param p The packet to enqueue
 */
void Connection::enqueue_head (Packet * p) 
{
  //Mark the timestamp for queueing delay
  //HDR_CMN(p)->timestamp() = NOW;
  queue_->enqueHead (p);
}
//End RPI

/**
 * Dequeue a packet from the queue
 * @param p The packet to enqueue
 */
Packet * Connection::dequeue () 
{
  Packet *p = queue_->deque ();
  return p;
}

/**
 * Flush the queue and return the number of packets freed
 * @return The number of packets flushed
 */
int Connection::flush_queue()
{
  int i=0;
  Packet *p;
  while ( (p=queue_->deque()) ) {
    manager_->getMac()->drop(p, "CON");
    i++;
  }
  return i;
}

/**
 * Return queue size in bytes
 */
int Connection::queueByteLength () 
{
  return queue_->byteLength ();
}

/**
 * Lookup element n in queue
 */
Packet* Connection::queueLookup (int n)
{
  return queue_->lookup(n);
}

/**
 * Return queue size in bytes
 */
int Connection::queueLength () 
{
  return queue_->length ();
}

/** 
 * Update the fragmentation information
 * @param status The new fragmentation status
 * @param index The new fragmentation index
 * @param bytes The number of bytes 
 */
void Connection::updateFragmentation (fragment_status status, int index, int bytes)
{
  frag_status_ = status;
  frag_nb_ = index;
  frag_byte_proc_ = bytes;
}

/**
 * Set the bandwidth requested
 * @param bw The bandwidth requested in bytes
 */
void Connection::setBw (int bw)
{
  //in some cases, the SS may send more data than allocated.
  //assert (bw >=0);
  requested_bw_ = bw<0?0:bw;
  //printf ("Set bw %d for connection %d(%x)\n", requested_bw_,cid_, this);
}

/**
 * Set the bandwidth requested
 * @param bw The bandwidth requested in bytes
 */
int Connection::getBw ()
{
  //printf ("Get %d bw for connection %d(%x)\n", requested_bw_,cid_, this);
  return requested_bw_;
}

/**
 * Set the polling interval
 * @param polling frame
 */
void Connection::setPOLL_interval (int poll_int)
{
  poll_int_ = poll_int;
}

/**
 * Get the polling interval
 * @param polling in frames
 */
int Connection::getPOLL_interval ()
{
  return poll_int_;
}

/**
 * Inital values for all cdma-initial-ranging variables
 */
void Connection::initCDMA_SSID()
{
  for (int i = 0; i < MAX_SSID; i++) {
    cdma_per_conn[i].cdma_flag = 0;
    cdma_per_conn[i].cdma_ssid = 0;
    cdma_per_conn[i].cdma_code = 0;
    cdma_per_conn[i].cdma_top = 0;
    cdma_per_conn[i].cdma_cid = 0;
    init_req_queue_[i] = 0;
  }
}

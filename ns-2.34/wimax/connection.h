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

#ifndef CONNECTION_H
#define CONNECTION_H

#include "serviceflow.h"
#include "arqstatus.h"
#include "packet.h"
#include "queue.h"
#include "mac802_16pkt.h"

/* CONSTANTS */
#define INITIAL_RANGING_CID 0x0000
#define BASIC_CID_START     0x0001
#define BASIC_CID_STOP      0x2000
#define PRIMARY_CID_START   0x2001
#define PRIMARY_CID_STOP    0x4000
#define TRANSPORT_SEC_CID_START 0x4001
#define TRANSPORT_SEC_CID_STOP 0xFEFE
#define AAS_INIT_RANGIN_CID 0xFEFF
#define MULTICAST_CID_START 0xFF00
#define MULTICAST_CID_STOP  0xFFFD
#define PADDING_CID         0xFFFE
#define BROADCAST_CID       0xFFFF

#define IN_CONNECTION  false
#define OUT_CONNECTION true

/**
 * Define maximum subscribers since in this version, we use "br" field to indicate subsriber station;
 * Need to change it later with cdma packet structure at mac802_16pkt.h
 */
#define MAX_SSID 2048

/**
 * Define cdma_connection for cdma-initial-ranging (uses only cid 0 for all subscribers)
 */
struct cdma_connection {
  int cdma_flag;
  int cdma_ssid;
  u_char cdma_code;
  u_char cdma_top;
  int cdma_cid;
};

/**
 * Define the type of the connection
 */
enum ConnectionType_t {
  CONN_INIT_RANGING,
  CONN_AAS_INIT_RANGING,
  CONN_MULTICAST_POLLING,
  CONN_PADDING,
  CONN_BROADCAST,
  CONN_BASIC,
  CONN_PRIMARY,
  CONN_SECONDARY,
  CONN_DATA
};

class PeerNode;
class ConnectionManager;
class Connection;
class Arqstatus;
LIST_HEAD (connection, Connection);

/** 
 * Class Connection
 * The class supports LIST.
 */ 
class Connection {
 public:
  /** constructor */
  Connection (ConnectionType_t);

  /** constructor */
  Connection (ConnectionType_t, int cid);    

  /** destructor */
  ~Connection ();

  /**
   * Set the connection manager
   * @param manager The Connection manager 
   */
  void setManager (ConnectionManager *manager);

  /**
   * Enqueue the given packet
   * @param p The packet to enqueue
   */
  void  enqueue (Packet * p);

   /**
   * Enqueue the given packet at the head
   * @param p The packet to enqueue
   */
  void  enqueue_head (Packet * p);

  /**
   * Set the service flow for this connection
   * @param sflow The service flow for this connection
   */
  void  setServiceFlow (ServiceFlow * sflow);
  
  /**
   * Return the service flow for this connection
   */
  ServiceFlow *  getServiceFlow ();

  /**
   * Get the value of cid
   * The connection id
   * @return the value of cid
   */
  inline int get_cid ( ) { return cid_; }

  /**
   * Get the value of type_
   * The connection id
   * @return the value of type_
   */
  inline ConnectionType_t get_category ( ) { return type_; }
          
  /**
   * Get the value of serviceflow_
   * The service flow associated with the connection
   * @return the value of serviceflow_
   */
  inline ServiceFlow * get_serviceflow ( ) { return serviceflow_; }
  
  /**
   * Set the value of serviceflow_
   * The service flow associated with the connection
   * @return the value of serviceflow_
   */
  inline void set_serviceflow (ServiceFlow * value ) { serviceflow_ = value; }
  
  /**
   * return the connection type
   * @return The connection type
   */
  inline ConnectionType_t getType () { return type_; }

  /**
   * Get the value of queue_
   * The queue for this connection
   * @return the value of queue_
   */
  inline PacketQueue * get_queue ( ) { return queue_; }
    
  /**
   * Dequeue a packet from the queue
   * @param p The packet to enqueue
   */
  Packet * dequeue ();

  /**
   * Return queue size in bytes
   * @return The queue size in bytes
   */
  int queueByteLength ();

  Packet * queueLookup (int n);

  /**
   * Return queue size in number of packets
   * @return The number of packet in the queue
   */
  int queueLength ();

  /**
   * Flush the queue
   */
  int flush_queue ();

  /**
   * Enable/Disable fragmentation
   */
  void enable_fragmentation (bool enable) { frag_enable_ = enable; }

  /**
   * Indicates if the connection supports fragmentation
   */
  bool isFragEnable () { return frag_enable_; }

  /**
   * Enable/Disable Packing
   */
  void enable_packing (bool enable) { pack_enable_ = enable; }

  /**
   * Indicates if the connection supports packing
   */
  bool isPackingEnable () { return pack_enable_; }

  // Chain element to the list
  inline void insert_entry(struct connection *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Return next element in the chained list
  Connection* next_entry(void) const { return link.le_next; }

  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

  /**
   * Return the peer node for this connection
   * @return the peer node for this connection
   */
  inline PeerNode * getPeerNode () { return peer_; }

  /**
   * Return the ArqStatus for this connection
   * @return the ArqStatus for this connection
   */
  inline Arqstatus * getArqStatus () { return arqstatus_; }

  /**
   * Set the peer node for this connection
   * @param the peer node for this connection
   */
  inline void setPeerNode (PeerNode *peer) { peer_=peer; }

  /**
   * Set the ArqStatus for this connection
   * @param the ArqStatus for this connection
   */
  inline void setArqStatus (Arqstatus * arqstatus) { arqstatus_= arqstatus; }

  /** 
   * Update the fragmentation information
   * @param status The new fragmentation status
   * @param index The new fragmentation index
   * @param bytes The number of bytes 
   */
  void updateFragmentation (fragment_status status, int index, int bytes);

  fragment_status getFragmentationStatus () { return frag_status_; }

  int getFragmentNumber () { return frag_nb_; }

  int getFragmentBytes () { return frag_byte_proc_; }

  /**
   * Set the bandwidth requested 
   * @param bw The bandwidth requested in bytes
   */
  void setBw (int bw);

  /**
   * Get the bandwidth requested
   * @param bw The bandwidth requested in bytes
   */
  int getBw ();

  /**
   * Get and Set polling interval 
   */
  int getPOLL_interval ();
  void setPOLL_interval (int poll_int);

  /**
   * Get and Set counter for cdma bandwidth requested parameters
   */
  inline int getBW_REQ_QUEUE () { return bw_req_queue_; }
  inline void setBW_REQ_QUEUE ( int bw_req_queue ) { bw_req_queue_ = bw_req_queue ;}

  /**
   * Get and Set counter for cdma initial ranging requested parameters
   */
  inline int getINIT_REQ_QUEUE (int ssid) { return init_req_queue_[ssid]; }
  inline void setINIT_REQ_QUEUE (int ssid, int init_req_queue) { init_req_queue_[ssid] = init_req_queue;}

  /**
   * Get and Set the cdma bandwidth requested parameters
   */
  inline int getCDMA ()		{ return requested_cdma_; }
  inline u_char getCDMA_code () { return requested_cdma_code_; }
  inline u_char getCDMA_top ()  { return requested_cdma_top_; }
  inline void setCDMA (int cdma_flag) 			     { requested_cdma_ = cdma_flag; }
  inline void setCDMA_code (u_char cdma_code) 		     { requested_cdma_code_ = cdma_code;}
  inline void setCDMA_top (u_char cdma_top) 		     { requested_cdma_top_ = cdma_top;}
   
  /**
   * Get and Set the cdma initial ranging requested parameters; uses only cid 0 so need array of subscribers
   */
  inline int getCDMA_SSID_FLAG (int ssid)    { return cdma_per_conn[ssid].cdma_flag; }
  inline u_char getCDMA_SSID_TOP (int ssid)  { return cdma_per_conn[ssid].cdma_top; }
  inline u_char getCDMA_SSID_CODE (int ssid) { return cdma_per_conn[ssid].cdma_code; }
  inline int getCDMA_SSID_CID (int ssid)     { return cdma_per_conn[ssid].cdma_cid; }
  inline int getCDMA_SSID_SSID (int ssid)    { return cdma_per_conn[ssid].cdma_ssid; }

  inline void setCDMA_SSID_FLAG (int ssid, int flag_ssid)    { cdma_per_conn[ssid].cdma_flag = flag_ssid;}
  inline void setCDMA_SSID_TOP (int ssid, u_char top_ssid)   { cdma_per_conn[ssid].cdma_top = top_ssid;}
  inline void setCDMA_SSID_CODE (int ssid, u_char code_ssid) { cdma_per_conn[ssid].cdma_code = code_ssid;}
  inline void setCDMA_SSID_CID (int ssid, int cid_ssid)      { cdma_per_conn[ssid].cdma_cid = cid_ssid;}
  inline void setCDMA_SSID_SSID (int ssid, int ssid_ssid)    { cdma_per_conn[ssid].cdma_ssid = ssid_ssid;}
  inline void initCDMA () 				     { requested_cdma_ = 0; requested_cdma_code_ = 0; requested_cdma_top_ = 0;}

  /**
   * Clear cdma parameter value
   */
  void initCDMA_SSID ();

 protected:

  /**
   * Pointer to next in the list
   */
  LIST_ENTRY(Connection) link;
  //LIST_ENTRY(Connection); //for magic draw


 private:
  /**
   * The connection manager
   */
  ConnectionManager* manager_;

  /**
   * The connection id
   */
  int cid_;

  /**
   * The service flow associated with the connection
   */
  ServiceFlow * serviceflow_;

  /**
   * ArqStatus of the connection
   */
  Arqstatus * arqstatus_;

  /**
   * The queue for this connection
   */
  PacketQueue * queue_;

  /** 
   * The connection type
   */
  ConnectionType_t type_;
  
  /**
   * Pointer to the peer node data
   */
  PeerNode *peer_;

  /**
   * Fragmentation status 
   */
  fragment_status frag_status_;

  /**
   * Fragmentation number
   */
  int frag_nb_;
  
  /**
   * Bytes already processed (i.e sent or received)
   */
  int frag_byte_proc_;

  /**
   * Indicates if the connection can use fragmentation
   */
  bool frag_enable_;

  /**
   * Indicates if the connection can use packing
   */
  bool pack_enable_;

  /** 
   * Indicates the number of bytes requested
   */
  int requested_bw_;

  /** 
   * Indicates polling interval for each connection
   */
  int poll_int_;

  /** 
   * Indicates a counter to resend bandwidth request (do not use for now)
   */
  int bw_req_queue_;

  /** 
   * Indicates cdma-bw-req variables
   */
  int requested_cdma_;
  u_char requested_cdma_code_;
  u_char requested_cdma_top_;

  /** 
   * Indicates cdma-initial-ranging-req variables
   */
  cdma_connection cdma_per_conn[MAX_SSID];
  int init_req_queue_[MAX_SSID];

};
#endif //CONNECTION_H


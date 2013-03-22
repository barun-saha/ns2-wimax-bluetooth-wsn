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

#include "serviceflowhandler.h"
#include "mac802_16.h"
#include "scheduling/wimaxscheduler.h"
#include "arqstatus.h"

static int TransactionID = 0; 
/* 
 * Create a service flow
 * @param mac The Mac where it is located
 */
ServiceFlowHandler::ServiceFlowHandler ()
{
  LIST_INIT (&flow_head_);
  LIST_INIT (&pendingflow_head_);
  LIST_INIT (&static_flow_head_);
}

/*
 * Set the mac it is located in
 * @param mac The mac it is located in
 */
void ServiceFlowHandler::setMac (Mac802_16 *mac)
{
  assert (mac);

  mac_ = mac;
}

/**
 * Process the given packet. Only service related packets must be sent here.
 * @param p The packet received
 */
void ServiceFlowHandler::process (Packet * p) 
{ 
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;

  //we cast to this frame because all management frame start with
  //a type 
  mac802_16_dl_map_frame *frame = (mac802_16_dl_map_frame*) p->accessdata();

  switch (frame->type) {
  case MAC_DSA_REQ: 
    processDSA_req (p);
    break;
  case MAC_DSA_RSP: 
    processDSA_rsp (p);
    break;
  case MAC_DSA_ACK: 
    processDSA_ack (p);
    break;
  default: 
    ; //*debug2 ("Unknow frame type (%d) in flow handler\n", frame->type);
  }
  //Packet::free (p);
}

/**
 * Add a flow with the given qos
 * @param qos The QoS for the flow
 * @return the created ServiceFlow
 */
ServiceFlow* ServiceFlowHandler::addFlow (ServiceFlowQoS * qos) {
  return NULL;
}

/**
 * Remove the flow given its id
 * @param id The flow id
 */
void ServiceFlowHandler::removeFlow (int id) {
  
}

/**
 * Send a flow request to the given node
 * @param index The node address
 * @param out The flow direction
 */
void ServiceFlowHandler::sendFlowRequest (int index, bool out)
{
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_dsa_req_frame *dsa_frame;
  PeerNode *peer;

  //create packet for request
  peer = mac_->getPeerNode(index);  
  p = mac_->getPacket ();
  ch = HDR_CMN(p);
  wimaxHdr = HDR_MAC802_16(p);
  p->allocdata (sizeof (struct mac802_16_dsa_req_frame));
  dsa_frame = (mac802_16_dsa_req_frame*) p->accessdata();
  dsa_frame->type = MAC_DSA_REQ;
  dsa_frame->uplink = (out && mac_->getNodeType()==STA_MN) || (!out && mac_->getNodeType()==STA_BS) ;
  dsa_frame->transaction_id = TransactionID++;
  //*debug2(" sampad in send flow request"); 
  if (mac_->getNodeType()==STA_MN)
    ch->size() += GET_DSA_REQ_SIZE (0);
  else {
    //assign a CID and include it in the message
    Connection *data = new Connection (CONN_DATA);
/*
    data->setCDMA(0);
    data->initCDMA();
    data->setPOLL_interval(0);
*/

    mac_->getCManager()->add_connection (data, out);
    if (out)
    {
      peer->setOutData (data);
      //*debug2("set outcoming data connection for mac %d\n", mac_->addr());
    }
    else
    {
   	   peer->setInData (data);
   	   //*debug2("set incoming data connection for mac %d\n", mac_->addr());
    }
    dsa_frame->cid = data->get_cid();
    ch->size() += GET_DSA_REQ_SIZE (1);
  }

  wimaxHdr->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
  peer->getPrimary(OUT_CONNECTION)->enqueue (p);
}

/**
 * process a flow request
 * @param p The received request
 */
void ServiceFlowHandler::processDSA_req (Packet *p)
{
  mac_->debug ("At %f in Mac %d received DSA request from %d\n", NOW, mac_->addr(), HDR_MAC802_16(p)->header.cid);
  //*debug2(" sampad in send process DSA_request"); 
  Packet *rsp;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr_req;
  hdr_mac802_16 *wimaxHdr_rsp;
  mac802_16_dsa_req_frame *dsa_req_frame;
  mac802_16_dsa_rsp_frame *dsa_rsp_frame;
  PeerNode *peer;
  Connection *data;
  Arqstatus *arqstatus;

  //read the request
  wimaxHdr_req = HDR_MAC802_16(p);
  dsa_req_frame = (mac802_16_dsa_req_frame*) p->accessdata();
  peer = mac_->getCManager ()->get_connection (wimaxHdr_req->header.cid, true)->getPeerNode();
  
  //allocate response
  //create packet for request
  rsp = mac_->getPacket ();
  ch = HDR_CMN(rsp);
  wimaxHdr_rsp = HDR_MAC802_16(rsp);
  rsp->allocdata (sizeof (struct mac802_16_dsa_rsp_frame));
  dsa_rsp_frame = (mac802_16_dsa_rsp_frame*) rsp->accessdata();
  dsa_rsp_frame->type = MAC_DSA_RSP;
  dsa_rsp_frame->transaction_id = dsa_req_frame->transaction_id;
  dsa_rsp_frame->uplink = dsa_req_frame->uplink;
  dsa_rsp_frame->confirmation_code = 0; //OK
  dsa_rsp_frame->staticflow = dsa_req_frame->staticflow;

  if (mac_->getNodeType()==STA_MN) {
    //the message contains the CID for the connection
    data = new Connection (CONN_DATA, dsa_req_frame->cid);
/*
    data->setCDMA(0);
    data->initCDMA();
    data->setPOLL_interval(0);
*/

    data->set_serviceflow(dsa_req_frame->staticflow);
    // We will move all the ARQ information in the service flow to the isArqStatus that maintains the Arq Information.
    if(data->get_serviceflow ()->getQoS ()->getIsArqEnabled () == 1 ) {
    		//*debug2("ARQ status is enable.\n");
	    arqstatus = new Arqstatus ();
	    data->setArqStatus (arqstatus);
	    data->getArqStatus ()->setArqEnabled (data->get_serviceflow ()->getQoS ()->getIsArqEnabled ());
	    data->getArqStatus ()->setRetransTime (data->get_serviceflow ()->getQoS ()->getArqRetransTime ()); 
	    data->getArqStatus ()->setMaxWindow (data->get_serviceflow ()->getQoS ()->getArqMaxWindow ());
            data->getArqStatus ()->setCurrWindow (data->get_serviceflow ()->getQoS ()->getArqMaxWindow ()); 
	    data->getArqStatus ()->setAckPeriod (data->get_serviceflow ()->getQoS ()->getArqAckPeriod ());
	    // setting timer array for the flow
	    data->getArqStatus ()->arqRetransTimer = new ARQTimer(data);        /*RPI*/
	    //*debug2("In DSA req STA_MN, generate a ARQ Timer and cid is %d.\n", data->get_cid());
    }  
    if (dsa_req_frame->uplink){
      mac_->getCManager()->add_connection (data, OUT_CONNECTION);
      //*debug2(" dsa-req-frame being processed and connection being added for uplink node = MN\n"); 
      peer->setOutData (data);
      
    } else {
      mac_->getCManager()->add_connection (data, IN_CONNECTION);
      //*debug2(" dsa-req-frame being processed and connection being added for downlink node =MN\n"); 
      peer->setInData (data);
    }
    ch->size() += GET_DSA_RSP_SIZE (0);
  } else {
    //allocate new connection
    data = new Connection (CONN_DATA);
/*
    data->setCDMA(0);
    data->initCDMA();
    data->setPOLL_interval(0);
*/

    data->set_serviceflow(dsa_req_frame->staticflow);
    // We will move all the ARQ information in the service flow to the isArqStatus that maintains the Arq Information.
    if(data->get_serviceflow ()->getQoS ()->getIsArqEnabled () == 1 ){
    		//*debug2("Going to set ARQ parameters.\n");
	    arqstatus = new Arqstatus ();
	    data->setArqStatus (arqstatus);
	    data->getArqStatus ()->setArqEnabled (data->get_serviceflow ()->getQoS ()->getIsArqEnabled ());
	    data->getArqStatus ()->setRetransTime (data->get_serviceflow ()->getQoS ()->getArqRetransTime ()); 
	    data->getArqStatus ()->setMaxWindow (data->get_serviceflow ()->getQoS ()->getArqMaxWindow ());
            data->getArqStatus ()->setCurrWindow (data->get_serviceflow ()->getQoS ()->getArqMaxWindow ()); 
	    data->getArqStatus ()->setAckPeriod (data->get_serviceflow ()->getQoS ()->getArqAckPeriod ());
 	    // setting timer array for the flow
	    data->getArqStatus ()->arqRetransTimer = new ARQTimer(data);        /*RPI*/
	    //*debug2("In DSA req STA_BS, generate a ARQ Timer and cid is %d.\n", data->get_cid());
	    
    }  
    if (dsa_req_frame->uplink) {
      mac_->getCManager()->add_connection (data, IN_CONNECTION);
	//*debug2(" dsa-req-frame being processed and connection being added for uplink node = not MN\n"); 
      peer->setInData (data);
    } else {
      mac_->getCManager()->add_connection (data, OUT_CONNECTION);
      //*debug2(" dsa-req-frame being processed and connection being added for downlink node = not MN\n"); 
      peer->setOutData (data);
      //*debug2("set outcoming data connection for mac %d\n", mac_->addr());
//Begin RPI
      if(data->get_serviceflow ()->getQoS ()->getIsArqEnabled () == 1 ){	
        //schedule timer based on retransmission time setting
        //mac_->debug(" ARQ Timer Setting Downlink\n");	
        data->getArqStatus ()->arqRetransTimer->sched(data->getArqStatus ()->getRetransTime ());
        //mac_->debug(" ARQ Timer Set Downlink\n");
      }    		
//End RPI
    }
    dsa_rsp_frame->cid = data->get_cid();
    ch->size() += GET_DSA_RSP_SIZE (1);
  }

  wimaxHdr_rsp->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
  peer->getPrimary(OUT_CONNECTION)->enqueue (rsp);

}

/**
 * process a flow response
 * @param p The received response
 */
void ServiceFlowHandler::processDSA_rsp (Packet *p)
{
  mac_->debug ("At %f in Mac %d received DSA response\n", NOW, mac_->addr());

  Packet *ack;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr_ack;
  hdr_mac802_16 *wimaxHdr_rsp;
  mac802_16_dsa_ack_frame *dsa_ack_frame;
  mac802_16_dsa_rsp_frame *dsa_rsp_frame;
  Connection *data;
  PeerNode *peer;
  Arqstatus * arqstatus;

  //read the request
  wimaxHdr_rsp = HDR_MAC802_16(p);
  dsa_rsp_frame = (mac802_16_dsa_rsp_frame*) p->accessdata();
  peer = mac_->getCManager ()->get_connection (wimaxHdr_rsp->header.cid, true)->getPeerNode();
  
  //TBD: check if status not OK

  if (mac_->getNodeType()==STA_MN) {
    //the message contains the CID for the connection
    data = new Connection (CONN_DATA, dsa_rsp_frame->cid);
/*
    data->setCDMA(0);
    data->initCDMA();
    data->setPOLL_interval(0);
*/

    data->set_serviceflow(dsa_rsp_frame->staticflow);
    // We will move all the ARQ information in the service flow to the isArqStatus that maintains the Arq Information.
    if(data->get_serviceflow ()->getQoS ()->getIsArqEnabled () == 1 ){
	    arqstatus = new Arqstatus ();
	    data->setArqStatus (arqstatus);
	    data->getArqStatus ()->setArqEnabled (data->get_serviceflow ()->getQoS ()->getIsArqEnabled ());
	    data->getArqStatus ()->setRetransTime (data->get_serviceflow ()->getQoS ()->getArqRetransTime ()); 
	    data->getArqStatus ()->setMaxWindow (data->get_serviceflow ()->getQoS ()->getArqMaxWindow ()); 
	    data->getArqStatus ()->setCurrWindow (data->get_serviceflow ()->getQoS ()->getArqMaxWindow ());	
	    data->getArqStatus ()->setAckPeriod (data->get_serviceflow ()->getQoS ()->getArqAckPeriod ());
	    // setting timer array for the flow
	     data->getArqStatus ()->arqRetransTimer = new ARQTimer(data);             /*RPI*/ 
	     //*debug2("In DSA rsp STA_MN, generate a ARQ Timer and cid is %d.\n", data->get_cid());
    }
    if (dsa_rsp_frame->uplink) {
      mac_->getCManager()->add_connection (data, OUT_CONNECTION);
      peer->setOutData (data);
      //*debug2("set outcoming data connection for mac %d\n", mac_->addr());
//Begin RPI
      if(data->get_serviceflow ()->getQoS ()->getIsArqEnabled () == 1 ){
      	//schedule timer based on retransmission time setting
        //mac_->debug(" ARQ Timer Setting -Uplink\n");
        data->getArqStatus ()->arqRetransTimer->sched(data->getArqStatus ()->getRetransTime ());
        //mac_->debug(" ARQ Timer Set- Uplink\n");
      }   		
//End RPI
    } else {
      mac_->getCManager()->add_connection (data, IN_CONNECTION);
      peer->setInData (data);
      //*debug2("set incoming data connection for mac %d\n", mac_->addr());
    }
  }

  //allocate ack
  //create packet for request
  ack = mac_->getPacket ();
  ch = HDR_CMN(ack);
  wimaxHdr_ack = HDR_MAC802_16(ack);
  ack->allocdata (sizeof (struct mac802_16_dsa_ack_frame));
  dsa_ack_frame = (mac802_16_dsa_ack_frame*) ack->accessdata();
  dsa_ack_frame->type = MAC_DSA_ACK;
  dsa_ack_frame->transaction_id = dsa_rsp_frame->transaction_id;
  dsa_ack_frame->uplink = dsa_rsp_frame->uplink;
  dsa_ack_frame->confirmation_code = 0; //OK
  ch->size() += DSA_ACK_SIZE;

  wimaxHdr_ack->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
  peer->getPrimary(OUT_CONNECTION)->enqueue (ack);

}

/**
 * process a flow request
 * @param p The received response
 */
void ServiceFlowHandler::processDSA_ack (Packet *p)
{
  mac_->debug ("At %f in Mac %d received DSA ack\n", NOW, mac_->addr());
}

/*
 * Add a static flow
 * @param argc The number of parameter
 * @param argv The list of parameters
 */
int ServiceFlowHandler::addStaticFlow (int argc, const char*const* argv) 
{
  dir_t dir;
  int32_t datarate = atoi(argv[3]);
  SchedulingType_t  flow_type;
  double data_size = 0.0;
  u_int16_t period = 0;
  u_int8_t isArqEnabled = atoi(argv[7]);
  double arq_retrans_time = atof(argv[8]);
  u_int32_t arq_max_window = atoi(argv[9]);
  u_int8_t ack_period = atoi(argv[10]);
  int delay = 0 ;
  int burstsize = 0;

  if (strcmp(argv[2], "DL") == 0) {
	  dir = DL;
  } else if (strcmp(argv[2], "UL") == 0) {
	  dir = UL;
  } else {
	  return TCL_ERROR;
  }

  if (strcmp(argv[4], "BE") == 0) {
	  flow_type = SERVICE_BE;
  } else if (strcmp(argv[4], "UGS") == 0) {
	  flow_type = SERVICE_UGS;
	  if (argc != 21) {
		  return TCL_ERROR;
	  }
	  else {
		  // convert user defined bytes to time
		  //data_size = (atoi(argv[5])<<3)/(double)(mac_->phymib_.getDataRate());
		  period = atoi(argv[6]);
		  data_size = atoi(argv[5]);	
		  
	  }
  } else if (strcmp(argv[4], "ertPS") == 0) {
	  flow_type = SERVICE_ertPS;
	  if (argc != 21) {
		  return TCL_ERROR;
	  }
	  else {
		  period = atoi(argv[6]);
		  data_size = atoi(argv[5]);	
	  }    
  } else if (strcmp(argv[4], "rtPS") == 0) {
	  flow_type = SERVICE_rtPS;
	  if (argc != 21) {
		  return TCL_ERROR;
	  }
	  else {
		  period = atoi(argv[6]);
	  }    
  } else if (strcmp(argv[4], "nrtPS") == 0) {
	  flow_type = SERVICE_nrtPS;
	  if (argc != 21) { 
		  return TCL_ERROR;
	  }
	  else {
		  period = atoi(argv[6]);
	  }   
  } else {
	  return TCL_ERROR;
  } 
 
  
  /* Create the Service Flow Qos object */
  ServiceFlowQoS * staticflowqos = new ServiceFlowQoS (delay, datarate, burstsize) ;
  staticflowqos->setDataSize(data_size);
  staticflowqos->setPeriod(period);
  staticflowqos->setIsArqEnabled(isArqEnabled);
  //*debug2("ARQ is enabled for this static flow.\n");
  staticflowqos->setArqRetransTime(arq_retrans_time);
  staticflowqos->setArqMaxWindow(arq_max_window);
  staticflowqos->setArqAckPeriod(ack_period);


  staticflowqos->setTrafficPriority(atoi(argv[11]));
  staticflowqos->setPeakTrafficRate(atoi(argv[12]));
  staticflowqos->setMinReservedTrafficRate(atoi(argv[13]));
  staticflowqos->setReqTransmitPolicy(atoi(argv[14]));
  staticflowqos->setJitter(atoi(argv[15]));
  staticflowqos->setSDUIndicator(atoi(argv[16]));
  staticflowqos->setMinTolerableTrafficRate(atoi(argv[17]));
  staticflowqos->setSDUSize(atoi(argv[18]));
  staticflowqos->setMaxBurstSize(atoi(argv[19]));
  staticflowqos->setSAID(atoi(argv[20]));



   /* Create the Static Service Flow */
  ServiceFlow * staticflow = new ServiceFlow (flow_type, staticflowqos);
  staticflow->setDirection(dir);
 
  /* Add the Service Flow to the Static Flow List*/
  staticflow->insert_entry_head (&static_flow_head_);
  //*debug2(" service flow static flow created ");
  return TCL_OK;
}

/**
 * Send a flow request to the given node
 * @param index The node address
 * @param uplink The flow direction
 */
void ServiceFlowHandler::init_static_flows (int index)
{
  Packet *p;
  struct hdr_cmn *ch;
  hdr_mac802_16 *wimaxHdr;
  mac802_16_dsa_req_frame *dsa_frame;
  PeerNode *peer;
  Arqstatus * arqstatus;
  
  //*debug2(" sampad in init staic flows\n"); 

  for (ServiceFlow *n=static_flow_head_.lh_first; 
		  n; n=n->next_entry()) {
	  //create packet for request
	  peer = mac_->getPeerNode(index);  
	  p = mac_->getPacket ();
	  ch = HDR_CMN(p);
	  wimaxHdr = HDR_MAC802_16(p);
	  p->allocdata (sizeof (struct mac802_16_dsa_req_frame));
	  dsa_frame = (mac802_16_dsa_req_frame*) p->accessdata();
	  dsa_frame->type = MAC_DSA_REQ;
	  if (n->getDirection() == UL ) 
		  dsa_frame->uplink = true;
	  else
		  dsa_frame->uplink = false;
	  dsa_frame->transaction_id = TransactionID++;
	  dsa_frame->staticflow = n;
	  if (mac_->getNodeType()==STA_MN)
		  ch->size() += GET_DSA_REQ_SIZE (0);
	  else {
		  //assign a CID and include it in the message
		  Connection *data = new Connection (CONN_DATA);
/*
		  data->setCDMA(0);
		  data->initCDMA();
    		  data->setPOLL_interval(0);
*/

		  data->set_serviceflow(n);
		  // We will move all the ARQ information in the service flow to the isArqStatus that maintains the Arq Information.
		  if(data->get_serviceflow ()->getQoS ()->getIsArqEnabled () == 1 ){
		  		//*debug2("add an ARQ enabled connection.\n");
			  arqstatus = new Arqstatus ();
			  data->setArqStatus (arqstatus);
			  data->getArqStatus ()->setArqEnabled (data->get_serviceflow ()->getQoS ()->getIsArqEnabled ());
			  data->getArqStatus ()->setRetransTime (data->get_serviceflow ()->getQoS ()->getArqRetransTime ()); 
			  data->getArqStatus ()->setMaxWindow (data->get_serviceflow ()->getQoS ()->getArqMaxWindow ());
                          data->getArqStatus ()->setCurrWindow (data->get_serviceflow ()->getQoS ()->getArqMaxWindow ()); 
			  data->getArqStatus ()->setAckPeriod (data->get_serviceflow ()->getQoS ()->getArqAckPeriod ());
			  // setting timer array for the flow
			  data->getArqStatus ()->arqRetransTimer = new ARQTimer(data);      /*RPI*/
			  //*debug2("In init_static_flow, ARQ connection timer is generated.\n");
		  } 
		  if (n->getDirection() == UL )
		  {
			  mac_->getCManager()->add_connection (data, OUT_CONNECTION);
			  //*debug2("ARQ UL connection.\n");
		  }
		  else
		  {
			  mac_->getCManager()->add_connection (data, IN_CONNECTION); 
			  //*debug2("ARQ DL connection.\n");
		  }
		  if (n->getDirection() == UL)
		  {
			  peer->setInData (data);
			  //*debug2("set incoming data connection for mac %d\n", mac_->addr());
		  }
		  else
		  {
			  peer->setOutData (data);
			  //*debug2("set outcoming data connection for mac %d\n", mac_->addr());
		  }
		  dsa_frame->cid = data->get_cid();
		  ch->size() += GET_DSA_REQ_SIZE (1);
	  }
 
	//*debug2(" sampad in init staic flows before end\n");
	  wimaxHdr->header.cid = peer->getPrimary(OUT_CONNECTION)->get_cid();
	  peer->getPrimary(OUT_CONNECTION)->enqueue (p);
  }
}
// End RPI


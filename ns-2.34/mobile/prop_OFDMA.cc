#include <iostream>

#include <math.h>
#include <stdio.h>
#include <stdarg.h>

#include "delay.h"
#include "packet.h"

#include "packet-stamp.h"
#include "antenna.h"
#include "mobilenode.h"

#include "phy.h" 
#include "wireless-phy.h"
#include "propagation.h"
#include "cmu-trace.h"
#include "prop_OFDMA.h"
//#include "mac802_16.h"
#include "ofdmphy.h"

#include <fstream>
#include <iomanip>

#ifndef BASE_POWER
#define BASE_POWER 1e-20   // The thermal noise level: value is arbit.. made after checking ns-default.tcl // sam added for int power
#endif
using namespace std;

#define rand_channel_MIMO 200  // this is a random no used for antenna no 2 .. n, this num ber will be added to the channel index and used for antennas 2..n for recv diversity. Since channel no 1 is random and is correlated from frame to frame so we add a const no to maintain the randomness and correlation. Can be done in a better way but later. 

/*
 * Trace the given packet
 */
void PropOFDMA::trace(char* fmt, ...)
{
  va_list ap;

  if(trtarget_) {
    assert(trtarget_);
    va_start(ap, fmt);
    vsprintf(trtarget_->pt_->buffer(), fmt, ap);
    trtarget_->pt_->dump();
    va_end(ap);
  }
}

/*
 * Expiration of a packet. 
 */
void PacketTimer::expire(Event *) 
{
  prop_->removePower(record_);
}

static class PropOFDMAClass: public TclClass {
public:
  PropOFDMAClass() : TclClass("Propagation/OFDMA") {}
  TclObject* create(int, const char*const*) {
    return (new PropOFDMA);
  }
} class_prop_OFDMA;

PropOFDMA::PropOFDMA() : Cost231()
{
  trtarget_ = 0;

  head_pkt_ = NULL;
  tail_pkt_ = NULL;
  trash_pkt_ = NULL;
  Pr_subcarrier =NULL; 
  intpower_ =NULL;
  initialized_ = false;
}

PropOFDMA::~PropOFDMA()
{
  trtarget_ = 0;
}

/* ======================================================================
   Public Routines
   ====================================================================== */
int
PropOFDMA::command(int argc, const char*const* argv)
{
    // Commented by Barun : 21-Sep-2011
  //cout << "Enter PropOFDMA::command : ";
  //cout << "Number of arguments : " << argc << endl;
  //cout << "0th arg : " << argv[0] << endl;
  //cout << "1st arg : " << argv[1] << endl;
  //cout << "2nd arg : " << argv[2] << endl;

  if( argc == 3) { 	
      if (strcmp(argv[1], "ITU_PDP") == 0) {
	  SelectedPDP=argv[2];
	  // Commented by Barun : 21-Sep-2011
	  //cout << endl << "SelectedPDP in command() = " << SelectedPDP << endl;
	  LoadDataFile();
	  initialized_ = true;
	  return TCL_OK;
	} else {
	//cout << "not match!" << endl;
      }
    }
	
  return Cost231::command(argc, argv);
}

/*
 * Load the propagation datafile
 */
void PropOFDMA::LoadDataFile()
{
    // Commented by Barun : 21-Sep-2011
  //cout << "Enter PropOFDMA::LoadDataFile function" << endl;
  //cout << "In LoadDataFile, SelectedPDP is " << SelectedPDP << endl;

  ifstream ChannelFile;
  const char * FileToOpen = SelectedPDP.c_str();

    // Added by Barun : 22-Sep-2011
    // To verify the files are being read over the Web; fail otherwise
    // http://www.cplusplus.com/reference/iostream/ios/exceptions/
    ChannelFile.exceptions ( ifstream::failbit | ifstream::badbit );
      try {
        ChannelFile.open(FileToOpen);        
        cout << "PropOFDMA::LoadDataFile : Reading PDP file " << SelectedPDP << endl;
      }
      catch (ifstream::failure e) {
        cout << "*** Exception opening/reading file " << SelectedPDP << " -- file not found!" << endl;
        cout << "Exiting ..." << endl;        
      }
      // End changes
  //ChannelFile.open(FileToOpen);
    // End changes

  /*
    if(!fdstream) {
    cout << "PDP file " << SelectedPDP << " is opened" << endl;
    perror("PDP file opened");
    return rc;
    }
  */
  //cout << "after open file" << endl;;

  //READ CHANNEL GAINS FROM THE FILE
  for(int realization = 0; realization < NUM_REALIZATIONS; realization++){
    for (int subcarrier = 1; subcarrier <= NUM_SUBCARRIERS; subcarrier++){
      //cout << "Realization: " << realization << " Subcarrier: " << subcarrier << endl;
      ChannelFile >> channel_gain[realization][subcarrier];
      //cout << "CurrentValue: " << channel_gain[realization][subcarrier] << endl;
    }
  }

  //cout << "after read gains" << endl;
	
  //set the 1st "subcarrier" to 0 (used for tracking channel useage)
  for(int j = 0; j < NUM_REALIZATIONS; j++){
    channel_gain[j][0]=0;
  }
	
  //fclose(fdstream);
  ChannelFile.close();

  //cout << "leave LoadDataFile" << endl;
}

/**
 * Compute the received power of the given packet
 */
double
PropOFDMA::Pr(PacketStamp *tx, PacketStamp *rx, WirelessPhy *ifp, Packet *p)
{
  OFDMAPhy* phy = (OFDMAPhy*)ifp;
  double Pr_per_subcarrier[NUM_SUBCARRIERS];
  double Pr_tot = 0.0;
  double Pr_bulk = 0.0;
  int Channel_num = 0;
  int num_symbol_per_slot;
  int num_data_subcarrier;
  int num_subchannel;
  int subChannelOffset;
  int numsubChannel; 
  int receive_combining = 0; 
  direction dir;

  if (!initialized_) {
    fprintf (stderr, "Error: no ITU file specified\n");
    exit (1);
  }

  //Skip a power calculation for cdma packet (not in term of slot-based allocation); we assume there is no loss but may collide with the same code
  hdr_cmn* ch = HDR_CMN(p);
  hdr_mac802_16 *wimaxHdr = HDR_MAC802_16(p);
  gen_mac_header_t header = wimaxHdr->header;
  
  if (wimaxHdr->cdma) {
    cdma_req_header_t *req = (cdma_req_header_t *)&header;
    
    if (req->type == 0x3) {
      //debug10 ("\t prof_OFDMA (power calculation) by pass cdma_bw_req code :%d, top :%d\n", req->code, req->top);
      return 1;
    } else if (req->type == 0x2) {
      //debug10 ("\t prof_OFDMA (power calculation) by pass cdma_init_req code :%d, top :%d\n", req->code, req->top);
      return 1;
    } 
  }
  //end cdma packet

  Pr_bulk = Cost231::Pr(tx, rx, ifp);

  //*cout << endl << "Pr after Cost231: " << Pr_bulk << endl;

  for(int i=0;i<NUM_SUBCARRIERS;i++)
    Pr_per_subcarrier[i]=0.0;

  int n =0; 

  //Retrieve interface configuration
  receive_combining = phy->getMIMORxScheme();

  //Retrieve packet information

  subChannelOffset = wimaxHdr->phy_info.subchannel_offset;
  numsubChannel = wimaxHdr->phy_info.num_subchannels;
  Channel_num = wimaxHdr->phy_info.channel_index;
  dir = (direction) wimaxHdr->phy_info.direction;

  //debug2("processing packet with cid [%d]\n",header.cid);

  //debug2("PropOFDMA: Incoming Packet info: subch_off[%d], subch_# [%d]\t sym_# [%d]\t sym_off [%d]\t direction[%d] \n",
	 //subChannelOffset,numsubChannel,wimaxHdr->phy_info.num_OFDMSymbol,wimaxHdr->phy_info.OFDMSymbol_offset,dir);


  //get subchannel and slot information
  num_subchannel = phy->getNumsubchannels (dir);
  num_data_subcarrier = phy->getNumSubcarrier (dir);
  num_symbol_per_slot = phy->getSlotLength (dir);

  //debug2("         : num_subchannel :%d, num_data_subcarrier (pilot included) :%d, num_symbol_per_slot :%d\n", num_subchannel, num_data_subcarrier, num_symbol_per_slot);

  int total_subcarriers=0; 
      if(wimaxHdr->phy_info.num_OFDMSymbol % num_symbol_per_slot == 0)   // chk this condition , chk whether broacastcid reqd ir not. 
	{	
	  if(wimaxHdr->phy_info.num_OFDMSymbol > num_symbol_per_slot) 
	    {
	      // for the first num_symbol_per_slot symbols 
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
		{
		  total_subcarriers += (num_subchannel*num_data_subcarrier) - ((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ) ;
		}

	      // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+num_symbol_per_slot ; i< (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot) ; i++)
		{
		  total_subcarriers +=  num_subchannel*num_data_subcarrier;
		} 

	      // last 3 
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i<  wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol ; i++)
		{
		  total_subcarriers += ((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier;	
		}
	    }
	  else 
	    {
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
		{
		  total_subcarriers += wimaxHdr->phy_info.num_subchannels*num_data_subcarrier ;//-  (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ;	
		}	   
	    }			
	}
     else
	{
	  // for the first num_symbol_per_slot symbols 
	  if(wimaxHdr->phy_info.num_OFDMSymbol > 1) 
	    {
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
		{
		  total_subcarriers += num_subchannel*num_data_subcarrier - (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ;
		}

	      // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+1 ; i< (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
		{
		  total_subcarriers +=  num_subchannel*num_data_subcarrier;
		}  

	      // last 3 
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-1 ; i< wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol ; i++)
		{
		  total_subcarriers += ((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset) ))%num_subchannel)*num_data_subcarrier;
		}
	    }
	  else
	    {
	      for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
		{
		  total_subcarriers +=    wimaxHdr->phy_info.num_subchannels*num_data_subcarrier;// - (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; 
		}	
	    }
	}
      //debug2("Total number of subcarrier for this  broadcast/Non-broadcast message is [%d] \n",total_subcarriers);

  //removing start here to chk without this whether thingsare working fine or not 

  if( (wimaxHdr->phy_info.OFDMSymbol_offset == 0 && wimaxHdr->phy_info.num_OFDMSymbol == 0) 
      /*|| wimaxHdr->header.cid == BROADCAST_CID*/) 

    {   // this kind of packets are treated diff(OFDM types) basically it a bw req packet. 
	//debug2("\n----------------In prop_OFDMA.cc Loop 1.\n");
      int number_of_data_subcarriers = 0;// wimaxHdr->phy_info.num_subchannels*num_data_subcarrier;
      if(total_subcarriers > NUM_DATA_SUBCARRIERS)
	{
	  number_of_data_subcarriers = NUM_DATA_SUBCARRIERS;
				
	  for(int i = 0 ; i< number_of_data_subcarriers; i++)
	    Pr_per_subcarrier[i] = (Pr_bulk/(number_of_data_subcarriers))* pow(ReceiveCombining(wimaxHdr->phy_info.channel_index, i+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining),2);
	}
       else
	{
	  number_of_data_subcarriers = total_subcarriers;
	  for(int i = 0 ; i< number_of_data_subcarriers; i++)
	    Pr_per_subcarrier[i] = (Pr_bulk/(number_of_data_subcarriers))* pow(ReceiveCombining(wimaxHdr->phy_info.channel_index, i+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining),2);
	}

      //  power calculations   -- for returning the recv power , we compute the total power over all the subcarriers 
      Pr_tot=0.0; 
      for (int i =0; i< number_of_data_subcarriers; i++)
	Pr_tot += Pr_per_subcarrier[i]; 

      debug10("The Pr_tot of this broadcast message is [%e]\n",Pr_tot);
      return Pr_tot; 
    }
  //bw request packet's recv power computation ends --------------------------------------------------------------------------------------------------------------
 
  //computation starts 

  Pr_subcarrier = new double[total_subcarriers] ;   
  for (int i = 0; i<total_subcarriers ; i++)
    Pr_subcarrier [i] = 0.0 ; 



  if(wimaxHdr->phy_info.num_OFDMSymbol % num_symbol_per_slot == 0)   // chk this condition , chk whether broacastcid reqd ir not. 
    {
      if(wimaxHdr->phy_info.num_OFDMSymbol > num_symbol_per_slot) 
	{
	  // for the first num_symbol_per_slot symbols 
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot) ; i++)
	    {
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
		{
		  Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		  Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		}
	    }
	    // Commented by Barun : 21-Sep-2011
	  //debug2("num_subchannel is [%d]\t num_data_subcarrier is [%d]\n", num_subchannel, num_data_subcarrier);
	  //debug2("xingting i [%d]\t j from[%d]-[%d]\n",wimaxHdr->phy_info.OFDMSymbol_offset + num_symbol_per_slot,(wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier,num_subchannel*num_data_subcarrier);

	  // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset+num_symbol_per_slot ; i< (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot) ; i++)
	    {
	      for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
		{
		  Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		  Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		} 
	    }
	    // Commented by Barun : 21-Sep-2011
	  //debug2("xingting i [%d]\t j[%d]\n",(wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot),num_subchannel*num_data_subcarrier);


	  // last 3 
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-num_symbol_per_slot ; i<  wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol ; i++)
	    {
	      for(int j = 0 ; j<((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier ; j++ )
		{
		  Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		  Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		}
	    }
	    // Commented by Barun : 21-Sep-2011
	  //debug2("xingting i [%d]\t j[%d]\n", wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol,((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel));
	}
      else 
	{
	  //for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + wimaxHdr->phy_info.num_OFDMSymbol) ; i++)
	    { 
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier) + wimaxHdr->phy_info.num_subchannels*num_data_subcarrier); j++ )
		{
		  if(j < NUM_SUBCARRIERS)
		    {
		      Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		      Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		    }
		  else
		    {
		      Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, (j+1)%NUM_SUBCARRIERS, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		      Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, (j+1)%NUM_SUBCARRIERS, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		    }
		}	
		// Commented by Barun : 21-Sep-2011
	      //debug2("xingting i [%d]\t j [%d]-[%d]\n",i,
		     //(wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier,
		     //(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier)+wimaxHdr->phy_info.num_subchannels*num_data_subcarrier));
	    }
	}
    }
  else
    {
      // for the first num_symbol_per_slot symbols 
      if(wimaxHdr->phy_info.num_OFDMSymbol > 1) 
	{
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++)
	    for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<num_subchannel*num_data_subcarrier ; j++ )
	      {
		Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
	      }
      // Commented by Barun : 21-Sep-2011
	  //debug2("xingting i [%d]\t j[%d]\n",(wimaxHdr->phy_info.OFDMSymbol_offset + 1),num_subchannel*num_data_subcarrier);

	  // except the last num_symbol_per_slot and first num_symbol_per_slot whatever is thr
	  for(int i = (wimaxHdr->phy_info.OFDMSymbol_offset+1) ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset) + (wimaxHdr->phy_info.num_OFDMSymbol-1) ; i++)
	    for(int j = 0 ; j<num_subchannel*num_data_subcarrier ; j++ )
	      {
		Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
	      }  
	  // Commented by Barun : 21-Sep-2011
	  //debug2("xingting i [%d]\t j[%d]\n",(wimaxHdr->phy_info.OFDMSymbol_offset),num_subchannel*num_data_subcarrier);

	  // last 3 
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol-1 ; i< wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol ; i++)
	    for(int j = 0 ; j<((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel)*num_data_subcarrier ; j++ )
	      {
		Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
	      }
	  // Commented by Barun : 21-Sep-2011
	  //debug2("xingting i [%d]\t j[%d]\n",wimaxHdr->phy_info.OFDMSymbol_offset +wimaxHdr->phy_info.num_OFDMSymbol,((wimaxHdr->phy_info.num_subchannels - (num_subchannel - (wimaxHdr->phy_info.subchannel_offset)) )%num_subchannel));
	}
      else
	{
	  int num_of_data_subcarriers = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier + wimaxHdr->phy_info.num_subchannels*num_data_subcarrier;
	  for(int i = wimaxHdr->phy_info.OFDMSymbol_offset ; i<  (wimaxHdr->phy_info.OFDMSymbol_offset + 1) ; i++) 
	    {
	      for(int j = (wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier ; j<(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier) + wimaxHdr->phy_info.num_subchannels*num_data_subcarrier) ; j++ )
		{
		  if(j < NUM_SUBCARRIERS)
		    {
		      Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		      Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		    }
		  else
		    {
		      Pr_per_subcarrier[j] = (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, j+1-NUM_SUBCARRIERS, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		      Pr_subcarrier[n++]= (Pr_bulk/(num_data_subcarrier)) * ReceiveCombining(wimaxHdr->phy_info.channel_index, (j+1)%NUM_SUBCARRIERS, ((OFDMAPhy*)ifp)->getNumAntenna(), receive_combining);
		    }
		}	
		// Commented by Barun : 21-Sep-2011
	      //debug2("xingting\t j [%d]-[%d]\n",
		     //(wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier,
		     //(((wimaxHdr->phy_info.subchannel_offset)*num_data_subcarrier)+wimaxHdr->phy_info.num_subchannels*num_data_subcarrier));
	    }
	}
    }

  // computation ends 
  //debug2 (" n = %d total_subcarriers = %d  \n", n, total_subcarriers ); 
 
  Pr_tot=0.0; 
  for (int i =0; i< n ; i++)
      Pr_tot += Pr_subcarrier[i];   // for the receive power,  I add all the powers on the subcarriers. 

  //cout << "Pr_bulk = " << Pr_bulk<< endl;
  //cout << "Pr_tot_my = " << Pr_tot << endl; 


  for(int i= 0; i<NUM_SUBCARRIERS; i++) 
  {
	p->txinfo_.RxPr_OFDMA[i] = Pr_per_subcarrier[i]; 
  }

  delete [] Pr_subcarrier;
  Pr_subcarrier = NULL;
  return Pr_tot;
}


// intference power calculation 


// SINR
void
PropOFDMA::addPower( intpower *pwr, double duration ) {
  PacketRecord *ptr;

  // Create a new packet record and set the timer and power for it
  ptr = new PacketRecord(this);
  // (double *)(ptr->power) = pwr;
        
  for (int i =0; i<MAX_OFDM_SYMBOLS; i++)
    for (int j=0;j<NUM_SUBCARRIERS;j++)
      ptr->power[i][j] =pwr[i][j];
  //	for (int i=0;i<NUM_SUBCARRIERS;i++)
  //      ptr->power[i] = pwr[i];        

  ptr->timer.sched( duration );
  //debug2("\t scheduled interference event\n");
  // Add the new object to the tail of the list (ensures that packet powers
  // will always be added in the same order (probably doesn't matter but...))
  ptr->prev = tail_pkt_;
  ptr->next = NULL;
  if (head_pkt_ == NULL) {
    head_pkt_ = ptr;
  }
  if (tail_pkt_ != NULL) {
    tail_pkt_->next = ptr;
  }
  tail_pkt_ = ptr;

  // Update the total power being received
  for (int i =0; i<MAX_OFDM_SYMBOLS; i++)
    for (int j=0;j<NUM_SUBCARRIERS;j++)
      total_power_[i][j] += pwr[i][j];
  //    if (phy_addr() <= 6)debug2("\t\t Addpower at node: %d, total_power_: %.25f\n",phy_addr(),total_power_);
}



void
PropOFDMA::removePower( PacketRecord *expired ) {

  PacketRecord *ptr;
  assert( head_pkt_ != NULL );
  assert( tail_pkt_ != NULL );
  // Remove the expired packet 
  if (expired->prev == NULL) {
    head_pkt_ = expired->next;
  } else {
    expired->prev->next = expired->next;
  }
  if (expired->next == NULL){
    tail_pkt_ = expired->prev;
  } else {
    expired->next->prev = expired->prev;
  }

  // Cannot delete the packet record right away because the TimerHandler class
  // has to perform some more housekeeping.  So we move the packet to the
  // "trash" so it can be deleted the next time a packet expires.

  // If there is a packet in the trash, delete it.
  if (trash_pkt_ != NULL) {
    delete trash_pkt_;
  }
  // Move the expired packet to the trash.
  trash_pkt_ = expired;

  // Recompute the total power to prevent roundoff accumulation
  //total_power_ = BASE_POWER;
  for (int i =0; i<MAX_OFDM_SYMBOLS; i++)
    for (int j=0;j<NUM_SUBCARRIERS;j++)
      total_power_[i][j] = 0.0;//ptr->power[i];
  ptr = head_pkt_;
  while (ptr != NULL) {
    for (int i =0; i<MAX_OFDM_SYMBOLS; i++)
      for (int j=0;j<NUM_SUBCARRIERS;j++)
	total_power_[i][j] += ptr->power[i][j];
    //total_power_ += ptr->power;
    ptr = ptr->next;
  }
  //     if (phy_addr() <= 6)debug2("\t\t Removed power : %.25f at node: %d remaining pwr: %.25f\n",trash_pkt_->power,phy_addr(),total_power_);
}
// SINR_


int
PropOFDMA::GetRandChannel( int Channel_Num ) {

  if (Channel_Num >= NUM_REALIZATIONS )
    { 
      return (Channel_Num - (NUM_REALIZATIONS - 1) + 1);
    }
  else
    return Channel_Num;
}

double
PropOFDMA::ReceiveCombining(int ChannelId, int NumSubcarrier, int NumAntenna, int Scheme) {


  if (Scheme == 0) { 
    if(NumAntenna == 1) return channel_gain[ChannelId][NumSubcarrier];
    else if (NumAntenna == 2){
      //debug2(" \n MIMO Check - inside selection combining chk 1st channel = %.4f 2nd channel = %.4f  \n ", channel_gain[ChannelId][NumSubcarrier],channel_gain[(GetRandChannel(ChannelId + rand_channel_MIMO))][NumSubcarrier]); 	
      if(channel_gain[ChannelId][NumSubcarrier]>channel_gain[(GetRandChannel(ChannelId + rand_channel_MIMO))][NumSubcarrier])
	return pow(channel_gain[ChannelId][NumSubcarrier],2);
      else return pow(channel_gain[(GetRandChannel(ChannelId + rand_channel_MIMO))][NumSubcarrier],2);		
    }

  }
  else if(Scheme == 1) {
    if(NumAntenna == 1) return channel_gain[ChannelId][NumSubcarrier];
    else if (NumAntenna == 2){
      // debug2(" \n MIMO Check - inside maximal ratio combining chk 1st channel = %.4f 2nd channel = %.4f  \n ", channel_gain[ChannelId][NumSubcarrier],channel_gain[(GetRandChannel(ChannelId + rand_channel_MIMO))][NumSubcarrier]); 	
      //if(channel_gain[ChannelId][NumSubcarrier]>channel_gain[(GetRandChannel(ChannelId + rand_channel_MIMO))][NumSubcarrier])
      return ((channel_gain[ChannelId][NumSubcarrier] * channel_gain[ChannelId][NumSubcarrier]) + pow(channel_gain[(GetRandChannel(ChannelId + rand_channel_MIMO))][NumSubcarrier],2));
      //else return channel_gain[(GetRandChannel(ChannelId + rand_channel_MIMO))][NumSubcarrier];		
    }

  }
  return 0.0; //what should be the wrong value?
}


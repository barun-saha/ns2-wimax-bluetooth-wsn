/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) Xerox Corporation 1997. All rights reserved.
 *  
 * License is granted to copy, to use, and to make and to use derivative
 * works for research and evaluation purposes, provided that Xerox is
 * acknowledged in all documentation pertaining to any such copy or derivative
 * work. Xerox grants no other licenses expressed or implied. The Xerox trade
 * name should not be used in any advertising without its written permission.
 *  
 * XEROX CORPORATION MAKES NO REPRESENTATIONS CONCERNING EITHER THE
 * MERCHANTABILITY OF THIS SOFTWARE OR THE SUITABILITY OF THIS SOFTWARE
 * FOR ANY PARTICULAR PURPOSE.  The software is provided "as is" without
 * express or implied warranty of any kind.
 *  
 * These notices must be retained in any copies of any part of this software.
 */

/*
 *  VoIP Traffic Generator - G.Stea, University of Pisa, Nov 2003.
 *  Ref: "Traffic Analysis for Voice over IP - Cisco documents
*/

#include <voip.h>
#include <stdlib.h>
#include <iostream>

static class VoIP_TrafficClass : public TclClass {
 public:
	VoIP_TrafficClass() : TclClass("Application/Traffic/VoIP") {}
	TclObject* create(int, const char*const*) {
		return (new VoIP_Traffic());
	}
} class_VoIP_traffic;


VoIP_Traffic::VoIP_Traffic() : Ontime_ (1.0, 1.0, 1.0), Offtime_ (1.0, 1.0, 1.0)
{
	bind("HeaderCompression_", &HeaderCompression_);
	bind("VAD_", &VAD_);
	codec_no=G711;             /* default: G.711 codec */
	Ontime_.setscale(1.423);  /* default: one-to-one conversation */
	Ontime_.setshape(0.824);
	Offtime_.setscale(0.899);
	Offtime_.setshape(1.089);
	Ontime_.setlocation (0.0); /* set the location to 0 */
	Offtime_.setlocation (0.0);
}


void VoIP_Traffic::init()
{
        // compute inter-packet interval 
	size_ = (HeaderCompression_>0)? codec[codec_no].payload + 2 : codec[codec_no].payload + 40;
	interval_ = 1./codec[codec_no].pps;
	if (agent_)
		agent_->set_pkttype(PT_EXP);

	burst_len = 0;
}


void VoIP_Traffic::start()
{
        init();
        running_ = 1;
        timeout();
}


double VoIP_Traffic::next_interval(int& size)
{
	// You NEED this line. Bound variables may change during the execution...
	size_ = (HeaderCompression_>0)? codec[codec_no].payload + 2 : codec[codec_no].payload + 40;

	interval_ = 1./codec[codec_no].pps;
	double t = interval_;

	if (VAD_)
	{
		if (burst_len == 0) {

			/* compute number of packets in next burst */
			burst_len = int (.5 + Ontime_.value() / interval_);

			/* make sure we got at least 1 */
			if (burst_len == 0)
				burst_len = 1;

			/* start of an idle period, compute idle time */
			t += Offtime_.value();			

		}	
		burst_len--;	
	}

	size = size_;
	return (t);
}


void VoIP_Traffic::timeout()
{
	if (! running_)
		return;

	/* send a packet */

	if (nextPkttime_ != interval_ || nextPkttime_ == -1) 
		agent_->sendmsg(size_, "NEW_BURST");
	else 
		agent_->sendmsg(size_);

	/* figure out when to send the next one */
	nextPkttime_ = next_interval(size_);

	/* schedule it */
	if (nextPkttime_ > 0)
		timer_.resched(nextPkttime_);
}



int VoIP_Traffic::command(int argc, const char*const* argv){
        

        if(argc==2){
                if (strcmp(argv[1], "show-codec") == 0) {
			fprintf (stdout, "\nCodec name: %s;\npayload: %i bytes;\npackets per second: %g;\n", codec[codec_no].name, codec[codec_no].payload, codec[codec_no].pps);
			fprintf (stdout, "Voice Activity Detection: %s; Header Compression: %s\n", VAD_? "ON":"OFF", HeaderCompression_? "ON":"OFF" );
			double bw = (codec[codec_no].payload+ (HeaderCompression_? 2 : 40)) * codec[codec_no].pps * (VAD_? Ontime_.avg()/(Ontime_.avg()+Offtime_.avg()) : 1.) * 8; 
			fprintf (stdout, "Average bandwidth: %g bps\n", bw);
                        return (TCL_OK);
        	}
                if (strcmp(argv[1], "show-all-codecs") == 0) {
			fprintf (stdout, "\n");
			for (int j=0; j<N_CODEC; j++)
			{
				fprintf (stdout, "%s: payload: %i bytes; packets per second: %g;", codec[j].name, codec[j].payload, codec[j].pps);
				if (j==codec_no)
					fprintf (stdout, " [currently being used]");
				fprintf (stdout, "\n");
			}
                        return (TCL_OK);
        	}
					 if (strcmp(argv[1], "vad") == 0) {
						 VAD_ = 1;
						 return (TCL_OK);
					 }
					 if (strcmp(argv[1], "novad") == 0) {
						 VAD_ = 0;
						 return (TCL_OK);
					 }
	}
        else if(argc==3){
                if (strcmp(argv[1], "use-rng") == 0) {
                        Ontime_.seed((char *)argv[2]);
                        Offtime_.seed((char *)argv[2]);
                        return (TCL_OK);
                }
                if (strcmp(argv[1], "use-codec") == 0) {
			codec_no=-1;
			for (int i=0; i<N_CODEC; i++)
                        	if (!strcmp (codec[i].name,(char *)argv[2])) 
				{	
					codec_no=i;
					break;
				}
			if (codec_no == -1) {
				fprintf (stderr,
					"\nVOIP traffic: unknown codec %s.\n", (char *)argv[2]);
			}
        	        return (TCL_OK);
                }
					 if (strcmp(argv[1], "use-vad-model") == 0) {
						 if (strcmp(argv[2], "one-to-one") == 0) {
								Ontime_.setscale(1.423);
								Ontime_.setshape(0.824);
								Offtime_.setscale(0.899);
								Offtime_.setshape(1.089);
						 } else if (strcmp(argv[2], "many-to-many") == 0) {
								Ontime_.setscale(2.184);
								Ontime_.setshape(0.435);
								Offtime_.setscale(3.093);
								Offtime_.setshape(0.450);
						 } else if (strcmp(argv[2], "one-to-many") == 0) {
								Ontime_.setscale(23.952);
								Ontime_.setshape(1.278);
								Offtime_.setscale(3.941);
								Offtime_.setshape(0.820);
						 } else if (strcmp(argv[2], "many-to-one") == 0) {
								Ontime_.setscale(3.342);
								Ontime_.setshape(0.732);
								Offtime_.setscale(44.267);
								Offtime_.setshape(0.432);
						 } else {
							 fprintf (stderr,
								 "\nVOIP traffic: unknown VAD model %s.\n", argv[2]);
						 }
						 return (TCL_OK);
					 }

        }
        return Application::command(argc,argv);
}

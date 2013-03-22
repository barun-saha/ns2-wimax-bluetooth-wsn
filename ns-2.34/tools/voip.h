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

#ifndef ns_voip_h
#define ns_voip_h

#include <stdlib.h>
#include <math.h>
#include <random.h>
#include <trafgen.h>
#include <ranvar.h>

//! Codec name macros.
enum Codec_name {G711, G729, G729_2, G723, G723_2};

//! Codec structure.
/*!
  Includes the codec name, payload length (bytes) and
  number of packets per second.
  */
struct Codec {
   char name[8];		// to be used in "command"
	int payload;		// no header length here
	double pps;		// packets per second
};

//! Available codecs.
/*!
  G.711   (64kbps): 80 bytes frame, 160 bytes payload, 50 pkts per sec. 
  G.729   (8 kbps): 10 bytes frame, 20 bytes payload, 50 pkts per sec. 
  G.729.2 (8 kbps): 10 bytes frame, 30 bytes payload, 33 pkts per sec. 
  G.723   (6.3 kbps): 30 bytes frame, 30 bytes payload, 26 pkts per sec. 
  G.723.2 (5.3 kbps): 30 bytes frame, 30 bytes payload, 22 pkts per sec. 
  */
const Codec 
	codec[]= {
			{"G.711",   160, 50.0},
			{"G.729",   20,  50.0},
			{"G.729.2", 20,  33.0},
			{"G.723",   30,  26.0},
			{"G.723.2", 30,  22.0}
		};

//! Number of available codecs.
const int N_CODEC = 5;

//! VoIP traffic source.
/*!
  Three CISCO codecs (some with 2 variants - improperly
  accounted for as ".2"'s) are considered: G.711, G.729, G.723.
  The list of the available codecs is printed using the method show-all-codecs,
  while the currently used codec parameters are printed with the
  show-codec method.

  Voice Activity Detection (VAD) can be enabled (disabled) using the
  TCL method vad (novad) or setting the corresponding bound variable VAD_.
  Header compression can be enabled (disabled) using the TCL method
  compression (nocompression) or setting the corresponding bound variable
  HeaderCompression_.

  When VAD is selected, talkspurt and silence duration depends on the specific
  VAD model. Available VAD models are: one-to-one, many-to-many, one-to-many,
  many-to-one.
  40 bytes IP/UDP/RTP headers are added to the payload. No Layer 2 header
  considered.
  A specific RNG object may set used to generate talkspurt and silence
  periods with the method use-rng.
 */
class VoIP_Traffic : public TrafficGenerator {
 public:
	//! Initialize the traffic parameters (G.711, no VAD).
	VoIP_Traffic();
	//! Compute the next talkspurt and silence period duration.
	virtual double next_interval(int&);
	//! Called when the previous burst expired.
	virtual void timeout();
	//! Invocation of methods from TCL.
	int command(int argc, const char*const* argv);
	//! HACK so that udp agent knows interpacket arrival time within a burst.
	inline double interval() { return (interval_); }
 protected:
	//! Start traffic generation (called from the TCL scripts).
	virtual void start();
	//! Initialize the packet generation routine.
	void init();
	//! Packet inter-arrival time (sec).
	double interval_;
	//! If nonzero, Voice Activity Detection is enabled.
	int VAD_;
	//! If nonzero, Header Compression is enabled.
	int HeaderCompression_;
	//! Current codec.
	int codec_no;
	//! Duration of a talkspurt.
	int burst_len;	

	//! Weibull random variable used for the talkspurt periods.
	WeibullRandomVariable Ontime_;
	//! Weibull random variable used for the silence periods.
	WeibullRandomVariable Offtime_;
};

#endif // ns_voip_h

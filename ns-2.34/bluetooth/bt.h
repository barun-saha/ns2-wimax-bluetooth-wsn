/*
 * Copyright (c) 2004, University of Cincinnati, Ohio.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the OBR Center for 
 *      Distributed and Mobile Computing lab at the University of Cincinnati.
 * 4. Neither the name of the University nor of the lab may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef __ns_bt_h__
#define __ns_bt_h__

#include "bt-stat.h"

#define BTVERSION "BT Version: 0.9"
// #define BTSPEC		11	// spec 1.1
#define BTSPEC		12	// spec 1.2

#define MAX_SLAVE_PER_PICONET 7	


#define PAGERESPTO	16	// 8 slots specs
#define INQRESPTO	256	// 128 slots  specs
#define NEWCONNECTIONTO	64	// 32 slots specs
#define SUPERVISIONTO	20	// sec. HCI default to 20s maximum 40.9s
#define BACKOFF		2046
#define BACKOFF_s	254

#define T_GAP_100	32768	// 10.24s inquiry span
#define T_GAP_101	34	// 10.625ms  -- min scan window
#define T_GAP_102	8192	// 2.56s  -- max T_inquiry_scan
#define T_GAP_103	98304	// 30.72s -- mininum time to be discoverable
#define T_GAP_104	196608	// 1 min (30.72s *2) -- limited disc mode
#define T_GAP_105	320	// 100ms -- T_inquiry_scan 
#define T_GAP_106	320	// 100ms -- T_page_san 
#define T_GAP_107	4096	// 1.28s -- max T_page_san in R1 mode 
#define T_GAP_108	8192	// 2.56s -- max T_page_san in R2 mode 
////// the above are defined in specs

#define INQUIRYTO 	T_GAP_100	// HCI // 10.24s
#define PAGETO 		16384	// 5.12

#define TINQUIRYSCAN	8192	// 2.56s	-- HCI default
#define TWINQUIRYSCAN	36	// 11.25ms	-- HCI default
#define NINQUIRY_0SCO	256
#define NINQUIRY_1SCO	512
#define NINQUIRY_2SCO	768
#define NINQUIRY	NINQUIRY_0SCO
#define INQUIRYHCIUNIT	4096	// 1.28s

#define TWPAGESCAN	36	// 11.25ms	-- HCI default
#define TPAGESCAN 	4096	// 1.28s	-- HCI default
#define NPAGE_R0	1
#define NPAGE_R1	128
#define NPAGE_R2	256
#define NPAGE		NPAGE_R1

#define GIAC 		0x9e8b33
#define LIAC 		0x9e8b00

#define BT_PASSIVE_LISTEN_AC 		0x9e8b09

// LMP default
// #define AFH_mode	AFH_disabled
// #define AFH_reporting_mode	AFH_reporting_disabled
// #define drift	250
// #define jiter	10
// #define mac_slots	1
// #define poll_interval 	40

#define L2CAP_SIG_CID	((L2CAPChannel *) 0x01)
#define L2CAP_BCAST_CID	((L2CAPChannel *) 0x02)

#define L2CAP_IFQ	"Queue/DropTail"
#define L2CAP_IFQ_LIMIT	-1
// #define L2CAP_IFQ_LIMIT	2

// #define BD_ADDR_BCAST   0xFFFFFFFF
#define BD_ADDR_BCAST   -1

// Normally, the range of BT device is within 10 meter. The transmission
// delay less than 10/3e8 = 3.33e-8 sec.
// Consider the +-10ms grace period for slot misalignment. 
// We set BTDELAY to 10e-6 to simplify receiving handling code.
// The receiver only need to check if the imcomping pkt is within
// 0-20ms of the starting part of the receiving slot.
// Maybe 'BTDELAY' is not the right term. to be fixed.
#define MAX_SLOT_DRIFT	20E-6	// +-10ms. has to sychronize within 250 slots
#define BTDELAY 		10.03E-6	// transmission delay 

#define BT_CLKN_CLK_DIFF 	1e-8	// clk is a little bit later than clkn
// #define SYNSLOTMAX 40	
#define SYNSLOTMAX 400	
#define CHSYNSCHRED 300	
// #define CHSYNSCHRED1 100	
#define CHSYNSCHRED1 20	

#define PKT_MAX_SLOT 		5
#define MAX_PKT_TX_TIME		.003125   // 5 slots
#define BTInterferQueMaxLen	1024

#define BT_NUM_BCAST_RETRAN 	2	// don't change this if using PLSF
#define BT_TRANSMIT_MAX_RETRAN 	9999999   // Inifinity

#define DEF_TSNIFF	128
#define DEF_SNIFFATTEMPT	64
#define DEF_SNIFFTIMEOUT	0

#define DEF_HOLDT	256
#define MIN_HOLDT	18
#define MAX_HOLDT	400
#define DEF_NULL_TRGR_SCHRED	4
#define DEF_POLL_TRGR_SCHRED	4

// used to define clk drift
#define BT_DRIFT_NORMAL_STD		10E-6

// Different nodes may have different views because of clock drift.
#define BTSlotTime	625E-6
#define CLKDRFT		40E-6	// +-20ppm
#define CLKDRFT_SLEEP	500E-6	// +-250ppm

#define IDLECHECKINTV 0.1	// sec
#define IDLESCHRED 0.5	// sec

// in the process of RP adjustment, AWMMF will be reset, in the 
// transitive period, DRR is used.
#define BTAWMMF_RESET_LEN 0.4	// sec
#define BTAWMMF_ALPHA	0.3	
// #define BTAWMMF_DELTA	6	
#define BTAWMMF_DELTA	12	
#define BTAWMMF_DELTA_INC	64
#define FAILS_SCHRED_FOR_TPOLL_DBL 8	
#define NULL_SCHRED_FOR_TPOLL_DBL 1	
#define RS_REPEAT_COUNT	4

// for TxBuffer type
#define ACL 0
#define SCO 1
#define BBSIG 2

#define BTDISCONN_LINKDOWN	0
#define BTDISCONN_TIMEOUT	1
#define BTDISCONN_RECONN	2

#define L_CH_UD			0
#define L_CH_L2CAP_CONT		1
#define L_CH_L2CAP_START	2
#define L_CH_LM			3

#define VARIABLE_PKT

#define INQ_MIN_PERIOD_LEN 30
#define INQ_MAX_PERIOD_LEN 50
#define INQ_NUM_RESP        7

#define MAX_INQ_CALLBACK        4

#define PAGESTARTTO	256
#define INQSTARTTO	256
#define SCANSTARTTO	256

#define NB_TIMEOUT	90000	// Met witin the time frame. Infinity
#define NB_RANGE	10	// being within to count as a neiboughor

#define MAX_T_POLL          	400
#define T_POLL_DEFAULT          6
#define PRR_MIN_PRIO_MINUS_ONE 	-9999

#define PRIO_BR		4
#define PRIO_PANU	0

#define PMADDRLEN	256
#define ARADDRLEN	256

#define BTMAXHOPS 128
#define BTMAXPKTNUM 12800

// effective radio range. meter.
// Used by BlueHoc
// #define BT_EFF_DISTANCE		24	
#define BT_EFF_DISTANCE		11.2	

// packets within this range collide.
// It's resettable at runtime.
// #define BT_INTERFERE_DISTANCE	48	
#define BT_INTERFERE_DISTANCE	22	

/* BTNode will initially be placed in (BT_RANGE X BT_RANGE), unless otherwise
 * specified.
 */
#define BT_RANGE		7 // So all nodes are within 10m distance

// Intial clkn is set randomly within [0, 2^24=16,777,216)
// It should not be too large since clk wrap around is not handled.
#define CLK_RANGE		(0x01 << 24)	

// Used to control trace format.
#ifndef BTPREFIX
// #define BTPREFIX	"b "
#define BTPREFIX	
#endif
#ifndef BTPREFIX1
#define BTPREFIX1	BTPREFIX
#endif

#ifndef BTTXPREFIX
#define BTTXPREFIX	't'	
#endif
#ifndef BTRXPREFIX
#define BTRXPREFIX	'r'	
#endif
#ifndef BTINAIRPREFIX
#define BTINAIRPREFIX	'a'	
#endif
#ifndef BTLOSTPREFIX
#define BTLOSTPREFIX	'l'	
#endif
#ifndef BTCOLLISONPREFIX
#define BTCOLLISONPREFIX	'c'	
#endif
#ifndef BTERRPREFIX
#define BTERRPREFIX	'e'	
#endif

#ifndef L2CAPPREFIX0
#define L2CAPPREFIX0	"gen l2cmd "
#endif

#ifndef L2CAPPREFIX1
#define L2CAPPREFIX1	"l2cmd "
#endif

#ifndef BNEPPREFIX0
#define BNEPPREFIX0	"bv "
#endif
#ifndef BNEPPREFIX1
#define BNEPPREFIX1	"b^ "
#endif

#ifndef SCOAGENTPREFIX0
#define SCOAGENTPREFIX0	"sv "
#endif
#ifndef SCOAGENTPREFIX1
#define SCOAGENTPREFIX1	"s^ "
#endif
#ifndef SCOAGENTPREFIX2
#define SCOAGENTPREFIX2	"ScoAgent "
#endif

#define PSM_BNEP 	0x0F
#define PSM_SDP 	0x01

#define ETHER_PROT_SCAT_FORM 0x10	// different from inet/arp

#define BNEP_MAX_EXT_HEADER 1
#define SDP_PARAM_SIZE	64

// Assigned number for SDP

#define BASE_UUID	"00000000-0000-1000-8000-00805F9B34FB"	// 128 bit

// Protocols
#define prSDP		0x0001
#define prUDP		0x0002
#define prTCP		0x0004
#define prIP		0x0009
#define prFTP		0x000A
#define prHTTP		0x000C
#define prBNEP		0x000F
#define prL2CAP		0x0100

#define brwsGroupID	0x0200

// Service classes
#define scServDiscServerServClassID	0x1000
#define scBrwsGrpDescriptorServClassID	0x1001
#define scPubBrwsGrp			0x1002
#define scAudioSource			0x110A
#define scAudioSink			0x110B
#define scPANU				0x1115
#define scNAP				0x1116
#define scGN				0x1117
#define scBR				0xFF01	// extension: Bridge Role
						// change when defined.

// Attribute Identifier codes Numeirc IDs
#define aiServiceRecordHandle		0x0000
#define aiServiceClassIDList		0x0001
#define aiServiceRecordState		0x0002
#define aiServiceID			0x0003
#define aiProtocolDescriptorList	0x0004
#define aiBrowseGroupList		0x0005
#define aiLanguageBaseAttributeIDList	0x0006
#define aiServiceInfoTimeToLive		0x0007
#define aiServiceAvailability		0x0008
#define aiBluetoothProfileDescriptorList	0x0009
#define aiDocumentationURL		0x000A
#define aiClientExecutableURL		0x000B
#define aiIconURL			0x000C

// Protocol Parameters
#define ppL2capPSM	1
#define ppTcpPort	1
#define ppUdpPort	1
#define ppBnepVersion	1
#define ppBnepSupportedNtwkPktTypLst 	2

// in ../config.h
// typedef int32_t nsaddr_t;
// typedef int32_t nsmask_t;
// struct ns_addr_t {int32_t addr_;int32_t port_;}

typedef int bd_addr_t;
typedef int clk_t;

// typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
// typedef int int32_t;
// typedef unsigned int uint32_t;
typedef long long int int64_t;
typedef unsigned long long int uint64_t;

typedef uint8_t uchar;

#endif				// __ns_bt_h__

# Script showing a layer 2 handover in 802.16
# @author rouil
# Scenario: Communication between MN and Sink Node with MN attached to BS.
# Notes:- In order to perform layer 2 handover, the BSs must share the same channel
#       - Additional mechanisms are required in order to continue a communication after a layer 2 handover
#         This is achieved by updating the MN address to simulate layer 3 handovers and traffic redirection.
#         We will provide the code as part of our implementation of IEEE 802.21.
#
# @version 1.1 12-26-06 : added support for handover on link going down
# @version 1.2 07-02-08 : changed script to OFDMA

#
# Topology scenario:
#
#
#	        |-----|          
#	        | MN0 |  2.0.1 
#	        |-----|        
#
#
#		  (^)                         (^)
#		   |                           |
#	    |--------------|            |--------------|
#           | Base Station | 2.0.0      | Base Station | 3.0.0
#           |--------------|            |--------------|
#	    	   |                           |
#	    	   |                           |
#	     |-----------|                     |
#            | Router |---------------------| 		
#            |-----------| 1.0.0
#                  |
#	     |-----------|                     |
#            | Sink node |---------------------| 		
#            |-----------| 0.0.0
#
# Explanations of scenario/information to follow the code:
# 1- Upon link going down (i.e power level decreased and crossed a
# threshold), the MS sends a scan request to its serving BS 

# 2- when the WimaxCtrlAgent located in the BS receives the scan
# request, it needs to respond to the SS. In case of no association or
# association without coordination, the BS handles the request locally
# and can respond right away. When there is coordination with other
# neighboring BSs, it cannot do that since it has to consult these
# BSs. That's why the code is commented in case 2, but you will see
# that it is sending synchronization request to the neighbor BSs. Case
# 3 as not been implemented.

# 3- When the MS receives the response, there is a delay as defined in the
# standard (note that the scanning does not start but it is set to
# PENDING). Also it checks for Association with coordination. If there
# is coordination, there are rendez-vous time that were defined by the
# serving BS (in the WimaxCtrlAgent) and we need to schedule timers so
# that we switch to the right channel at the right time.  
# After few frames the scanning will start then a serie of
# start/pause/resume/stop scanning depending on the number of scan
# iterations.

# 4- Handover: SS will send MSHO-REQ message to the serving BS:
# message type = MAC_MOB_MSHO_REQ. The data includes, id and mean RSSI
# of current serving BS; id and mean RSSI of neighbor BSs that where
# detected during scanning. The serving BS will response with MSHO-RSP
# to the SS: message type =MAC_MOB_BSHO_RSP, with a recommended target
# BS (the one with the strongest RSSI).

# 5- SS receives the response from serving BS and starts to HO
# with the recommended BS. If the recommended BS is
# different from the current serving BS, it will send a message to
# serving BS. 

#check input parameters
if {$argc != 0} {
	puts ""
	puts "Wrong Number of Arguments! No arguments in this topology"
	puts ""
	exit 
}

# set global variables
set output_dir .
set traffic_start 15
set traffic_stop  250
set simulation_stop 250

#define debug values
Mac/802_16 set debug_             0
Agent/WimaxCtrl set debug_        1

Mac/802_16 set print_stats_       0
Mac/802_16 set t21_timeout_       0.02 ;#20 
Mac/802_16/BS set dlratio_        .66
Mac/802_16/SS set dlratio_        .66
Mac/802_16 set ITU_PDP_           1
Mac/802_16 set lgd_factor_        10.0      ;#note: the higher the value the earlier the trigger 
Mac/802_16 set rxp_avg_alpha_     0.2      ;# the higher the value (max=1), the more sensitive the scan trigger
Agent/WimaxCtrl set adv_interval_ 1.0
Agent/WimaxCtrl set default_association_level_ 0
Agent/WimaxCtrl set synch_frame_delay_ 0.5

#Physical layer configuration
Phy/WirelessPhy set Pt_ 0.2 
Phy/WirelessPhy set RXThresh_ 1.90546e-16
Phy/WirelessPhy set CSThresh_ [expr 0.9*[Phy/WirelessPhy set RXThresh_]]
Phy/WirelessPhy set OFDMA_ 1 

# Parameter for wireless nodes
set opt(chan)           Channel/WirelessChannel    ;# channel type
set opt(prop)           Propagation/OFDMA          ;# radio-propagation model
set opt(netif)          Phy/WirelessPhy/OFDMA      ;# network interface type
set opt(mac)            Mac/802_16                 ;# MAC type
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set opt(ll)             LL                         ;# link layer type
set opt(ant)            Antenna/OmniAntenna        ;# antenna model
set opt(ifqlen)         50              	   ;# max packet in ifq
set opt(adhocRouting)   DSDV                       ;# routing protocol

set opt(x)		2000			   ;# X dimension of the topography
set opt(y)		2000			   ;# Y dimension of the topography

#defines function for flushing and closing files
proc finish {} {
        global ns tf output_dir nb_mn
        $ns flush-trace
        close $tf
	exit 0
}

#create the simulator
set ns [new Simulator]
$ns use-newtrace

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)

#open file for trace
set tf [open $output_dir/out.res w]
$ns trace-all $tf

# set up for hierarchical routing (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_ 4          			;# domain number
lappend cluster_num 1 1 1 1          			;# cluster number for each domain 
AddrParams set cluster_num_ $cluster_num
lappend eilastlevel 1 1 2 2 ;# number of nodes for each cluster 
AddrParams set nodes_num_ $eilastlevel
puts "Configuration of hierarchical addressing done"

# Create God
create-god 3				;# nb_mn + 2 (base station and sink node)

#creates the sink node in first addressing space.
set sinkNode [$ns node 0.0.0]
puts "sink node created"

set router [$ns node 1.0.0]
puts "router node created"

#create common channel
set channel [new $opt(chan)]

#creates the Access Point (Base station)
$ns node-config -mobileIP ON \
                 -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType Mac/802_16/BS \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop)    \
                 -phyType $opt(netif) \
                 -channel $channel \
                 -topoInstance $topo \
                 -wiredRouting ON \
                 -agentTrace ON \
                 -routerTrace ON \
                 -macTrace ON  \
                 -movementTrace OFF

#setup channel model
set prop_inst [$ns set propInstance_]
$prop_inst ITU_PDP PED_A

#puts "Configuration of base station"
set bstation [$ns node 2.0.0]  
$bstation random-motion 0
#provide some co-ord (fixed) to base station node
$bstation set X_ 50.0
$bstation set Y_ 50.0
$bstation set Z_ 0.0
[$bstation set mac_(0)] set-channel 0
#add MOB_SCN handler
set wimaxctrl [new Agent/WimaxCtrl]
$wimaxctrl set-mac [$bstation set mac_(0)]
$ns attach-agent $bstation $wimaxctrl
puts "Base Station 1 created"

set bstation2 [$ns node 3.0.0]  
$bstation2 random-motion 0
#provide some co-ord (fixed) to base station node
$bstation2 set X_ 800.0
$bstation2 set Y_ 50.0
$bstation2 set Z_ 0.0
[$bstation2 set mac_(0)] set-channel 1
#add MOB_SCN handler
set wimaxctrl2 [new Agent/WimaxCtrl]
$wimaxctrl2 set-mac [$bstation2 set mac_(0)]
$ns attach-agent $bstation2 $wimaxctrl2
puts "Base Station 2 created"

#Add neighbor information to the BSs
$wimaxctrl add-neighbor [$bstation2 set mac_(0)] $bstation2
$wimaxctrl2 add-neighbor [$bstation set mac_(0)] $bstation

# creation of the mobile nodes
$ns node-config -macType Mac/802_16/SS \
                -wiredRouting OFF \
                -macTrace ON  				;# Mobile nodes cannot do routing.

set wl_node [$ns node 2.0.1] 	;# create the node with given @.	
$wl_node random-motion 0			;# disable random motion
$wl_node base-station [AddrParams addr2id [$bstation node-addr]] ;#attach mn to basestation
$wl_node set X_ 300.0
$wl_node set Y_ 40.0
$wl_node set Z_ 0.0
set HAaddress [AddrParams addr2id [$bstation node-addr]]
[$wl_node set regagent_] set home_agent_ $HAaddress
$ns at 15.0 "$wl_node setdest 750.0 40.0 5.0"
$ns at 100.0 "$wl_node setdest 100.0 40.0 5.0"
[$wl_node set mac_(0)] set-channel 0
[$wl_node set mac_(0)] set-diuc 1
[$wl_node set mac_(0)] setflow UL 10000 BE 275 2 0 0.05 15 1 0 0 0 0 0 0 0 0 0 0 ;# setting up static flows 
[$wl_node set mac_(0)] setflow DL 10000 BE 275 2 0 0.05 15 1 0 0 0 0 0 0 0 0 0 0 ;# setting up static flows 
puts "wireless node created ..."			;# debug info

#create source traffic
#Create a UDP agent and attach it to node n0
set udp [new Agent/UDP]
$udp set packetSize_ 1500
$ns attach-agent $sinkNode $udp

# Create a CBR traffic source and attach it to udp0
set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 1000
$cbr set interval_ 0.5
$cbr attach-agent $udp

#create an sink into the sink node

# Create the Null agent to sink traffic
set null [new Agent/Null] 
$ns attach-agent $wl_node $null

# Attach the 2 agents
$ns connect $udp $null


# create the link between sink node and base station
$ns duplex-link $router $bstation 100Mb 3ms DropTail
$ns duplex-link $router $bstation2 100Mb 3ms DropTail
$ns duplex-link $sinkNode $router 100Mb 3ms DropTail

# Traffic scenario: here the all start talking at the same time
$ns at $traffic_start "$cbr start"
$ns at $traffic_stop "$cbr stop"

$ns at $simulation_stop "finish"
puts "Running simulation"
$ns run
puts "Simulation done."

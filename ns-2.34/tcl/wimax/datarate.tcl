# Test script to evaluate datarate in 802.16 networks.
# @author rouil
# Scenario: Communication between MN and Sink Node with MN attached to BS.

#
# Topology scenario:
#
#
#	        |-----|          
#	        | MN0 |                 ; 1.0.1 
#	        |-----|        
#
#
#		  (^)
#		   |
#	    |--------------|
#           | Base Station | 		; 1.0.0
#           |--------------|
#	    	   |
#	    	   |
#	     |-----------|
#            | Sink node | 		; 0.0.0
#            |-----------|
#
# Notes: 
# Traffic should not start before 25s for the following reasons:
# - Network Entry can be time consuming
#    - The time to discover the AP (i.e. receive DL_MAP) is fairly quick even
#      with channel scanning. In the order of few hundred ms.
#    - Default DCD/UCD interval is 5s. 
#    - Ranging/Registration is quick (<100ms)
# - Routing protocol used here is DSDV, with default updates interval of 15s.

#check input parameters
if {$argc != 2} {
    puts ""
    puts "Wrong Number of Arguments! 2 arguments for this script"
    puts "Usage: ns datarate.tcl modulation cyclic_prefix "
    puts "modulation: 1-7"
    puts "   1-OFDM_BPSK_1_2"
    puts "   2-OFDM_QPSK_1_2"
    puts "   3-OFDM_QPSK_3_4"
    puts "   4-OFDM_16QAM_1_2"
    puts "   5-OFDM_16QAM_3_4"
    puts "   6-OFDM_64QAM_2_3"
    puts "   7-OFDM_64QAM_3_4"
    puts "cyclic_prefix: 0.25, 0.125, 0.0625, 0.03125"
    exit 
}

# set global variables
set output_dir .
set traffic_start 25
set traffic_stop  35
set simulation_stop 50

# Configure Wimax
set diuc [lindex $argv 0]
Mac/802_16 set debug_ 0
Mac/802_16 set ITU_PDP_  1 ;#PED_A

#define coverage area for base station: 500m coverage 
Phy/WirelessPhy/OFDMA set g_ [lindex $argv 1]
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

set opt(x)		1100			   ;# X dimension of the topography
set opt(y)		1100			   ;# Y dimension of the topography

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
#puts "Topology created"

#open file for trace
set tf [open $output_dir/out_mod_$diuc.res w]
$ns trace-all $tf
#puts "Output file configured"

# set up for hierarchical routing (needed for routing over a basestation)
$ns node-config -addressType hierarchical
AddrParams set domain_num_ 2          			;# domain number
lappend cluster_num 1 1            			;# cluster number for each domain 
AddrParams set cluster_num_ $cluster_num
lappend eilastlevel 1 2 		;# number of nodes for each cluster (1 for sink and one for mobile node + base station
AddrParams set nodes_num_ $eilastlevel
puts "Configuration of hierarchical addressing done"

# Create God
create-god 2	

#creates the sink node in first address space.
set sinkNode [$ns node 0.0.0]
puts "sink node created"

#creates the Access Point (Base station)
$ns node-config -adhocRouting $opt(adhocRouting) \
                 -llType $opt(ll) \
                 -macType Mac/802_16/BS \
                 -ifqType $opt(ifq) \
                 -ifqLen $opt(ifqlen) \
                 -antType $opt(ant) \
                 -propType $opt(prop)    \
                 -phyType $opt(netif) \
                 -channel [new $opt(chan)] \
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

set bstation [$ns node 1.0.0]  
$bstation random-motion 0
#provide some co-ord (fixed) to base station node
$bstation set X_ 550.0
$bstation set Y_ 550.0
$bstation set Z_ 0.0
[$bstation set mac_(0)] set-channel 0
puts "Base-Station node created"

# creation of the mobile nodes
$ns node-config -macType Mac/802_16/SS \
                -wiredRouting OFF \
                -macTrace ON  				;# Mobile nodes cannot do routing.

set wl_node [$ns node 1.0.1] 	;# create the node with given @.	
$wl_node random-motion 0			;# disable random motion
$wl_node base-station [AddrParams addr2id [$bstation node-addr]] ;#attach mn to basestation
#compute position of the node
$wl_node set X_ 400.0
$wl_node set Y_ 550.0
$wl_node set Z_ 0.0
puts "wireless node created ..."
[$wl_node set mac_(0)] set-diuc $diuc

[$wl_node set mac_(0)] set-channel 0
#[$wl_node set mac_(0)] setflow UL 10000 BE 700 2 1 0.05 15 1 ;# setting up static flows 
[$wl_node set mac_(0)] setflow UL 10000 BE 275 2 0 0.05 15 1 0 0 0 0 0 0 0 0 0 0 ;# setting up static flows 
#create source traffic
#Create a UDP agent and attach it to node n0
set udp [new Agent/UDP]
$udp set packetSize_ 1500
$ns attach-agent $wl_node $udp

# Create a CBR traffic source and attach it to udp0
set cbr [new Application/Traffic/CBR]
$cbr set packetSize_ 1500
$cbr set interval_ 0.0008
$cbr attach-agent $udp

#create an sink into the sink node

# Create the Null agent to sink traffic
set null [new Agent/Null] 
$ns attach-agent $sinkNode $null

# Attach the 2 agents
$ns connect $udp $null

# create the link between sink node and base station
$ns duplex-link $sinkNode $bstation 100Mb 1ms DropTail

#Schedule start/stop of traffic
$ns at $traffic_start "$cbr start"
$ns at $traffic_stop "$cbr stop"

$ns at $simulation_stop "finish"
puts "Starts simulation"
$ns run
puts "Simulation done."

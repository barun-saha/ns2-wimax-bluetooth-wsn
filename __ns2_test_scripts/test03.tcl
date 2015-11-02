# Test for 802.16 scheduler.
# @author rouil
# @date 03/25/2007
# Test file for wimax
# Scenario: Communication between MN and Sink Node with MN attached to BS.
#           Using grep ^r out_mod.res | grep MAC | grep -c cbr you can see the number of
#           mac packets received at the BS.
#           Using grep ^s out_mod.res | grep MAC | grep -c cbr you can see the number of 
#           mac packets sent (200 packets). 
#           
#
# Topology scenario: Uplink transmission
#
#
#               |-----|          
#               | MN0 |                 ; 1.0.1 
#               |-----|        
#                  |
#                  |
#           |--------------|
#           | Base Station |            ; 1.0.0
#           |--------------|
#                   |
#                   |
#                  (v)
#            |-----------|
#            | Sink node |              ; 0.0.0
#            |-----------|
#
#
# Notes: 
# Traffic should not start before 25s for the following reasons:
# - Network Entry can be time consuming
#    - The time to discover the AP (i.e. receive DL_MAP) is fairly quick even
#      with channel scanning. In the order of few hundred ms.
#    - Default DCD/UCD interval is 5s. 
#    - Ranging/Registration is quick (<100ms)
# - Routing protocol used here is DSDV, with default updates interval of 15s.
#
# @modified on 08/20/2008 to use more realistic physical layer configuration
#                         look for ### BEGIN Physical ...
#                         also adjusted the transmit data rate to reduce simulation time



#defines function for flushing and closing files
proc finish {} {
    global ns tf output_dir nb_mn
    $ns flush-trace
    close $tf
    puts "Simulation done."
    exit 0
}

# set global variables
set seed 0      ;# seed
set diuc 1      ;# diuc profile [1-7]
set direction ul          ;# traffic direction
set distance 1000           ;# distance of the MS from the BS

global defaultRNG
$defaultRNG seed $seed

set nb_mn 1                     ;# A single mobile node
set packet_size 1500            ;# packet size in bytes at CBR applications 
set gap_size 0.0008                     ;# 15Mb/s
#we know lower modulations do not support high datarate, so no need to push it
if { $diuc == "1" } {
     set gap_size 0.004                     ;# 3Mb/s  
}
if { $diuc == "2" } {
     set gap_size 0.0024                     ;# 5Mb/s   
}
if { $diuc == "3" } {
     set gap_size 0.0017                     ;# ~7Mb/s   
}
if { $diuc == "4" } {
     set gap_size 0.0015                     ;# 8Mb/s   
}
if { $diuc == "5" } {
     set gap_size 0.0011                     ;# ~11Mb/s   
}
if { $diuc == "6" } {
    set gap_size 0.0008                     ;# 15Mb/s    
}
if { $diuc == "7" } {
    set gap_size 0.0008                     ;# 15Mb/s
}


puts "gap size=$gap_size"
set traffic_start 5
set traffic_stop  15
set simulation_stop 20

#define debug values
Mac/802_16 set debug_           1
Mac/802_16 set rtg_             20
Mac/802_16 set ttg_             20
Mac/802_16 set frame_duration_  0.005
Mac/802_16 set ITU_PDP_         2
Mac/802_16/BS set dlratio_      .66
Mac/802_16/SS set dlratio_      .66
Mac/802_16 set fbandwidth_      10e+6
Mac/802_16 set disable_interference_ 0

Phy/WirelessPhy/OFDMA set g_ 0.25

### BEGIN Physical layer configuration

#define coverage area for base station
#This configuration is based on the following product (randomly picked)
#http://www.proxim.com/downloads/products/mp16/DS_0806_MP16_3500_USHR.pdf
Antenna/OmniAntenna set X_ 0
Antenna/OmniAntenna set Y_ 0
Antenna/OmniAntenna set Z_ 20                   ;#height 
Antenna/OmniAntenna set Gt_ 100.0               ;#20 dBi
Antenna/OmniAntenna set Gr_ 63.1                ;#18 dBi
Phy/WirelessPhy set Pt_ 0.126                   ;# 126 mW
if { $diuc == "1" } {
    Phy/WirelessPhy set RXThresh_ 6.309573444801928e-13          ;#-92 dBm
}
if { $diuc == "2" } {
    Phy/WirelessPhy set RXThresh_ 1.2589254117941626e-12         ;#-89 dBm
}
if { $diuc == "3" } {
    Phy/WirelessPhy set RXThresh_ 1.995262314968876e-12         ;#-87 dBm
}
if { $diuc == "4" } {
    Phy/WirelessPhy set RXThresh_ 3.981071705534953e-12         ;#-84 dBm
}
if { $diuc == "5" } {
    Phy/WirelessPhy set RXThresh_ 7.94328234724281e-12         ;#-81 dBm
}
if { $diuc == "6" } {
    Phy/WirelessPhy set RXThresh_ 1.995262314968878e-11         ;#-77 dBm
}
if { $diuc == "7" } {
    Phy/WirelessPhy set RXThresh_ 3.162277660168371e-11         ;#-75 dBm
}

Phy/WirelessPhy set CSThresh_ [expr 0.9*[Phy/WirelessPhy set RXThresh_]]
Phy/WirelessPhy set OFDMA_ 1 

### end Physical layer configuration




# Parameter for wireless nodes
set opt(chan)           Channel/WirelessChannel    ;# channel type
set opt(prop)           Propagation/OFDMA          ;# radio-propagation model
set opt(netif)          Phy/WirelessPhy/OFDMA      ;# network interface type
set opt(mac)            Mac/802_16/BS              ;# MAC type
set opt(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set opt(ll)             LL                         ;# link layer type
set opt(ant)            Antenna/OmniAntenna        ;# antenna model
set opt(ifqlen)         50                         ;# max packet in ifq
set opt(adhocRouting)   DSDV                       ;# routing protocol

set opt(x)      5500                               ;# X dimension of the topography
set opt(y)      1100                               ;# Y dimension of the topography


#create the simulator
set ns [new Simulator]
$ns use-newtrace

#create the topography
set topo [new Topography]
$topo load_flatgrid $opt(x) $opt(y)
#puts "Topology created"

#open file for trace
set tf [open test03.tr w]
$ns trace-all $tf
#puts "Output file configured"

# set up for hierarchical routing (needed for routing over a basestation)
#puts "start hierarchical addressing"
$ns node-config -addressType hierarchical
AddrParams set domain_num_ 2                    ;# domain number
lappend cluster_num 1 1                         ;# cluster number for each domain 
AddrParams set cluster_num_ $cluster_num
lappend eilastlevel 1 [expr ($nb_mn+1)]         ;# number of nodes for each cluster (1 for sink and one for mobile nodes + base station
AddrParams set nodes_num_ $eilastlevel
puts "Configuration of hierarchical addressing done"

# Create God
create-god [expr ($nb_mn + 2)]                  ;# nb_mn + 2 (base station and sink node)
#puts "God node created"

#creates the sink node in first addressing space.
set sinkNode [$ns node 0.0.0]
#provide some co-ord (fixed) to base station node
$sinkNode set X_ 50.0
$sinkNode set Y_ 50.0
$sinkNode set Z_ 0.0
#puts "sink node created"

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
#puts "Configuration of base station"

#setup channel model
set prop_inst [$ns set propInstance_]
$prop_inst ITU_PDP PED_A
puts "after set pPDP"


set bstation [$ns node 1.0.0]  
$bstation random-motion 0
#puts "Base-Station node created"
#provide some co-ord (fixed) to base station node
$bstation set X_ 550.0
$bstation set Y_ 550.0
$bstation set Z_ 0.0
[$bstation set mac_(0)] set-channel 0

# creation of the mobile nodes
$ns node-config -macType Mac/802_16/SS \
    -wiredRouting OFF \
    -macTrace ON                ;# Mobile nodes cannot do routing.
for {set i 0} {$i < $nb_mn} {incr i} {
    set wl_node_($i) [$ns node 1.0.[expr $i + 1]]   ;# create the node with given @.    
    $wl_node_($i) random-motion 0           ;# disable random motion
    $wl_node_($i) base-station [AddrParams addr2id [$bstation node-addr]] ;#attach mn to basestation
    #compute position of the node
    $wl_node_($i) set X_ [expr 550.0+$distance]
    $wl_node_($i) set Y_ 550.0
    $wl_node_($i) set Z_ 0.0
    #$ns at 0 "$wl_node_($i) setdest 1060.0 550.0 1.0"
    puts "wireless node $i created ..."         ;# debug info

    [$wl_node_($i) set mac_(0)] set-channel 0
    [$wl_node_($i) set mac_(0)] set-diuc $diuc
    #[$wl_node_($i) set mac_(0)] setflow UL 10000 BE 700 2 1 0.05 15 1 ;# setting up static flows 
    [$wl_node_($i) set mac_(0)] setflow UL 10000 BE 275 2 0 0.05 15 1 0 0 0 0 0 0 0 0 0 0 ;# setting up static flows 
    [$wl_node_($i) set mac_(0)] setflow DL 10000 BE 275 2 0 0.05 15 1 0 0 0 0 0 0 0 0 0 0 ;# setting up static flows 

    #create source traffic
    #Create a UDP agent and attach it to node n0
    set udp_($i) [new Agent/UDP]
    $udp_($i) set packetSize_ 1500

    if { $direction == "ul" } {
        $ns attach-agent $wl_node_($i) $udp_($i)
    } else {
        $ns attach-agent $sinkNode $udp_($i)
    }

    # Create a CBR traffic source and attach it to udp0
    set cbr_($i) [new Application/Traffic/CBR]
    $cbr_($i) set packetSize_ $packet_size
    $cbr_($i) set interval_ $gap_size
    $cbr_($i) attach-agent $udp_($i)

    #create an sink into the sink node

    # Create the Null agent to sink traffic
    set null_($i) [new Agent/Null] 
    if { $direction == "ul" } {    
        $ns attach-agent $sinkNode $null_($i)
    } else {
        $ns attach-agent $wl_node_($i) $null_($i)
    }
    
    # Attach the 2 agents
    $ns connect $udp_($i) $null_($i)
}

# create the link between sink node and base station
$ns duplex-link $sinkNode $bstation 100Mb 1ms DropTail

# Traffic scenario: if all the nodes start talking at the same
# time, we may see packet loss due to bandwidth request collision
set diff 0.02
for {set i 0} {$i < $nb_mn} {incr i} {
    $ns at [expr $traffic_start+$i*$diff] "$cbr_($i) start"
    $ns at [expr $traffic_stop+$i*$diff] "$cbr_($i) stop"
}

$ns at $simulation_stop "finish"
# Run the simulation
puts "Running simulation for MS with distance $distance..."
$ns run

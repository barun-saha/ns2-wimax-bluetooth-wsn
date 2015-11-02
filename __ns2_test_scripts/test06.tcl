#############################################
#             Star over 802.15.4            #
#              (beacon enabled)             #
#      Copyright (c) 2003 Samsung/CUNY      #
# - - - - - - - - - - - - - - - - - - - - - #
#        Prepared by Jianliang Zheng        #
#         (zheng@ee.ccny.cuny.edu)          #
#############################################

# ======================================================================
# Define options
# ======================================================================
set val(chan)           Channel/WirelessChannel    ;# Channel Type
set val(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set val(netif)          Phy/WirelessPhy/802_15_4
set val(mac)            Mac/802_15_4
set val(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(ifqlen)         150                        ;# max packet in ifq
set val(nn)             7                          ;# number of mobilenodes
set val(rp)             AODV                       ;# routing protocol
set val(x)      50
set val(y)      50
set val(traffic)    ftp                        ;# cbr/poisson/ftp

set appTime1            7.0 ;# in seconds 
set appTime2            7.1 ;# in seconds 
set appTime3            7.2 ;# in seconds 
set appTime4            7.3 ;# in seconds 
set appTime5            7.4 ;# in seconds 
set appTime6            7.5 ;# in seconds 
set stopTime            100 ;# in seconds 

# Initialize Global Variables
set ns_     [new Simulator]
$ns_ use-newtrace
set tracefd     [open test06.tr w]
$ns_ trace-all $tracefd

Mac/802_15_4 wpanCmd verbose on
#Mac/802_15_4 wpanNam namStatus on      ;# default = off (should be turned on before other 'wpanNam' commands can work)
#Mac/802_15_4 wpanNam ColFlashClr gold      ;# default = gold

set dist(15m) 8.54570e-07
# For model 'TwoRayGround'

Phy/WirelessPhy set CSThresh_ $dist(15m)
Phy/WirelessPhy set RXThresh_ $dist(15m)

# set up topography object
set topo       [new Topography]
$topo load_flatgrid $val(x) $val(y)

# Create God
set god_ [create-god $val(nn)]

set chan_1_ [new $val(chan)]

# configure node

$ns_ node-config -adhocRouting $val(rp) \
        -llType $val(ll) \
        -macType $val(mac) \
        -ifqType $val(ifq) \
        -ifqLen $val(ifqlen) \
        -antType $val(ant) \
        -propType $val(prop) \
        -phyType $val(netif) \
        -topoInstance $topo \
        -agentTrace OFF \
        -routerTrace OFF \
        -macTrace ON \
        -movementTrace OFF \
                -energyModel "EnergyModel" \
                -initialEnergy 1 \
                -rxPower 0.3 \
                -txPower 0.3 \
        -channel $chan_1_ 

for {set i 0} {$i < $val(nn) } {incr i} {
    set node_($i) [$ns_ node]   
    $node_($i) random-motion 0      ;# disable random motion
}

#source ./wpan_demo2.scn

$node_(0) set X_ 25
$node_(0) set Y_ 25
$node_(0) set Z_ 0

$node_(1) set X_ 35
$node_(1) set Y_ 25
$node_(1) set Z_ 0

$node_(2) set X_ 29.9963
$node_(2) set Y_ 33.6624
$node_(2) set Z_ 0

$node_(3) set X_ 19.9927
$node_(3) set Y_ 33.656
$node_(3) set Z_ 0

$node_(4) set X_ 15
$node_(4) set Y_ 24.9874
$node_(4) set Z_ 0

$node_(5) set X_ 20.0146
$node_(5) set Y_ 16.3313
$node_(5) set Z_ 0

$node_(6) set X_ 30.0182
$node_(6) set Y_ 16.3503
$node_(6) set Z_ 0

$ns_ at 0.0 "$node_(0) NodeLabel PAN Coor"
$ns_ at 0.0 "$node_(0) sscs startPANCoord 1 6 0"        ;# startPANCoord <txBeacon=1> <BO=6> <SO=0>
$ns_ at 0.5 "$node_(1) sscs startDevice 1 0 0 6 0"  ;# startDevice <isFFD=1> <assoPermit=1> <txBeacon=0> <BO=6> <SO=0>
$ns_ at 1.5 "$node_(2) sscs startDevice 1 0 0 6 0"
$ns_ at 2.5 "$node_(3) sscs startDevice 1 0 0 6 0"
$ns_ at 3.5 "$node_(4) sscs startDevice 1 0 0 6 0"
$ns_ at 4.5 "$node_(5) sscs startDevice 1 0 0 6 0"
$ns_ at 5.5 "$node_(6) sscs startDevice 1 0 0 6 0"

#Mac/802_15_4 wpanNam PlaybackRate 3ms

$ns_ at $appTime1 "puts \"\nTransmitting data ...\n\""

# Setup traffic flow between nodes


proc ftptraffic { src dst starttime } {
   global ns_ node_
   set tcp($src) [new Agent/TCP]
   eval \$tcp($src) set packetSize_ 50
   set sink($dst) [new Agent/TCPSink]
   eval $ns_ attach-agent \$node_($src) \$tcp($src)
   eval $ns_ attach-agent \$node_($dst) \$sink($dst)
   eval $ns_ connect \$tcp($src) \$sink($dst)
   set ftp($src) [new Application/FTP]
   eval \$ftp($src) attach-agent \$tcp($src)
   $ns_ at $starttime "$ftp($src) start"
}
     
if { "$val(traffic)" == "ftp" } {
   puts "\nTraffic: ftp"
   #Mac/802_15_4 wpanCmd ack4data off
   puts [format "Acknowledgement for data: %s" [Mac/802_15_4 wpanCmd ack4data]]
   $ns_ at $appTime1 "Mac/802_15_4 wpanNam PlaybackRate 0.20ms"
   $ns_ at [expr $appTime1 + 0.5] "Mac/802_15_4 wpanNam PlaybackRate 1.5ms"
   ftptraffic 0 1 $appTime1
   ftptraffic 0 3 $appTime3
   ftptraffic 0 5 $appTime5
   $ns_ at $appTime1 "$ns_ trace-annotate \"(at $appTime1) ftp traffic from node 0 to node 1\""
   $ns_ at $appTime3 "$ns_ trace-annotate \"(at $appTime3) ftp traffic from node 0 to node 3\""
   $ns_ at $appTime5 "$ns_ trace-annotate \"(at $appTime5) ftp traffic from node 0 to node 5\""
   Mac/802_15_4 wpanNam FlowClr -p AODV -c tomato
   Mac/802_15_4 wpanNam FlowClr -p ARP -c green
   Mac/802_15_4 wpanNam FlowClr -p MAC -s 0 -d -1 -c navy
   Mac/802_15_4 wpanNam FlowClr -p tcp -s 0 -d 1 -c blue
   Mac/802_15_4 wpanNam FlowClr -p ack -s 1 -d 0 -c blue
   Mac/802_15_4 wpanNam FlowClr -p tcp -s 0 -d 3 -c green4
   Mac/802_15_4 wpanNam FlowClr -p ack -s 3 -d 0 -c green4
   Mac/802_15_4 wpanNam FlowClr -p tcp -s 0 -d 5 -c cyan4
   Mac/802_15_4 wpanNam FlowClr -p ack -s 5 -d 0 -c cyan4
}


# Tell nodes when the simulation ends
for {set i 0} {$i < $val(nn) } {incr i} {
    $ns_ at $stopTime "$node_($i) reset";
}

$ns_ at $stopTime "stop"
$ns_ at $stopTime "puts \"NS EXITING...\n\""
$ns_ at $stopTime "$ns_ halt"

proc stop {} {
    global ns_ tracefd appTime1 val env
    $ns_ flush-trace
    close $tracefd
    set hasDISPLAY 0
    foreach index [array names env] {
        #puts "$index: $env($index)"
        if { ("$index" == "DISPLAY") && ("$env($index)" != "") } {
                set hasDISPLAY 1
        }
    }
   
}

puts "\nStarting Simulation..."
$ns_ run

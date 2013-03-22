
set val(mac)            Mac/BNEP    ;# set MAC type to bluetooth
set val(nn)             2               ;# number of btnodes
# time table for each node to turn on
set StartTime [list 0.0 0.0006 0.1031 0.1134 0.3878 0.8531 0.6406 0.0627]

set ns	[new Simulator]
set f [open sco.tr w]
$ns trace-all $f
set nf [open sco.nam w]
$ns namtrace-all-wireless $nf 7 7
Simulator set MacTrace_ ON

$ns node-config -macType $val(mac) 	;# set node type to BTNode

for {set i 0} {$i < $val(nn) } {incr i} {
    set node($i) [$ns node $i ]		;# create new BTNode
    $node($i) rt AODV
    $ns at [lindex $StartTime $i] "$node($i) on" ;# turn on the node
}

$node(0) LossMod BlueHoc			;# turn on LossMod for all nodes

$node(0) pos 0 0			;# set position -- optional
$node(1) pos 0 7			;# set position -- optional

set sco0 [new Agent/SCO]
set sco1 [new Agent/SCO]
# $ns attach-agent $node(0) $sco0
# $ns attach-agent $node(1) $sco1
set cbr0 [new Application/Traffic/CBR]
set cbr1 [new Application/Traffic/CBR]
$cbr0 attach-agent $sco0
$cbr1 attach-agent $sco1

$cbr0 set packetSize_ 30		;# playload size for HV3
$cbr0 set interval_ [expr 625e-6 * 6]	;# T_poll = 6
$cbr1 set packetSize_ 30		;# playload size for HV3
$cbr1 set interval_ [expr 625e-6 * 6]	;# T_poll = 6

# $ns at 0.2 "$cbr0 start"

# set nscmd "$cbr0 start"

# at clock()=0.001, $node(0) make a SCO connection to $node(1), using
# HV3 packet.  When the connection is ready, the traffic generators start
# if they are defined.
$ns at 0.001 "$node(0) sco-connect $node(1) $sco0 $sco1"

#puts [$cbr0 set packetSize_]
#puts [$cbr0 set interval_]

$ns at 5.01 "$ns halt"

$ns run


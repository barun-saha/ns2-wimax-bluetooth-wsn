set val(mac)            Mac/BNEP    ;# set MAC type to bluetooth
set val(nn)             8               ;# number of btnodes

set ns	[new Simulator]
$ns node-config -macType $val(mac) 	;# set node type to BTNode

for {set i 0} {$i < $val(nn) } {incr i} {
    set node($i) [$ns node $i ]		;# create new BTNode
    $node($i) rt AODV
    $ns at 0.0 "$node($i) on" ;# turn on the node
    if {$i > 0} {
        $node($i) inqscan 4096 2048
        $node($i) pagescan 4096 2048
    }
}
#$node(0) set-statist 3 20 1
#$node(0) LossMod BlueHoc			;# turn on LossMod for all nodes
$node(0) CollisionDist 100
$node(0) trace-all-in-air on

#$node(0) pos 5 10			;# set position -- optional
#$node(1) pos 10 5			;# set position -- optional
#$node(2) pos 1 5			;# set position -- optional
#$node(3) pos 1 1			;# set position -- optional
#$node(4) pos 5 24			;# set position -- optional
#$node(4) radioRange   24			;# set position -- optional

# [$node(4) set bb_] set T_w_page_scan_ 4064
[$node(0) set lmp_] set scan_after_inq_ 0

set sco0 [new Agent/SCO]
set sco4 [new Agent/SCO]
set sco1 [new Agent/SCO]
set sco2 [new Agent/SCO]
$ns attach-agent $node(0) $sco0
$ns attach-agent $node(0) $sco1
$ns attach-agent $node(2) $sco2
$ns attach-agent $node(4) $sco4
set cbr0 [new Application/Traffic/CBR]
set cbr1 [new Application/Traffic/CBR]
set cbr2 [new Application/Traffic/CBR]
set cbr4 [new Application/Traffic/CBR]
$cbr0 attach-agent $sco0
$cbr1 attach-agent $sco1
$cbr2 attach-agent $sco2
$cbr4 attach-agent $sco4

$cbr0 set packetSize_ 30		;# playload size for HV3
$cbr0 set interval_ [expr 625e-6 * 6]	;# T_poll = 6
$cbr4 set packetSize_ 30		;# playload size for HV3
$cbr4 set interval_ [expr 625e-6 * 6]	;# T_poll = 6
$cbr1 set packetSize_ 30		;# playload size for HV3
$cbr1 set interval_ [expr 625e-6 * 6]	;# T_poll = 6
$cbr2 set packetSize_ 30		;# playload size for HV3
$cbr2 set interval_ [expr 625e-6 * 6]	;# T_poll = 6


# at clock()=0.001, $node(0) make a SCO connection to $node(1), using
# HV3 packet.  When the connection is ready, the traffic generators start
# if they are defined.
$ns at 0.1 "$node(0) sco-connect $node(4) $sco0 $sco4"
$ns at 0.1 "$node(0) sco-connect $node(2) $sco1 $sco2"

$ns at 3 "$node(0) inquiry 8 4 "

$ns at 15 "$node(0) sco-disconnect $sco0 0"

$ns at 16 "$node(0) inquiry 8 2 "

$ns at 35 "$node(0) sco-disconnect $sco1 0"

$ns at 36 "$node(0) inquiry 4 2 "

$ns at 42.0 "finish"

proc finish {} {
	global node
        # $node(0) print-all-stat
	exit 0
}

$ns run

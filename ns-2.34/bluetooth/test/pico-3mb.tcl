set val(mac)            Mac/BNEP    ;# set MAC type to bluetooth
set val(nn)             8               ;# number of btnodes

set ns	[new Simulator]
$ns node-config -macType $val(mac) 	;# set node type to BTNode

for {set i 0} {$i < $val(nn) } {incr i} {
    set node($i) [$ns node $i ]
    # $node($i) rt Manual
    $node($i) rt AODV
    $node($i) SchedAlgo DRR
    set lmp [$node($i) set lmp_]
    $ns at 0.0 "$node($i) on"       ;# An random delay up to 2 slots applied.
    $node($i) set-statist 7 15 1 adjust-l2cap-hdr

    if {$i > 0 } {
	[$node($i) set bb_] set T_w_inquiry_scan_ 36
	[$node($i) set bb_] set T_inquiry_scan_ 4096
	[$node($i) set bb_] set T_w_page_scan_ 4064
	[$node($i) set bb_] set T_page_scan_ 4096
    }
}

$node(0) set-rate 3	;# set 3mb high rate

#$node(0) LossMod BlueHoc			;# turn on LossMod for all nodes
 $node(0) trace-all-NULL on	
 $node(0) trace-all-POLL on	
# $node(0) trace-all-in-air on	

set udp0 [new Agent/UDP]
$ns attach-agent $node(0) $udp0
set cbr0 [new Application/Traffic/CBR]
$cbr0 attach-agent $udp0
set null0 [new Agent/Null]
$ns attach-agent $node(1) $null0
$udp0 set packetSize_ 1400
$cbr0 set packetSize_ 990
$cbr0 set interval_ 0.015
$ns connect $udp0 $null0

set udp1 [new Agent/UDP]
$ns attach-agent $node(0) $udp1
set cbr1 [new Application/Traffic/CBR]
$cbr1 attach-agent $udp1
set null1 [new Agent/Null]
$ns attach-agent $node(2) $null1
$ns connect $udp1 $null1
$udp1 set packetSize_ 1400
$cbr1 set packetSize_ 990
$cbr1 set interval_ 0.015

set udp2 [new Agent/UDP]
$ns attach-agent $node(0) $udp2
set cbr2 [new Application/Traffic/CBR]
$cbr2 attach-agent $udp2
set null2 [new Agent/Null]
$ns attach-agent $node(3) $null2
$ns connect $udp2 $null2
$udp2 set packetSize_ 1400
$cbr2 set packetSize_ 990
$cbr2 set interval_ 0.015


# make connections.
$ns at 0.01 "$node(0) make-bnep-connection $node(1) 3-DH5 3-DH5 noqos "
$ns at 0.1 "$node(0) make-bnep-connection $node(2) 3-DH3 3-DH1 noqos "
$ns at 0.2 "$node(0) make-bnep-connection $node(3)"
$ns at 0.3 "$node(0) make-bnep-connection $node(4)"
$ns at 0.4 "$node(0) make-bnep-connection $node(5)"
$ns at 0.5 "$node(0) make-bnep-connection $node(6)"
$ns at 0.6 "$node(0) make-bnep-connection $node(7)"

$ns at 6 "$cbr0 start"
$ns at 6 "$cbr1 start"
$ns at 6 "$cbr2 start"

$ns at 7 "$node(0) reset-energy-allnodes"

proc finish {} {
    global node
    $node(0) print-all-stat
    exit 0
}
                                                                                
$ns at 15.01 "finish"

$ns run


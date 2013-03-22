
set ns [new Simulator]
# set f [open out.tr w]
# $ns trace-all $f

$ns node-config -macType Mac/BNEP

set node(0) [$ns node 0]
set node(1) [$ns node 1]

$node(0) rt AODV
$node(1) rt AODV

# $node(0) LossMod BlueHoc

$node(0) set-statist 3 12 1
$node(1) set-statist 3 12 1
$node(0) trace-all-stat2 u.data a
$node(1) trace-me-stat-pernode u1.dat a

#$node(1) pos 14 2

$ns at 0.0002 "$node(0) on"
$ns at 0.0005 "$node(1) on"


set udp0 [new Agent/UDP]
$ns attach-agent $node(0) $udp0
set cbr0 [new Application/Traffic/CBR]
$cbr0 attach-agent $udp0

set null0 [new Agent/Null]
$ns attach-agent $node(1) $null0

$ns connect $udp0 $null0

# $ns at 1.0 "$cbr0 start"

set nscmd "$cbr0 start"

$ns at 0.001 "$node(0) make-bnep-connection $node(1) DH1 DH1 noqos none $nscmd"

# $cbr0 set packetSize_ 23

#puts [$cbr0 set packetSize_]
#puts [$cbr0 set interval_]

$ns at 12.1 "finish"

proc finish {} {
	global node
        $node(0) print-all-stat
	exit 0
}

$ns run


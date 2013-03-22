
set ns [new Simulator]

global defaultRNG
$defaultRNG seed raw 10735

$ns node-config -macType Mac/BNEP
set val(nn)     9

for {set i 0} {$i < $val(nn) } {incr i} {
    set node($i) [$ns node $i ]
    $node($i) rt Manual
    $node($i) SchedAlgo PRR
    $node($i) BrAlgo DRP
    set lmp [$node($i) set lmp_]
    $lmp set defaultTSniff_ 256
    $lmp set defaultSniffAttempt_ 128
    $ns at 0.0 "$node($i) on"       ;# An random delay up to 2 slots applied.
    $node($i) set-statist 50 90 1 adjust-l2cap-hdr
    $node($i) CollisionDist 22
}
                                                                                
$node(0) LossMod Off
$node(0) trace-all-stat2 9-90-DRP-22-256-Off a
#$node(0) trace-all-stat3 d-9-90-DRP-22-256-Off a

$node(1) trace-me-stat-pernode 1f1.dat a
$node(2) trace-me-stat-pernode 1f2.dat a
$node(3) trace-me-stat-pernode 1f3.dat a
$node(4) trace-me-stat-pernode 1f4.dat a

$node(0) trace-all-tx off
$node(0) trace-all-rx off
$node(0) trace-all-POLL off
$node(0) trace-all-NULL off
$node(0) trace-all-in-air off
$node(0) trace-all-link off
$node(0) trace-all-stat off

$node(0) manu-rt-path  $node(1) $node(2) $node(3) $node(4) $node(5) $node(6) $node(7) $node(8)

set udp0 [new Agent/UDP]
$ns attach-agent $node(0) $udp0
set cbr0 [new Application/Traffic/CBR]
$cbr0 attach-agent $udp0

set null0 [new Agent/Null]
$ns attach-agent $node(8) $null0
$ns connect $udp0 $null0

$udp0 set packetSize_ 1400
$cbr0 set packetSize_ 1349
#$cbr0 set interval_ 0.035
$cbr0 set interval_ 0.015
#$cbr0 set interval_ 0.15

set nscmd "$cbr0 start"

$ns at 0.01 "$node(0) make-bnep-connection $node(1) DH5 DH1"
$ns at 0.01 "$node(2) make-bnep-connection $node(3) DH5 DH1"
$ns at 0.01 "$node(4) make-bnep-connection $node(5) DH5 DH1"
$ns at 0.01 "$node(6) make-bnep-connection $node(7) DH5 DH1"
$ns at 3 "$node(8) make-br $node(7) DH1 DH5"
$ns at 3 "$node(6) make-br $node(5) DH1 DH5"
$ns at 3 "$node(4) make-br $node(3) DH1 DH5"
$ns at 12 "$node(2) make-br $node(1) DH1 DH5 "

$ns at 30 "$nscmd"

proc finish {} {
    global node
    $node(0) print-all-stat
    exit 0
}

$ns at 90.1 "finish"

$ns run


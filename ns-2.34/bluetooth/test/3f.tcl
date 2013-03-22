
set ns [new Simulator]

global defaultRNG
$defaultRNG seed raw 30675

$ns node-config -macType Mac/BNEP
set val(nn)     23

for {set i 0} {$i < $val(nn) } {incr i} {
    set node($i) [$ns node $i ]
    $node($i) rt Manual
    $node($i) SchedAlgo PRR
    $node($i) BrAlgo DRP
    set lmp [$node($i) set lmp_]
    $lmp set defaultTSniff_ 256
    $lmp set defaultSniffAttempt_ 128
    $ns at 0.0 "$node($i) on"       ;# An random delay up to 2 slots applied.
    $node($i) set-statist 20 120 1 adjust-l2cap-hdr
    $node($i) CollisionDist 0
}
                                                                                
$node(0) LossMod Off
$node(10) trace-me-stat-pernode L10-3f-120-DRP-0-256-Off a
$node(16) trace-me-stat-pernode L16-3f-120-DRP-0-256-Off a
$node(22) trace-me-stat-pernode L22-3f-120-DRP-0-256-Off a
$node(3) trace-me-stat-pernode L3-3f-120-DRP-0-256-Off a
$node(13) trace-me-stat-pernode L13-3f-120-DRP-0-256-Off a
$node(9) trace-me-stat-pernode L9-3f-120-DRP-0-256-Off a
$node(8) trace-me-stat-pernode L8-3f-120-DRP-0-256-Off a
$node(2) trace-me-stat-pernode L2-3f-120-DRP-0-256-Off a
$node(19) trace-me-stat-pernode L19-3f-120-DRP-0-256-Off a
$node(0) trace-all-stat2 3f-120-DRP-0-256-Off a

$node(0) trace-all-tx off
$node(0) trace-all-rx off
$node(0) trace-all-POLL off
$node(0) trace-all-NULL off
$node(0) trace-all-in-air off
$node(0) trace-all-link off
$node(0) trace-all-stat off


set startt1 40
set startt2 60
set startt3 60

set finisht1 80
set finisht2 120
set finisht3 120


set udp0 [new Agent/UDP]
$ns attach-agent $node(0) $udp0
set cbr0 [new Application/Traffic/CBR]
$cbr0 attach-agent $udp0
set null0 [new Agent/Null]
$ns attach-agent $node(10) $null0
$ns connect $udp0 $null0
$udp0 set packetSize_ 1400
$cbr0 set packetSize_ 1329
$cbr0 set interval_ 0.015

set udp1 [new Agent/UDP]
$ns attach-agent $node(12) $udp1
set cbr1 [new Application/Traffic/CBR]
$cbr1 attach-agent $udp1
set null1 [new Agent/Null]
$ns attach-agent $node(16) $null1
$ns connect $udp1 $null1
$udp1 set packetSize_ 1400
$cbr1 set packetSize_ 1329
$cbr1 set interval_ 0.015
                                                                                
set udp2 [new Agent/UDP]
$ns attach-agent $node(18) $udp2
set cbr2 [new Application/Traffic/CBR]
$cbr2 attach-agent $udp2
set null2 [new Agent/Null]
$ns attach-agent $node(22) $null2
$ns connect $udp2 $null2
                                                                                
$udp2 set packetSize_ 1400
$cbr2 set packetSize_ 1329
$cbr2 set interval_ 0.015
                                                                                
set nscmd "startapp"

proc startapp {} {
    global ns cbr0 cbr1 cbr2 
    $cbr0 start;
    $cbr1 start;
    $cbr2 start;
}

$node(0) manu-rt-path $node(1) $node(2) $node(3) $node(4) $node(5) $node(6) $node(7) $node(8) $node(9) $node(10)

$node(12) manu-rt-path $node(11) $node(2) $node(13) $node(14) $node(15) $node(16)
$node(18) manu-rt-path $node(17) $node(8) $node(19) $node(20) $node(21) $node(22)

$node(0) setall_scanWhenOn 0

$ns at 0.2 "$node(0) make-pico-fast DH5 DH1 1"
$ns at 0.2 "$node(2) make-pico-fast DH5 DH1 3 13"
$ns at 0.2 "$node(4) make-pico-fast DH5 DH1 5"
$ns at 0.2 "$node(6) make-pico-fast DH5 DH1 7"
$ns at 0.2 "$node(8) make-pico-fast DH5 DH1 9 19"

$ns at 0.2 "$node(12) make-pico-fast DH5 DH1 11"
$ns at 0.2 "$node(14) make-pico-fast DH5 DH1 15"

$ns at 0.2 "$node(18) make-pico-fast DH5 DH1 17"
$ns at 0.2 "$node(20) make-pico-fast DH5 DH1 21"


$ns at 1.2 "$node(2) make-pico-fast DH1 DH5 1 11"
$ns at 1.2 "$node(4) make-pico-fast DH1 DH5 3"
$ns at 1.2 "$node(6) make-pico-fast DH1 DH5 5"
$ns at 1.2 "$node(8) make-pico-fast DH1 DH5 7 17"
$ns at 1.2 "$node(10) make-pico-fast DH1 DH5 9"

$ns at 1.2 "$node(14) make-pico-fast DH1 DH5 13"
$ns at 1.2 "$node(16) make-pico-fast DH1 DH5 15"

$ns at 1.2 "$node(20) make-pico-fast DH1 DH5 19"
$ns at 1.2 "$node(22) make-pico-fast DH1 DH5 21"


$ns at $startt1 "$cbr0 start"
$ns at $startt2 "$cbr1 start"
$ns at $startt3 "$cbr2 start"

$ns at $finisht1 "$cbr0 stop"
$ns at $finisht2 "$cbr1 stop"
$ns at $finisht3 "$cbr2 stop"

proc finish {} {
    global node
    $node(0) print-all-stat
    exit 0
}

$ns at 120.1 "finish"

$ns run


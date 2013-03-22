set ns [new Simulator]

$ns node-config -macType Mac/BNEP
set btRouting AODV

#defaults
set val(nn)     32

set P 0.5	;# 0.3333 ~ 0.6667
set K 7
set delta 5
set lastRoundTries 6

set trafficStartTime 90
set collisionRng 22.4	;# set to 0 to turn collision off.
set rpAlgo	DRP
set tsniff	256
set seed 1634
set tracePageScan off

if {$argc >= 1} {
    set val(nn) [lindex $argv 0]
}

if {$argc >= 2} {
    set P [lindex $argv 1]
}
if {$argc >= 3} {
    set K [lindex $argv 2]
}
if {$argc >= 4} {
    set delta [lindex $argv 3]
}
if {$argc >= 5} {
    set lastRoundTries [lindex $argv 4]
}

if {$argc >= 6} {
    set trafficStartTime [lindex $argv 5]
}
if {$argc >= 7} {
    set collisionRng [lindex $argv 6]
}
if {$argc >= 8} {
    set rpAlgo [lindex $argv 7]
}
if {$argc >= 9} {
    set tsniff [lindex $argv 8]
}
if {$argc >= 10} {
    set seed [lindex $argv 9]
}
if {$argc >= 11} {
    set tracePageScan [lindex $argv 10]
}

global defaultRNG
$defaultRNG seed raw $seed

set recStartT [expr $trafficStartTime + 10]
set recFinishT [expr $recStartT + 10]
set dst [expr $val(nn) - 1]
# set radioRng 11.2
set sniffatt [expr $tsniff / 2]

for {set i 0} {$i < $val(nn) } {incr i} {
    set node($i) [$ns node $i ]
    $node($i) rt $btRouting
    $node($i) set-statist $recStartT $recFinishT 1
    $ns at 0 "$node($i) on"
    [$node($i) set lmp_] set defaultTSniff_ $tsniff
    [$node($i) set lmp_] set defaultSniffAttempt_ $sniffatt
    $node($i) SchedAlgo PRR
    $node($i) BrAlgo $rpAlgo
    $node($i) ScatForm Law
    $node($i) sf-law-delta $delta
    $node($i) sf-law-p $P
    $node($i) sf-law-k $K
    $node($i) sf-law-term-schred $lastRoundTries
    [$node($i) set bb_] set N_page_ 1
    [$node($i) set bb_] set ver_ 11
    #[$node($i) set bb_] set ver_ 12
}

$node(0) CollisionDist $collisionRng
$node(0) trace-all-in-air $tracePageScan


set udp0 [new Agent/UDP]
$ns attach-agent $node(0) $udp0
set cbr0 [new Application/Traffic/CBR]
$cbr0 attach-agent $udp0
                                                                                
set null0 [new Agent/Null]
$ns attach-agent $node($dst) $null0
                                                                                
$ns connect $udp0 $null0
                                                                                
$udp0 set packetSize_ 1400
$cbr0 set packetSize_ 1329
$cbr0 set interval_ 0.015
                                                                                
$ns at $recStartT "$cbr0 start"

$ns at $recFinishT "finish"
                                                                                
proc finish {} {
    global node
    $node(0) print-all-stat
    exit 0
}

$ns run

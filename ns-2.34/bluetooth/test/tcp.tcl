
set ns [new Simulator]
set f [open tcp.tr w]
$ns trace-all $f
set nf [open tcp.nam w]
#$ns namtrace-all $nf
$ns namtrace-all-wireless $nf 7 7

Simulator set MacTrace_ ON
Simulator set RouterTrace_ ON

$ns node-config -macType Mac/BNEP

set node(0) [$ns node 0]
set node(1) [$ns node 1]

$node(0) set-statist 10 30 1
$node(1) set-statist 10 30 1

$node(0) rt AODV
$node(1) rt AODV

$node(0) LossMod BlueHoc
 $node(0) trace-all-NULL on
 $node(0) trace-all-POLL on
# $node(0) trace-all-in-air on

# [$node(1) set bb_] set T_w_inquiry_scan_ 4096
# [$node(1) set bb_] set T_inquiry_scan_ 4096

$ns at 0.0002 "$node(0) on"
$ns at 0.0005 "$node(1) on"

set tcp0 [new Agent/TCP]
$ns attach-agent $node(0) $tcp0
set ftp0 [new Application/FTP]
$ftp0 attach-agent $tcp0

set null0 [new Agent/TCPSink]
$ns attach-agent $node(1) $null0

$ns connect $tcp0 $null0

# $ns at 1.0 "$ftp0 start"
set nscmd "$ftp0 start"

[$node(1) set l2cap_] set ifq_limit_ 30
[$node(0) set l2cap_] set ifq_limit_ 40

set ifq_ [new Queue/DropTail]
$ifq_ set limit_ 20

$ns at 0.001 "$node(0) make-bnep-connection $node(1) DH5 DH3 noqos $ifq_ $nscmd"

$ns at 30.1 "finish"

proc finish {} {
	global node
        $node(0) print-all-stat
	exit 0
}

$ns run


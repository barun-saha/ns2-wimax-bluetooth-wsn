
set ns [new Simulator]

$ns node-config -macType Mac/BNEP

set node(0) [$ns node 0]
set node(1) [$ns node 1]

$node(0) rt AODV
$node(1) rt AODV

# $node(0) LossMod BlueHoc
 $node(0) trace-all-in-air on

[$node(0) set bb_] set T_w_inquiry_scan_ 36
[$node(0) set bb_] set T_inquiry_scan_ 4096
[$node(0) set bb_] set T_w_page_scan_ 4032

$node(0) pos 0 0
$node(1) pos 4 3

$ns at 0.0002 "$node(0) on"
$ns at 0.0005 "$node(1) on"


set tcp0 [new Agent/TCP]
$ns attach-agent $node(0) $tcp0
set ftp0 [new Application/FTP]
$ftp0 attach-agent $tcp0

set null0 [new Agent/TCPSink]
$ns attach-agent $node(1) $null0

$ns connect $tcp0 $null0

set nscmd "$ftp0 start"

$ns at 0.1 "$node(1) make-bnep-connection $node(0) DH1 DH1"
$ns at 1.7 "$node(0) switch-role $node(1) master"
$ns at 2 "$nscmd"

$ns at 5.0 "finish"

proc finish {} {
	exit 0
}

$ns run


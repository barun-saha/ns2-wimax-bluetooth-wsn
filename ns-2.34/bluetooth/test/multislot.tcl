
set ns [new Simulator]

$ns node-config -macType Mac/BNEP

set node(0) [$ns node 0]
set node(1) [$ns node 1]

$node(0) rt AODV
$node(1) rt AODV

$node(0) LossMod BlueHoc

# [$node(1) set bb_] set T_w_inquiry_scan_ 4096
# [$node(1) set bb_] set T_inquiry_scan_ 4096

[$node(0) set l2cap_] set defaultPktType_ 15 ;# DH5
[$node(0) set l2cap_] set defaultRecvPktType_ 15 ;# DH5

$node(0) pos 0 0
$node(1) pos 4 8

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

$ns at 0.001 "$node(0) make-bnep-connection $node(1) DH5 DH5 noqos none $nscmd"

$ns at 5.0 "finish"

proc finish {} {
	exit 0
}

$ns run


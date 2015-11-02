set ns [new Simulator]

$ns color 1 Blue
$ns color 2 Red

# create trace file
set tracefile [open test01.tr w]
$ns trace-all $tracefile

# define finish procedure
proc finish {} {
     global ns tracefile
     $ns flush-trace
     close $tracefile
     exit 0
}

# create three nodes
set n0 [$ns node]
set n1 [$ns node]
set n2 [$ns node]
set n3 [$ns node]
set n4 [$ns node]

# create links between the nodes
$ns duplex-link $n0 $n4 2Mb 20ms DropTail
$ns duplex-link $n1 $n4 2Mb 10ms DropTail
$ns duplex-link $n4 $n3 2Mb 20ms DropTail
$ns duplex-link $n4 $n2 2Mb 10ms DropTail


# set TCP connection
set tcp [new Agent/TCP]
$ns attach-agent $n0 $tcp
set sink [new Agent/TCPSink]
$ns attach-agent $n3 $sink
$ns connect $tcp $sink
$tcp set fid_ 1

# set FTP connection
set ftp [new Application/FTP]
$ftp attach-agent $tcp

# setup a UDP connection
set udp [new Agent/UDP]
$ns attach-agent $n1 $udp
set null [new Agent/Null]
$ns attach-agent $n2 $null
$ns connect $udp $null
$udp set fid_ 2

# setup CBR over UDP
set cbr [new Application/Traffic/CBR]
$cbr attach-agent $udp
$cbr set packetSize_ 1000
$cbr set rate_ 0.01Mb
$cbr set random_ false

# schedule the events
$ns at 0.5 "$cbr start"
$ns at 1.0 "$ftp start"
$ns at 9.0 "$ftp stop"
$ns at 9.5 "$cbr stop"

$ns at 10.0 "finish"

$ns run

set val(mac)            Mac/BNEP                 ;# MAC type
set val(nn)             8                          ;# number of mobilenodes

set StartTime [list 0.0 0.0006 0.1031 0.1134 0.3878 0.8531 0.6406 0.0627]

set ns_		[new Simulator]

$ns_ node-config -macType $val(mac) 	;# set node type to BTNode

for {set i 0} {$i < $val(nn) } {incr i} {
	set node($i) [$ns_ node $i ]
        $node($i) rt AODV

	# set max backoff length for Inquirey response. 
	# The default is 2046 (1023 slots)
	[$node($i) set bb_ ] set backoffParam_ 2046
	[$node($i) set bb_ ] set ver_ 11
	$ns_ at [lindex $StartTime $i] "$node($i) on"
}

# at time 0.1, ask $node(0) to inquiry for 4x1.28s unless it gets 5 response
$ns_ at  0.1 "$node(0) inquiry 4 5"

# at time 1.1, ask $node(2) to inquiry for 2x1.28s unless it gets 4 response
$ns_ at  1.1 "$node(2) inquiry 2 4"

$ns_ at  2.1 "$node(1) inquiry 4 7"

$ns_ at 10.01 "$ns_ halt"

$ns_ run


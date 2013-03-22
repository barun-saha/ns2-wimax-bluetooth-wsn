set ns [new Simulator]
$ns node-config -macType Mac/BNEP
set n0 [$ns node 0]

# $n0 version
 $n0 print-lmp-cmd-len

puts "FREQUENCY HOPPING SAMPLE DATA"
puts ""
puts ""
# puts "FIRST SET"
# puts ""
$n0 test-fh 0x0
# puts ""
# puts ""
# puts "SECOND SET"
# puts ""
$n0 test-fh 0x2a96ef25
# puts ""
# puts ""
# puts "THIRD SET"
# puts ""
$n0 test-fh 0x6587cba9
# puts ""

# $n0 test-fh 0x9e8b33
# $n0 test-fh 0x1
# $n0 test-fh 0x2
# $n0 test-fh 0x3
# $n0 test-fh 0x4
# $n0 test-fh 0x5
# $n0 test-fh 0x6
# $n0 test-fh 0x7


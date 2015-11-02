#!/bin/bash

#
# Sample TCL scripts to test whether or not NS-2 has been installed properly
# and is working. This also tests the Bluetooth, Mannasim, and WiMax modules
# that has been incorporated into this version of NS-2.34.
#
# Barun Saha (http://barunsaha.me)
# 13 March 2015, IIT Kharagpur
#

rm -f test*.tr

for i in $(seq 1 6)
do
	file_name=test0"$i".tcl
	echo "Testing script $i/6"
	ns "$file_name" >/dev/null 2>&1
	exit_status=$(echo $?)

	if [[ "$exit_status" -ne 0 || ! -f "test0$i.tr" ]]
	then
		echo "Test failed with script test0$i.tcl!"
		exit 1
	fi
done

echo "All tests successfully completed!"

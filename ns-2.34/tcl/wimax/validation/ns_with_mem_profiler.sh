#!/bin/bash 

######## NS OPTIONS 
NS="/raid/rouil/ns-allinone-2.31/ns-2.34/ns"
TCL="test-be.tcl"
TCL_ARGS="20 dl"
OUT_FILE="log"


######## MEM OPTIONS
INTERVAL=1

######## SYSTEM OPTIONS
# Valid options: Fedora, RHEL
SYSTEM="RHEL"

######## ENV OPTIONS; NO NEED TO CHANGE
USER=`id -n -u`
MEMORY=0
TIME=0

######## RUN NS
TIME_INIT=`date +%s`
$NS $TCL $TCL_ARGS > $OUT_FILE &
PID=$!

######## MONITOR 
echo -e "Monitoring .\c "
while [ -f /proc/$PID/status ]
do
	if [ $SYSTEM == "Fedora" ]
	then
		MEMORY2=`cat /proc/$PID/status | grep Peak | cut -d":" -f2- 2> /dev/null`
	elif [ $SYSTEM == "RHEL" ]
	then
		MEMORY2=`cat /proc/$PID/status | grep VmSize | cut -d":" -f2- 2> /dev/null`
	fi

	if [ 'x$MEMORY2' != "xkB" -a 'x$MEMORY2' != "x" ]
	then
		MEMORY=$MEMORY2
	fi

	sleep $INTERVAL
	echo -e ".\c "
done

TIME_END=`date +%s`

TIME=`echo $TIME_END - $TIME_INIT | bc -l`

######## PRINT RESULT
echo
echo
echo "Memory Footprint: $MEMORY"
echo "Running Time: $TIME seconds"

exit 0

#!/bin/bash 

# This file contains a set of functions to collect common statistics
# @author Richard Rouil
# @date 03/04/08
# 
# functions:
# - collect_datarate ("trace file", "regular expression", "wireless[0/1]")
# - collect_delay_jitter ("trace file", "send regular expression", "send wireless[0/1]", "recv regular expression", "recv wireless[0/1]")
#
# Samples of regular expressions:
# - "^+.*0 1 cbr"  : collects all cbr packets sent from wired node 0 to wired node 1 regardless of flow
# - "^r.*AGT.*cbr" : collects all cbr packets received at the application level by any wireless node (i.e for aggregated stats)
# - 
#
#

function collect_datarate {

    traces=$1
    reg_exp=$2
    wireless=$3

    #echo $1 $2 $3 
    if [ "$wireless" = "1" ]; then
	time="\$3"
	ps="\$37"
    else
	time="\$2"
	ps="\$6"
    fi
    awk_str="BEGIN{start=0;stop=0}{if (start==0) {start=$time};stop=$time;size+=$ps}END{print (8*size/(stop-start))}"
    DATARATE=`grep "$reg_exp" $traces | awk "$awk_str"`
    echo $DATARATE
}

function collect_delay_jitter {

    traces=$1
    s_reg_exp=$2
    s_wireless=$3
    r_reg_exp=$4
    r_wireless=$5   


    if [ "$s_wireless" = "1" ]; then
	s_time="\$3"
	s_ps="\$37"
	s_uid="\$41"
    else
	s_time="\$2"
	s_ps="\$6"
	s_uid="\$12"
    fi

    if [ "$r_wireless" = "1" ]; then
	r_time="\$3"
	r_ps="\$37"
	r_uid="\$41"
    else
	r_time="\$2"
	r_ps="\$6"
	r_uid="\$12"
    fi

    #collect sending data first
    awk_str="{print \"+ \"$s_uid\" \"$s_time}"
    grep "$s_reg_exp" $traces | awk "$awk_str" > /tmp/tmp.dat
    awk_str="{print \"- \"$r_uid\" \"$r_time}"
    grep "$r_reg_exp" $traces | awk "$awk_str" >> /tmp/tmp.dat
        
    
    awk_str="{if (\$1==\"+\") { if (\$2 in start) {} else {start[\$2]=\$3;}} else {if (\$2 in stop) {} else {stop[\$2]=\$3; delay+=stop[\$2]-start[\$2];recv++;}}}END{if (recv==0) {print 0\" \"0} else {mean=delay/recv; for (tid in start) {if (tid in stop) {new_d = stop[tid]-start[tid]; var+=(new_d-mean)*(new_d-mean);} else {loss++;}} print mean\" \"sqrt(var/recv)}}"

    DELAY_JITTER=`cat /tmp/tmp.dat | awk "$awk_str"`
    echo $DELAY_JITTER
}


#SENT=`collect_datarate "out_be.res" "^+.*0 1 cbr" "0"`
#RECV=`collect_datarate "out_be.res" "^r.*AGT.*cbr" "1"`
#DELAY_JITTER=`collect_delay_jitter "out_be.res" "^+.*0 1 cbr" "0" "^r.*AGT.*cbr" "1"`
#echo $SENT $RECV $DELAY_JITTER

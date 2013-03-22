#!/bin/sh

if [ $# -lt 6 ]; then 
    echo "Usage: $0 gnuplot fontsize datafile outputfile keyx keyy [gv]"
    exit 1
fi

gnuplotcmd=$1
fontsize=$2
datafile=$3
outputfile=$4
keyx=$5
keyy=$6

# datafile="fig15.dat"
datafile="'$datafile'"
# outputfile="fig3.eps"
# fontsize=20
# keyx=2.9
# keyy=0.99000
ylabel="packet received rate (100kbps)"
xlabel="node"

key1="DRP w/o collision"
key2="DRP w/ collision"
key3="MDRP w/o collision"
key4="MDRP w/ collision"
key5="Upper Bound"

ploty1=0
ploty2="4.0"

# gnuplotcmd="gnuplot"
# gnuplotcmd="cat "

$gnuplotcmd << EOF

set output "$outputfile"
set terminal postscript eps enhanced $fontsize

set key $keyx,$keyy Left reverse  box
set ylabel "$ylabel" +0,+1.8
set xlabel "$xlabel"
set xtics 1,1

set data style linespoints

plot  [] [$ploty1:$ploty2] $datafile using 1:(\$2/100000) title "$key1" with linesp lt 1 lw 2 pt 6, \
    '' using 1:(\$3/100000) title "$key2" with linesp lt 2 lw 2 pt 1, \
    '' using 1:(\$4/100000) title "$key3" with linesp lt 3 lw 2 pt 8, \
    '' using 1:(\$5/100000) title "$key4" with linesp lt 6 lw 2 pt 2, \
    '' using 1:(\$6/100000) title "$key5" with linesp lt 4 lw 2 pt 0

EOF

if [ $# -gt 6 ]; then
    gv $outputfile
fi


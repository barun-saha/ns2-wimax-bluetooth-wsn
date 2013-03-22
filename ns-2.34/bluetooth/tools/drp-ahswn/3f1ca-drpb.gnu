
set output "3f1bca.eps"
set terminal postscript eps enhanced 17

#set key 3.1,0.72000 Left reverse  box
set ylabel "throughput (100kbps)" +0,+1.8
set xlabel "time (sec)"
#set xtics 1,1

set data style linespoints

plot  [50:] [0:4.4] '3f.dat' \
	 using ($1+20):(($3+$11)/100000) title "Flow 1+3 DRP" with linesp lt 1 lw 2 pt 6, \
	'' using ($1+20):(($5+$13)/100000) title "Flow 1+3 DRPB" with linesp lt 7 lw 2 pt 4  \



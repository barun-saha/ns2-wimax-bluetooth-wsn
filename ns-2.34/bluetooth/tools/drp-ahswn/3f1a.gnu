
set output "3f1a.eps"
set terminal postscript eps enhanced 17

#set key 3.1,0.72000 Left reverse  box
set key right bottom
set ylabel "throughput (100kbps)" +0,+1.8
set xlabel "time (sec)"
#set xtics 1,1

set data style linespoints

plot  [50:] [0:4.4] '3f.dat' using ($1+20):(($2+$6)/100000) title "Flow 1+2 DRP" with linesp lt 2 lw 2 pt 1, \
	'' using ($1+20):(($4+$8)/100000) title "Flow 1+2 MDRP" with linesp lt 4 lw 2 pt 2, \
	'' using ($1+20):(($2+$10)/100000) title "Flow 1+3 DRP" with linesp lt 1 lw 2 pt 6, \
	'' using ($1+20):(($4+$12)/100000) title "Flow 1+3 MDRP" with linesp lt 7 lw 2 pt 4  \




set output "3f1c.eps"
set terminal postscript eps enhanced 20

#set key 3.1,0.72000 Left reverse  box
set ylabel "throughput (100kbps)" +0,+1.8
set xlabel "time (sec)"
#set xtics 1,1

set data style linespoints

plot  [50:] [0:4.4] '3f.dat' using ($1+20):($3/100000) title "Flow 1 DRP" with linesp lt 2 lw 2 pt 8, \
	'' using ($1+20):($5/100000) title "Flow 1 MDRP" with linesp lt 3 lw 2 pt 12, \
	'' using ($1+20):($7/100000) title "Flow 2 DRP" with linesp lt 4 lw 2 pt 2, \
	'' using ($1+20):($9/100000) title "Flow 2 MDRP" with linesp lt 5 lw 2 pt 6, \
	'' using ($1+20):($11/100000) title "Flow 3 DRP" with linesp lt 1 lw 2 pt 3, \
	'' using ($1+20):($13/100000) title "Flow 3 MDRP" with linesp lt 7 lw 2 pt 1   


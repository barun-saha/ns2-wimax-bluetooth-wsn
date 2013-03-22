
# $1 udp/tcp
# $2 packet_slot_num
set_pkt_size () {
    if [ "$1" = "udp" ]; then
	transpt=u
	if [ $2 = 1 ]; then
	    pktsz=1080
	    rate=172800
	    pktszip=1053
        elif [ $2 = 3 ]; then
            pktsz=1098
            rate=585600
            pktszip=1071
        elif [ $2 = 5 ]; then
            pktsz=1356
            pktszip=1329
            rate=723200
        else
            exit 1
        fi
                                                                                
    elif [ "$1" = "tcp" ]; then
        transpt=t
        if [ $2 = 1 ]; then
            pktsz=1080
            pktszbnep=1073
            pktszip=1013
            rate=172800
        elif [ $2 = 3 ]; then
            pktsz=1098
            pktszbnep=1091
            pktszip=1031
            rate=585600
        elif [ $2 = 5 ]; then
            pktsz=1356
            pktszbnep=1349
            pktszip=1289
            rate=723200
        else
            exit 1
        fi
    fi
    if [ "x$rt" = "xManual" ]; then
        pktszip=`expr $pktszip + 20`
    fi
}

gen_bt_nodes () {
    nn=$1 # num of node
    btr=$2 # Rt
    bralgm=$3 #RP
    errmode=$4
    collDist=$5
    linkshed=$6
    tsniff=$7
    snatt=$8
    logfile=$9

    shift 9
    statstrttime=$1
    statendtime=$2
    statstep=$3
    rngx1=$4
    rngy1=$5
    rngx2=$6
    rngy2=$7
    fn=$8

    if [ $btr = "AODV" ]; then
	AODV_hdr="AODV";
    fi

##########################################################
    #generate start time
#    ind=0
#    rndnm=""
#    until [ $ind -eq $nn ]; do
#        tmp00=`echo "scale=6; $RANDOM/3276700" | bc -l`
#        rndnm="$rndnm $tmp00"
#        ind=`expr $ind + 1`
#    done
##########################################################

    src=0
    dst=$lst
                                                                                
    cat > $fn <<EOF
remove-all-packet-headers
add-packet-header IP TCP LL Mac BNEP BT $SDP_hdr $AODV_hdr

set ns [new Simulator]

global defaultRNG
\$defaultRNG seed raw $RANDOM

\$ns node-config -macType Mac/BNEP
set val(nn)     $nn

for {set i 0} {\$i < \$val(nn) } {incr i} {
    set node(\$i) [\$ns node \$i ]
    \$node(\$i) rt $btr
    \$node(\$i) CollisionDist $collDist
    [\$node(\$i) set bb_] set N_page_ 1
    [\$node(\$i) set bb_] set T_w_page_scan_ 4060
    \$node(\$i) SchedAlgo $linkshed
EOF

    if [ "$bralgm" != "noBR" ]; then
        echo "    \$node(\$i) BrAlgo $bralgm" >> $fn
        echo "    set lmp [\$node(\$i) set lmp_]" >> $fn
	echo "    \$lmp set defaultTSniff_ $tsniff" >> $fn
        echo "    \$lmp set defaultSniffAttempt_ $snatt" >> $fn
    fi

    if [ "$bralgm" = "LPDRP" ]; then
	echo "    \$lmp set scanWhenOn_ 0" >> $fn
	echo "    \$lmp set lowDutyCycle_ 1" >> $fn
    fi
    if [ "x$noOn" != "xyes" ]; then
        echo "    \$ns at 0.0 \"\$node(\$i) on\"       ;# An random delay up to 2 slots applied." >> $fn
    fi

    #\$node(\$i) range $rngx1 $rngy1 $rngx2 $rngy2
    if [ "x$noBtStat" != "xyes" ]; then
        echo "    \$node(\$i) set-statist $statstrttime $statendtime $statstep 4 ;# add 4 bytes l2cap hdr" >> $fn
    fi

    cat >> $fn <<EOF
}
                                                                                
\$node(0) LossMod $errmode
EOF

}


less_trace () {
    cat >> $1 <<EOF

\$node(0) trace-all-tx off
\$node(0) trace-all-rx off
\$node(0) trace-all-POLL off
\$node(0) trace-all-NULL off
\$node(0) trace-all-in-air off
\$node(0) trace-all-link off
\$node(0) trace-all-stat off
\$node(0) trace-all-bnep off

EOF
}

manual_path () {
    src=$1
    dst=$2
    bralgm=$3
    fn=$4

    ind=`expr $src + 1`
    path0=""
    until [ $ind -gt $dst ]; do
        # path0="$path0 \$node($ind)"
        path0="$path0 $ind"
        ind=`expr $ind + 1`
    done
    if [ "$bralgm" != "noBR" ]; then
        echo "\$node($src) manu-rt-path $path0" >> $fn
    else
        echo "\$node($src) manu-rt-path \$node(1) \$node($dst)" >> $fn
    fi
}

gen_cbr () {
    label=$1
    src=$2
    dst=$3
    pktszip=$4
    intvl=$5
    udppktsize=$6
    fn=$7

    cat >> $fn <<EOF

set udp$label [new Agent/UDP]
\$ns attach-agent \$node($src) \$udp$label
set cbr$label [new Application/Traffic/CBR]
\$cbr$label attach-agent \$udp$label

set null$label [new Agent/Null]
\$ns attach-agent \$node($dst) \$null$label
\$ns connect \$udp$label \$null$label

\$udp$label set packetSize_ $udppktsize
\$cbr$label set packetSize_ $pktszip
\$cbr$label set interval_ $intvl

EOF

}

gen_cbr_func () {
    fn=$1

    cat >> $fn <<EOF
proc gen-cbr {ind src dst pktszip intvl udppktsize startt {finisht 9999}} {
    global ns node 

    set udp(\$ind) [new Agent/UDP]
    \$ns attach-agent \$node(\$src) \$udp(\$ind)
    set cbr(\$ind) [new Application/Traffic/CBR]
    \$cbr(\$ind) attach-agent \$udp(\$ind)

    set null(\$ind) [new Agent/Null]
    \$ns attach-agent \$node(\$dst) \$null(\$ind)
    \$ns connect  \$udp(\$ind) \$null(\$ind)

    \$udp(\$ind) set packetSize_ \$udppktsize
    \$cbr(\$ind) set packetSize_ \$pktszip
    \$cbr(\$ind) set interval_ \$intvl

    \$ns at \$startt "\$cbr(\$ind) start"
    \$ns at \$finisht "\$cbr(\$ind) stop"

    return \$cbr(\$ind)
}

EOF
}

gen_ftp () {
    label=$1
    src=$2
    dst=$3
    pktszip=$4
    fn=$5

cat >> $fn <<EOF

set tcp$label [new Agent/TCP]
\$ns attach-agent \$node($src) \$tcp$label
set ftp$label [new Application/FTP]
\$ftp$label attach-agent \$tcp$label
\$tcp$label set packetSize_ $pktszip

set null$label [new Agent/TCPSink]
\$ns attach-agent \$node($dst) \$null$label
\$ns connect \$tcp$label \$null$label

EOF

}

gen_telnet () {
    label=$1
    src=$2
    dst=$3
    pktszip=$4
    fn=$5

cat >> $fn <<EOF

set tcp$label [new Agent/TCP]
\$ns attach-agent \$node($src) \$tcp$label
set telnet$label [new Application/Telnet]
\$telnet$label attach-agent \$tcp$label
\$tcp$label set packetSize_ $pktszip

set null$label [new Agent/TCPSink]
\$ns attach-agent \$node($dst) \$null$label
\$ns connect \$tcp$label \$null$label

EOF

}

make_piconet () {
    nn=$1
    master=$2
    numconn=$3
    pktslt=$4
    rpktslt=$5
    src=$6
    fn=$7

    ind=2
    lst=`expr $nn - 1`
    until [ $ind -gt $lst ]; do
        echo "\$ns at 0.1 \"\$node($master) make-bnep-connection \$node($ind) DH$pktslt DH$rpktslt\"" >> $fn
        ind=`expr $ind + 1`
    done
    echo "\$ns at 0.1 \"\$node($master) make-bnep-connection \$node($src) DH$rpktslt DH$pktslt \"" >> $fn

    echo "\$ns at 8 \"\$nscmd\"" >> $fn
}

# pkt rpkt fn starttime master s1 s2 s3 s4 s5 s6 s7
make_pico_bypass () {
    pktslt=$1 
    rpktslt=$2 
    fn=$3
    t=$4
    ma=$5
    shift 5
    while [ $# -gt 0 ]; do
	echo "\$ns at $t \"\$node($ma) join \$node($1) DH$pktslt DH$rpktslt\"" >> $fn
	shift
    done
    echo >> $fn
}

make_linear_mm () {
    nn=$1
    pktslt=$2
    rpktslt=$3
    fn=$4

    ind=0
    indp1=1
    lst=`expr $nn - 1`
    n2lst=`expr $lst - 1`

    echo >> $fn
    until [ $ind -gt $n2lst ]; do
        echo "\$ns at 0.01 \"\$node($ind) make-bnep-connection \$node($indp1) DH$pktslt DH$rpktslt\"" >> $fn
        ind=`expr $ind + 2`
        indp1=`expr $ind + 1`
    done

    ind=$lst
    indm1=`expr $ind - 1`
    n2lst=3

    until [ $ind -lt $n2lst ]; do
        echo "\$ns at 3 \"\$node($ind) make-br \$node($indm1) DH$rpktslt DH$pktslt\"" >> $fn
        ind=`expr $ind - 2`
        indm1=`expr $ind - 1`
    done

    echo "\$ns at 12 \"\$node(2) make-br \$node(1) DH$rpktslt DH$pktslt \"" >> $fn
    echo >> $fn
    echo "\$ns at 30 \"\$nscmd\"" >> $fn

}

make_linear_ss () {
    nn=$1
    pktslt=$2
    rpktslt=$3
    fn=$4

    ind=1
    indp1=2
    lst=`expr $nn - 1`
    n2lst=`expr $lst - 1`

    echo >> $fn
    until [ $ind -gt $n2lst ]; do
        echo "\$ns at 0.01 \"\$node($ind) make-bnep-connection \$node($indp1) DH$pktslt DH$rpktslt\"" >> $fn
        ind=`expr $ind + 2`
        indp1=`expr $ind + 1`
    done

    ind=$n2lst
    indm1=`expr $ind - 1`
    n2lst=1

    until [ $ind -lt $n2lst ]; do
        echo "\$ns at 3 \"\$node($ind) make-br \$node($indm1) DH$rpktslt DH$pktslt\"" >> $fn
        ind=`expr $ind - 2`
        indm1=`expr $ind - 1`
    done

    # echo "\$ns at 12 \"\$node(2) make-br \$node(1) DH$rpktslt DH$pktslt \"" >> $fn
    echo >> $fn
    echo "\$ns at 30 \"\$nscmd\"" >> $fn

}

make_reset_energy () {
    nn=$1
    t=$2
    fn=$3

    ind=0
    lst=`expr $nn - 1`
    echo >> $fn
    until [ $ind -gt $lst ]; do
	echo "\$ns at $t \"\$node($ind) reset-energy\"" >> $fn
	ind=`expr $ind + 1`
    done
    echo >> $fn
}

make_finish () {
    finishtime=$1
    fn=$2

    cat >> $fn <<EOF

proc finish {} {
    global node
    \$node(0) print-all-stat
    exit 0
}

\$ns at $finishtime "finish"

\$ns run

EOF

}

make_nonlinear_mm () {
    numnode=$1
    pktslt=$2
    rpktslt=$3
    fn=$4

    lst=`expr $numnode - 1`
    n2lst=`expr $lst - 1`
    ind=2
    indp1=3

    until [ $ind -gt $n2lst ]; do
        echo "\$ns at 3 \"\$node($ind) make-bnep-connection \$node($indp1) DH$pktslt DH$rpktslt\"" >> $fn
        ind=`expr $ind + 2`
        indp1=`expr $ind + 1`
    done

    ind=$lst
    indm1=`expr $ind - 1`
    n2lst=3

    until [ $ind -lt $n2lst ]; do
        echo "\$ns at 8 \"\$node($ind) make-br \$node($indm1) DH$rpktslt DH$pktslt\"" >> $fn
        ind=`expr $ind - 2`
        indm1=`expr $ind - 1`
    done

    if [ $numnode -gt 4 ]; then
        ind=$numnode
        ind1=`expr $ind  + 1`
        ind2=`expr $ind  + 2`     ind3=`expr $ind  + 3`
        echo "#" >> $fn
        echo "\$ns at 0.01 \"\$node(2) make-bnep-connection \$node($ind) DH$pktslt DH$rpktslt\"" >> $fn
        echo "\$ns at 2 \"\$node(2) make-bnep-connection \$node($ind2) DH$pktslt DH$rpktslt\"" >> $fn
        echo "\$ns at 10 \"\$node($ind1) make-br \$node($ind) DH$rpktslt DH$pktslt\"" >> $fn
        echo "\$ns at 10 \"\$node($ind3) make-br \$node($ind2) DH$rpktslt DH$pktslt\"" >> $fn
    fi

    if [ $numnode -gt 8 ]; then
        nd=6
        ind=`expr $numnode + 4`
        ind1=`expr $ind  + 1`
        ind2=`expr $ind  + 2`
        ind3=`expr $ind  + 3`
        echo "#" >> $fn
        echo "\$ns at 0.01 \"\$node($nd) make-bnep-connection \$node($ind) DH$pktslt DH$rpktslt\"" >> $fn
        echo "\$ns at 2 \"\$node($nd) make-bnep-connection \$node($ind2) DH$pktslt DH$rpktslt\"" >> $fn
        echo "\$ns at 10 \"\$node($ind1) make-br \$node($ind) DH$rpktslt DH$pktslt\"" >> $fn
        echo "\$ns at 10 \"\$node($ind3) make-br \$node($ind2) DH$rpktslt DH$pktslt\"" >> $fn
        echo " " >> $fn
    fi

    echo "\$ns at 5 \"\$node(2) make-bnep-connection \$node(1) DH$rpktslt DH$pktslt \"" >> $fn
    echo "\$ns at 12 \"\$node(0) make-br \$node(1) DH$pktslt DH$rpktslt \"" >> $fn
    echo "\$ns at 15 \"\$nscmd\"" >> $fn
}


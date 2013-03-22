Baseband set debug_ 0
L2CAP set debug_ 0
LMP set debug_ 0
Mac/BNEP set debug_ 0
SDP set debug_ 0
BTChannel set debug_ 0

Baseband set T_w_inquiry_scan_ 0
Baseband set T_inquiry_scan_ 0
Baseband set T_w_page_scan_ 0
Baseband set T_page_scan_ 0
Baseband set inquiryTO_ 0
Baseband set inq_max_num_responses_ 0
Baseband set pageTO_ 0
Baseband set SR_mode_ 0
Baseband set N_page_ 0
Baseband set N_inquiry_ 0
Baseband set backoffParam_ 0
Baseband set backoffParam_s_ 0
Baseband set Page_Scan_Type_ 0
Baseband set Inquiry_Scan_Type_ 0
Baseband set ver_ 0
Baseband set driftType_ 0
Baseband set clkdrift_ 0
Baseband set collisionDist_ 0

Baseband set useDynamicTpoll_ 0
Baseband set pollReserveClass_ 0

#Baseband set pmEnabled_ 0
#Baseband set energy_ 0
#Baseband set activeEnrgConRate_ 0
#Baseband set energyMin_ 0
#Baseband set activeTime_ 0
#Baseband set startTime_ 0
#Baseband set numTurnOn_ 0
#Baseband set warmUpTime_ 0

#Baseband set ext_inqconn_ 0

LMP set T_w_inquiry_scan_ 0
LMP set T_inquiry_scan_ 0
LMP set T_w_page_scan_ 0
LMP set T_page_scan_ 0
LMP set giac_ 0
LMP set inq_max_period_length_ 0
LMP set inq_min_period_length_ 0
LMP set inquiry_length_ 0
LMP set inq_num_responses_ 0
LMP set nb_timeout_ 0
LMP set nb_dist_ 0
LMP set takeover_ 0
LMP set scan_after_inq_ 0
LMP set NPage_manual_ 0
LMP set NInqury_manual_ 0

LMP set defaultTSniff_ 0
LMP set defaultSniffAttempt_ 0
LMP set defaultSniffTimeout_ 0

LMP set supervisionTO_ 0
LMP set supervisionEnabled_ 0
LMP set idleSchred_ 0
LMP set defaultHoldTime_ 0
LMP set minHoldTime_ 0
LMP set maxHoldTime_ 0
LMP set autoOnHold_ 0
LMP set idleCheckEnabled_ 0
LMP set idleCheckIntv_ 0
LMP set nullTriggerSchred_ 0
LMP set failTriggerSchred_ 0

LMP set defaultPktType_ 0
LMP set defaultRecvPktType_ 0
LMP set allowRS_ 0

LMP set pageStartTO_ 0
LMP set inqStartTO_ 0
LMP set scanStartTO_ 0

LMP set scanWhenOn_ 0
LMP set lowDutyCycle_ 0

L2CAP set ifq_limit_ 0

Agent/SCO set packetType_ 0
Agent/SCO set initDelay_ 0

Mac/BNEP set onDemand_ 0
Node/BTNode set enable_clkdrfit_in_rp_ 0
Node/BTNode set randomizeSlotOffset_ 0
Node/BTNode set initDelay_ 0
Node/BTNode set X_ 0
Node/BTNode set Y_ 0
Node/BTNode set Z_ 0

Node/BTNode instproc init args {
        $self instvar mac_ bnep_ sdp_ l2cap_ lmp_ bb_ phy_ ll_ \
		arptable_ classifier_ dmux_ entry_ ragent_ 
        eval $self next $args

	set bnep_ [new Mac/BNEP]
	set mac_ $bnep_
	set sdp_ [new SDP]
	set l2cap_ [new L2CAP]
	set lmp_ [new LMP]
	set bb_ [new Baseband]
	set phy_ [new BTChannel]
	set ll_ [new LL]

	set arptable_ [new ARPTable $self $bnep_]	;# not used

	$ll_ mac $bnep_
	# $ll_ arptable $arptable_	
	$ll_ up-target [$self entry]

	$ll_ down-target $bnep_
	$bnep_ up-target $ll_
	$bnep_ down-target $l2cap_
	$l2cap_ up-target $bnep_
	$l2cap_ down-target $lmp_
	$lmp_ up-target $l2cap_
#	$lmp_ down-target $bb_
	$bb_ up-target $lmp_
	$bb_ down-target $phy_
	$phy_ up-target $bb_

	set ns [Simulator instance]

	set namtracefd [$ns get-nam-traceall]
	if {$namtracefd != "" } {
	    $self namattach $namtracefd
	    puts $namtracefd "n -t * -s [AddrParams addr2id $args] -x [$self set X_] -y [$self set Y_] -Z 0 -z 0.6  -v circle -c black"
	}

	set tracefd [$ns get-ns-traceall]
	if {[Simulator set MacTrace_] == "ON" && $tracefd != "" } {
		# puts "tracefile : $tracefd"
		set sndT [bt-trace Send MAC $self]
		$bb_ down-target $sndT
		$sndT target $phy_
		$mac_ drop-target [bt-trace Drop MAC $self]
		set rcvT [bt-trace Recv MAC $self]
		$phy_ up-target $rcvT
		$rcvT target $bb_

		if {$namtracefd != "" } {
		    $sndT namattach $namtracefd
		    $rcvT namattach $namtracefd
		}
	}

	$classifier_ defaulttarget $ll_

	$self setup [AddrParams addr2id $args] $phy_ $bb_ $lmp_ $l2cap_ $bnep_ $sdp_
}

Node/BTNode instproc rt {rtagent} {
        $self instvar mac_ bnep_ sdp_ l2cap_ lmp_ bb_ ll_ arptable_ classifier_ dmux_ entry_ ragent_ 

	set addr [$self node-addr]

	switch -exact $rtagent {
	    DSDV {
		# puts "DSDV support is not complete!"
		# exit

		set rag [new Agent/DSDV/BT]
		$rag addr $addr
	        if [Simulator set mobile_ip_] {
                	$ragent port-dmux [$self demux]
        	}
	    }
	    AODV {
	 	set rag [new Agent/AODV/BT $addr]
		# $rag if-queue $ifq_
	    }
	    DumbAgent {
		puts "DumbAgent is no longer supported! Use Manual instead."
		exit
	    }
	    DSR {
		puts "DSR support is not complete!"
		exit
	    }
	    TORA {
		puts "TORA support is not complete!"
		exit
	    }
	    Manual {
	 	# set rag [new Agent/ManualBT $addr]
	 	set rag [new Agent/ManualBT ]
	    }
	    default {
		puts "Wrong routing agent!"
		exit
	    }
	}

	$rag node $self		;# connect in c++ space
	$self set ragent_ $rag
	# $self ragent $rag
	$self attach $rag [Node set rtagent_port_]
	# $ragent port-dmux [$self demux]

	set port [Node set rtagent_port_]
	$rag target $ll_
	$dmux_ install $port $rag
	$classifier_ defaulttarget $rag

##	set ns [Simulator instance]
##	set tracefd [$ns get-ns-traceall]
##	if {$tracefd != "" } {
##	    $self nodetrace $tracefd
##	    $self agenttrace $tracefd
##	}
##	set namtracefd [$ns get-nam-traceall]
##	if {$namtracefd != "" } {
##	    $self namattach $namtracefd
##	}
}


Node/BTNode instproc on { {rndSlotOffset yes} {clkdrift no} {drifval 0} } {
    $self instvar bb_ ragent_ randomizeSlotOffset_

    # Turn on immediately without the initial random delay up to 2 slots.
    # Use it if you want to control slots alignment yourself.
    if { $rndSlotOffset == "imm" || $clkdrift == "imm" } {
	set randomizeSlotOffset_ 0	
    }

    if { $rndSlotOffset == "drift" || $clkdrift == "drift" } {
	$bb_ set driftType_ 1	;# random
    }

    if { $rndSlotOffset == "drift-normal" || $clkdrift == "drift-normal" } {
	$bb_ set driftType_ 2	;# normal
    }

    if { $clkdrift == "drift-user" } {
	$bb_ set driftType_ 3	;# user defined.
	$bb_ set clkdrift_ $drifval
    }
    if { $rndSlotOffset == "drift-user" } {
	$bb_ set driftType_ 3   ;# user defined.
	$bb_ set clkdrift_ $clkdrift
    }

    # puts $rndSlotOffset 
    # puts $clkdrift
    # puts $drifval
    if { [$bb_ set clkdrift_] > 20 || [$bb_ set clkdrift_] < -20 } {
	puts "Clock drift should be in [-20, 20] ppm."
	exit
    }

    $self turn-it-on
    #$ragent_ start
}

Agent/ManualBT instproc start {} {
}

Agent/DSDV/BT instproc start {} {
    $self start-dsdv
}

Agent/DSDV/BT instproc if-queue {ifq} {
    # $self ll-queue $ifq
}

Node/BTNode instproc make-bnep-connection {dest {pktType DH1} {recvPktType DH1} {qos none} {ifq none} args} {
    if { $ifq == "none" } {
	$self bnep-connect $dest $pktType $recvPktType
    } else {
	$self bnep-connect $dest $pktType $recvPktType $ifq
    }
    
    if { [llength $qos] == 6 } {
	$self bnep-qos-setup $dest [set $qos]
    }

    if { [llength $args] > 0 } {
	#puts "$args";
	$self cmd-after-bnep-connect $dest "$args";
    }
}

Node/BTNode instproc make-br {br {pktType DH1} {recvPktType DH1} {qos none} {ifq none} args } {
    set debug_ 1
    $self instvar address_ bb_
    $br instvar address_
    set maddr [$self set address_]
    set baddr [$br set address_]
    #puts "$maddr try to connect br $baddr."
    #puts "$args"

    # $br pagescan 4096 4000
    $br pagescan 4096 4096
    [$self set bb_] set N_page_ 1
    if { [llength $args] > 0 } {
	# set cmd [join $args " "]
        # $self make-bnep-connection $br $pktType $recvPktType $qos "$cmd"
        $self make-bnep-connection $br $pktType $recvPktType $qos $ifq "$args"
    } else {
        $self make-bnep-connection $br $pktType $recvPktType $qos $ifq
    }
}

SDP instproc openSearch { } {
}

SDP instproc closeSearch { } {
}

# three senarioes are introduced in Profile 1.1 (p76)
# 1. collect_user_input -> inq -> foreach RemDev: (page) sdp_inq -> results
# 2. inq -> collect_user_input -> foreach RemDev: (page) sdp_inq -> results
# 3. inq -> collect_user_input -> conn all -> foreach RemDev: sdp_inq - results

#RemDevRelation : trusted/unknown/connected ??
SDP instproc serviceBrowse { remDev remDevRel browseGroup {getRemDevName no} } {
}

SDP instproc serviceSearch { remDev remDevRel srchPttn-attrLst {getRemDevName no} } {
}

SDP instproc enumerateRemDev { {classOfDev any} } {
}

SDP instproc terminatePrimitive { primitiveHandle } {
}

#####################################
source ../../bluetooth/ns-compund.tcl


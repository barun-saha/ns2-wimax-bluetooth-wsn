# ======================================================================
# Default Script Options
# ======================================================================
Agent/NOAH set sport_        0
Agent/NOAH set dport_        0
Agent/NOAH set use_mac_      0        ;# Performance suffers with this on
Agent/NOAH set be_random_    1        ;# Flavor the performance numbers :)
Agent/NOAH set verbose_      0        ;# 

#Class Agent/NOAH

set opt(ragent)		Agent/NOAH
set opt(pos)		NONE			;# Box or NONE

if { $opt(pos) == "Box" } {
	puts "*** NOAH using Box configuration..."
}

# ======================================================================
Agent instproc init args {
        eval $self next $args
}       

Agent/NOAH instproc init args {
        eval $self next $args
}       

# ===== Get rid of the warnings in bind ================================

# ======================================================================

proc create-noah-routing-agent { node id } {
    global ns_ ragent_ tracefd opt

    #
    #  Create the Routing Agent and attach it to port 255.
    #
    #set ragent_($id) [new $opt(ragent) $id]
    set ragent_($id) [new $opt(ragent)]
    set ragent $ragent_($id)

    ## setup address (supports hier-addr) for dsdv agent 
    ## and mobilenode
    set addr [$node node-addr]
    
    $ragent addr $addr
    $ragent node $node
    if [Simulator set mobile_ip_] {
	$ragent port-dmux [$node set dmux_]
    }
    $node addr $addr
    $node set ragent_ $ragent
    
    $node attach $ragent [Node set rtagent_port_]

    ##$ragent set target_ [$node set ifq_(0)]	;# ifq between LL and MAC
        
    # XXX FIX ME XXX
    # Where's the DSR stuff?
    #$ragent ll-queue [$node get-queue 0]    ;# ugly filter-queue hack
    $ns_ at 0.0 "$ragent_($id) start-dsdv"	;# start updates

    #
    # Drop Target (always on regardless of other tracing)
    #
    set drpT [cmu-trace Drop "RTR" $node]
    $ragent drop-target $drpT
    
    #
    # Log Target
    #
    set T [new Trace/Generic]
    $T target [$ns_ set nullAgent_]
    $T attach $tracefd
    $T set src_ $id
    $ragent tracetarget $T
}


proc noah-create-mobile-node { id args } {
    global ns ns_ chan prop topo tracefd opt node_
    
    puts "NOAH node created"

    set ns_ [Simulator instance]
    if {[Simulator set hier-addr?]} {
	if [Simulator set mobile_ip_] {
	    set node_($id) [new MobileNode/MIPMH $args]
	} else {
	    set node_($id) [new Node/MobileNode/BaseStationNode $args]
	}
    } else {
	set node_($id) [new Node/MobileNode]
    }
    set node $node_($id)
    $node random-motion 0		;# disable random motion
    $node topography $topo
    
    # XXX Activate energy model so that we can use sleep, etc. But put on 
    # a very large initial energy so it'll never run out of it.
    if [info exists opt(energy)] {
	$node addenergymodel [new $opt(energy) $node 1000 0.5 0.2]
    }

    #
    # This Trace Target is used to log changes in direction
    # and velocity for the mobile node.
    #
    set T [new Trace/Generic]
    $T target [$ns_ set nullAgent_]
    $T attach $tracefd
    $T set src_ $id
    $node log-target $T
    
    if ![info exist inerrProc_] {
	set inerrProc_ ""
    }
    if ![info exist outerrProc_] {
	set outerrProc_ ""
    }
    if ![info exist FECProc_] {
	set FECProc_ ""
    }

    $node add-interface $chan $prop $opt(ll) $opt(mac)	\
	    $opt(ifq) $opt(ifqlen) $opt(netif) $opt(ant) $inerrProc_ $outerrProc_ $FECProc_ 
    
    #
    # Create a Routing Agent for the Node
    #
    create-$opt(rp)-routing-agent $node $id
    
    # ============================================================
    
    return $node
}

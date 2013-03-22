#
# Copyright (c) 1996 Regents of the University of California.
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. All advertising materials mentioning features or use of this software
#    must display the following acknowledgement:
# 	This product includes software developed by the MASH Research
# 	Group at the University of California Berkeley.
# 4. Neither the name of the University nor of the Research Group may be
#    used to endorse or promote products derived from this software without
#    specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
#

####################################################################
#                                                                  #
#  The following trace handling code is adapted from cmu-trace.    #
#                                                                  #
####################################################################

proc bt-trace { ttype atype node } {
        # global ns_ tracefd
                                                                                
	set ns [Simulator instance]
	set tracefd [$ns get-ns-traceall]

        if { $tracefd == "" } {
                return ""
        }
        set T [new CMUTrace/BT/$ttype $atype]
        # $T target [$ns_ set nullAgent_]
        $T target [$ns set nullAgent_]
        $T attach $tracefd
        $T set src_ [$node id]
                                                                                
        $T node $node
                                                                                
        return $T
}

#
# Attach an agent to a node.  Pick a port and
# bind the agent to the port number.
# if portnumber is 255, default target is set to the routing agent
#
Node/BTNode instproc add-target { agent port } {
	$self instvar dmux_ imep_ toraDebug_ 

	set ns [Simulator instance]
	set newapi [$ns imep-support]

	$agent set sport_ $port

	# special processing for TORA/IMEP node
	set toraonly [string first "TORA" [$agent info class]] 
	if {$toraonly != -1 } {
	#	$agent if-queue [$self set ifq_(0)]  ;# ifq between LL and MAC
		#
		# XXX: The routing protocol and the IMEP agents needs handles
		# to each other.
		#
		$agent imep-agent [$self set imep_(0)]
		[$self set imep_(0)] rtagent $agent
	}
	
	# Special processing for AODV
	set aodvonly [string first "AODV" [$agent info class]] 
	if {$aodvonly != -1 } {
	#	$agent if-queue [$self set ifq_(0)]   ;# ifq between LL and MAC
	}
	
	if { $port == [Node set rtagent_port_] } {			
		# Ad hoc routing agent setup needs special handling
		$self add-target-rtagent $agent $port
		return
	}

# puts " AgentTrace_ [Simulator set AgentTrace_]"

	# Attaching a normal agent
	set namfp [$ns get-nam-traceall]
	if { [Simulator set AgentTrace_] == "ON" && [$ns get-ns-traceall] != ""} {
		set tf [$ns get-ns-traceall]
		if { $tf == ""} {
		    puts "No trace file specified. use stdout."
		    $ns set-ns-traceall stdout
		}
		#
		# Send Target
		#
		if {$newapi != ""} {
			set sndT [$self mobility-trace Send "AGT"]
		} else {
			set sndT [bt-trace Send AGT $self]
		}
		if { $namfp != "" } {
			$sndT namattach $namfp
		}
		$sndT target [$self entry]
		$agent target $sndT
		#
		# Recv Target
		#
		if {$newapi != ""} {
			set rcvT [$self mobility-trace Recv "AGT"]
		} else {
			set rcvT [bt-trace Recv AGT $self]
		}
		if { $namfp != "" } {
			$rcvT namattach $namfp
		}
		$rcvT target $agent
		$dmux_ install $port $rcvT
	} else {
		#
		# Send Target
		#
		$agent target [$self entry]
		#
		# Recv Target
		#
		$dmux_ install $port $agent
	}
}

Node/BTNode instproc add-target-rtagent { agent port } {
	$self instvar imep_ toraDebug_ 

	set ns [Simulator instance]
	set newapi [$ns imep-support]
	set namfp [$ns get-nam-traceall]

	set dmux_ [$self demux]
	set classifier_ [$self entry]

	# let the routing agent know about the port dmux
	$agent port-dmux $dmux_

# puts "[Simulator set RouterTrace_]"

	if { [Simulator set RouterTrace_] == "ON" } {
		#
		# Send Target
		#
		if {$newapi != ""} {
			set sndT [$self mobility-trace Send "RTR"]
		} else {
			set sndT [bt-trace Send "RTR" $self]
		}
		if { $namfp != "" } {
			$sndT namattach $namfp
		}
		if { $newapi == "ON" } {
			$agent target $imep_(0)
			$imep_(0) sendtarget $sndT
			# second tracer to see the actual
			# types of tora packets before imep packs them
			if { [info exists toraDebug_] && $toraDebug_ == "ON"} {
				set sndT2 [$self mobility-trace Send "TRP"]
				$sndT2 target $imep_(0)
				$agent target $sndT2
			}
		} else {  ;#  no IMEP
			$agent target $sndT
		}
		$sndT target [$self set ll_]
		#
		# Recv Target
		#
		if {$newapi != ""} {
			set rcvT [$self mobility-trace Recv "RTR"]
		} else {
			set rcvT [bt-trace Recv "RTR" $self]
		}
		if { $namfp != "" } {
			$rcvT namattach $namfp
		}
		if {$newapi == "ON" } {
			[$self set ll_] up-target $imep_(0)
			$classifier_ defaulttarget $agent
			# need a second tracer to see the actual
			# types of tora packets after imep unpacks them
			# no need to support any hier node
			if {[info exists toraDebug_] && $toraDebug_ == "ON" } {
				set rcvT2 [$self mobility-trace Recv "TRP"]
				$rcvT2 target $agent
				$classifier_ defaulttarget $rcvT2
			}
		} else {
			$rcvT target $agent
			$classifier_ defaulttarget $rcvT
			$dmux_ install $port $rcvT
		}
	} else {
		#
		# Send Target
		#
		# if tora is used
		if { $newapi == "ON" } {
			$agent target $imep_(0)
			# second tracer to see the actual
			# types of tora packets before imep packs them
			if { [info exists toraDebug_] && $toraDebug_ == "ON"} {
				set sndT2 [$self mobility-trace Send "TRP"]
				$sndT2 target $imep_(0)
				$agent target $sndT2
			}
			$imep_(0) sendtarget [$self set ll_]
			
		} else {  ;#  no IMEP
			$agent target [$self set ll_]
		}    
		#
		# Recv Target
		#
		if {$newapi == "ON" } {
			[$self set ll_] up-target $imep_(0)
			$classifier_ defaulttarget $agent
			# need a second tracer to see the actual
			# types of tora packets after imep unpacks them
			# no need to support any hier node
			if {[info exists toraDebug_] && $toraDebug_ == "ON" } {
				set rcvT2 [$self mobility-trace Recv "TRP"]
				$rcvT2 target $agent
				[$self set classifier_] defaulttarget $rcvT2
			}
		} else {
			$classifier_ defaulttarget $agent
			$dmux_ install $port $agent
		}
	}
}

Node/BTNode instproc mobility-trace { ttype atype } {
	set ns [Simulator instance]
        set tracefd [$ns get-ns-traceall]
        if { $tracefd == "" } {
	        puts "Warning: You have not defined you tracefile yet!"
	        puts "Please use trace-all command to define it."
		return ""
	}
	set T [new CMUTrace/BT/$ttype $atype]
	$T newtrace [Simulator set WirelessNewTrace_]
	$T tagged [Simulator set TaggedTrace_]
	$T target [$ns nullagent]
	$T attach $tracefd
        $T set src_ [$self id]
        $T node $self
	return $T
}

Node/BTNode instproc nodetrace { tracefd } {
	#
	# This Trace Target is used to log changes in direction
	# and velocity for the mobile node.
	#
	set T [new Trace/Generic]
	$T target [[Simulator instance] set nullAgent_]
	$T attach $tracefd
	$T set src_ [$self id]
	$self log-target $T    
}

Node/BTNode instproc agenttrace {tracefd} {
	set ns [Simulator instance]
	set ragent [$self set ragent_]
	#
	# Drop Target (always on regardless of other tracing)
	#
	set drpT [$self mobility-trace Drop "RTR"]
	set namfp [$ns get-nam-traceall]
	if { $namfp != ""} {
		$drpT namattach $namfp
	}
	$ragent drop-target $drpT
	#
	# Log Target
	#
	set T [new Trace/Generic]
	$T target [$ns set nullAgent_]
	$T attach $tracefd
	$T set src_ [$self id]
	$ragent tracetarget $T
	#
	# XXX: let the IMEP agent use the same log target.
	#
	set imepflag [$ns imep-support]
	if {$imepflag == "ON"} {
		[$self set imep_(0)] log-target $T
	}
}


CMUTrace/BT instproc init { tname type } {
	$self next $tname $type
	$self instvar type_ src_ dst_ callback_ show_tcphdr_

	set type_ $type
	set src_ 0
	set dst_ 0
	set callback_ 0
	set show_tcphdr_ 0
}

CMUTrace/BT instproc attach fp {
	$self instvar fp_
	set fp_ $fp
	$self cmd attach $fp_
}

Class CMUTrace/BT/Send -superclass CMUTrace/BT
CMUTrace/BT/Send instproc init { tname } {
	$self next $tname "s"
}

Class CMUTrace/BT/Recv -superclass CMUTrace/BT
CMUTrace/BT/Recv instproc init { tname } {
	$self next $tname "r"
}

Class CMUTrace/BT/Drop -superclass CMUTrace/BT
CMUTrace/BT/Drop instproc init { tname } {
	$self next $tname "D"
}

Class CMUTrace/BT/EOT -superclass CMUTrace/BT
 CMUTrace/BT/EOT instproc init { tname } {
       $self next $tname "x"
 }

# XXX MUST NOT initialize off_*_ here!! 
# They should be automatically initialized in ns-packet.tcl!!

CMUTrace/BT/Recv set src_ 0
CMUTrace/BT/Recv set dst_ 0
CMUTrace/BT/Recv set callback_ 0
CMUTrace/BT/Recv set show_tcphdr_ 0

CMUTrace/BT/Send set src_ 0
CMUTrace/BT/Send set dst_ 0
CMUTrace/BT/Send set callback_ 0
CMUTrace/BT/Send set show_tcphdr_ 0

CMUTrace/BT/Drop set src_ 0
CMUTrace/BT/Drop set dst_ 0
CMUTrace/BT/Drop set callback_ 0
CMUTrace/BT/Drop set show_tcphdr_ 0

CMUTrace/BT/EOT set src_ 0
CMUTrace/BT/EOT set dst_ 0
CMUTrace/BT/EOT set callback_ 0
CMUTrace/BT/EOT set show_tcphdr_ 0


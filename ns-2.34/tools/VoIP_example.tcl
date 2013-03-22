#definition of the procedure
proc create_voip { codec vad start stop } {
	global ns
	set application [new Application/Traffic/VoIP]
	$application vad
	$application use-vad-model $vad
	$application use-codec $codec
	$ns at $start "$application start"
	if { $stop != "never" } {
		$ns at $stop "$application stop"
	}
	return $application
}


# create VoIP application
set voice [create_voip "G.711" "one-to-one" 1.0 "never"]

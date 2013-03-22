rng.cc and rng.h are copied over from ns-2.26/tools/

setdest.cc and setdest.h are adapted from the same named files in 
	ns-2.26/indep-utils/cmu-scen-gen/setdest

added flag for setdest:

	-c <max start time> : node turn on within this time span.  If
			      negative, "$node_(n) on" won't be printed.

	-m <name>: e.g. -m nod -> $nod(n)
			-m Node -> $Node(n)


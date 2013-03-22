#!/bin/sh

tmp00=`pwd`
if . ../check-dir.sh $0 `basename $tmp00`
then
    echo "Working dir is ok."
else
    exit 1
fi

cd ../../
dir=`pwd`

LINK='ln -sf '

setlink () {
    if cd $1 
    then  
	if [ -h $2 ]
	then 
	    rm -rf $2
	else 
	    mv -f $2 $2.btsave
	fi
	$LINK $3/$2 $2
    fi
}

DST=bluetooth/ns-2.29

setlink $dir Makefile.in $DST
setlink $dir/tcl/lib ns-lib.tcl ../../$DST
setlink $dir/tcl/lib ns-packet.tcl ../../$DST
setlink $dir/common packet.h ../$DST
setlink $dir/common mobilenode.cc ../$DST
setlink $dir/common mobilenode.h ../$DST
setlink $dir/mac arp.cc ../$DST
setlink $dir/mac arp.h ../$DST
setlink $dir/mac wireless-phy.cc ../$DST
setlink $dir/mac wireless-phy.h ../$DST
setlink $dir/aodv aodv.h ../$DST
setlink $dir/aodv aodv.cc ../$DST
setlink $dir/aodv aodv_rtable.h ../$DST
setlink $dir/trace cmu-trace.h ../$DST
setlink $dir/trace cmu-trace.cc ../$DST
setlink $dir/dsdv dsdv.h ../$DST
setlink $dir/dsdv rtable.h ../$DST


#!/bin/sh

tmp00=`pwd`
if ../check-dir.sh $0 `basename $tmp00`
then 
    echo "Working dir is ok."
else
    exit 1
fi

cd $tmp00/../.. && diff -u Makefile.in.btsave Makefile.in > $tmp00/Makefile.in.patch
cd $tmp00/../../tcl/lib && diff -u ns-lib.tcl.btsave ns-lib.tcl > $tmp00/ns-lib.tcl.patch
cd $tmp00/../../tcl/lib && diff -u ns-packet.tcl.btsave ns-packet.tcl > $tmp00/ns-packet.tcl.patch
cd $tmp00/../../common && diff -u packet.h.btsave packet.h > $tmp00/packet.h.patch
cd $tmp00/../../common && diff -u mobilenode.h.btsave mobilenode.h > $tmp00/mobilenode.h.patch
cd $tmp00/../../common && diff -u mobilenode.cc.btsave mobilenode.cc > $tmp00/mobilenode.cc.patch
#cd $tmp00/../../common && diff -u scheduler.cc.btsave scheduler.cc > $tmp00/scheduler.cc.patch
cd $tmp00/../../mac && diff -u arp.h.btsave arp.h > $tmp00/arp.h.patch
cd $tmp00/../../mac && diff -u arp.cc.btsave arp.cc > $tmp00/arp.cc.patch
cd $tmp00/../../mac && diff -u wireless-phy.cc.btsave wireless-phy.cc > $tmp00/wireless-phy.cc.patch
cd $tmp00/../../mac && diff -u wireless-phy.h.btsave wireless-phy.h > $tmp00/wireless-phy.h.patch
cd $tmp00/../../aodv && diff -u aodv.cc.btsave aodv.cc > $tmp00/aodv.cc.patch
cd $tmp00/../../aodv && diff -u aodv.h.btsave aodv.h > $tmp00/aodv.h.patch
cd $tmp00/../../aodv && diff -u aodv_rtable.h.btsave aodv_rtable.h > $tmp00/aodv_rtable.h.patch
cd $tmp00/../../dsdv && diff -u dsdv.h.btsave dsdv.h > $tmp00/dsdv.h.patch
cd $tmp00/../../dsdv && diff -u rtable.h.btsave rtable.h > $tmp00/rtable.h.patch
cd $tmp00/../../trace && diff -u cmu-trace.h.btsave cmu-trace.h > $tmp00/cmu-trace.h.patch
cd $tmp00/../../trace && diff -u cmu-trace.cc.btsave cmu-trace.cc > $tmp00/cmu-trace.cc.patch


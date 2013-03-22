#!/bin/sh

tmp00=`pwd`
if ../check-dir.sh $0 `basename $tmp00`
then 
    echo "Working dir is ok."
else
    exit 1
fi

cd ../../
dir=`pwd`

mv Makefile Makefile.ttss
if [ "x$1" = "x--enable-tcldebug" ]; then

    sed '{
    s|^CCOPT	= .*$|CCOPT	= -g -DBTDEBUG -DDEBUG -Wall -fsigned-char -fno-inline -DHAVE_LIBTCLDBG -I../tcl-debug-2.0|
    s| -DNDEBUG||
    s|LIB	= |LIB	= -L../tcl-debug-2.0 -ltcldbg |
    }' < Makefile.ttss > Makefile

else

    sed '{
    s|^CCOPT	= .*$|CCOPT	= -g -DBTDEBUG -DDEBUG -Wall |
    s| -DNDEBUG||
    }' < Makefile.ttss > Makefile

fi
rm -rf Makefile.ttss

COMMOBJ="agent.o bi-connector.o connector.o Decapsulator.o encap.o Encapsulator.o fsm.o ip.o ivs.o message.o misc.o mobilenode.o net-interface.o node.o ns-process.o object.o packet.o parentnode.o pkt-counter.o ptypes2tcl.o scheduler.o sessionhelper.o session-rtp.o simulator.o tclAppInit.o timer-handler.o ttl.o"

QUEOBJ="cbq.o drop-tail.o drr.o errmodel.o fec.o fq.o priqueue.o queue.o red.o red-pd.o rtqueue.o"

MACOBJ="arp.o channel.o lanRouter.o ll.o mac-802_11.o mac-802_3.o mac.o mac-tdma.o mac-timers.o phy.o smac.o varp.o wired-phy.o wireless-phy.o"

RTOBJ="address.o addr-params.o alloc-address.o route.o rtmodule.o rtProtoDV.o rttable.o"

AODVOBJ="aodv.o aodv_rtable.o aodv_rqueue.o aodv_logs.o"

cd $dir/common && rm -f $COMMOBJ
cd $dir/queue && rm -f $QUEOBJ
cd $dir/mac && rm -f $MACOBJ
cd $dir/routing && rm -f $RTOBJ
cd $dir/aodv && rm -f $AODVOBJ

cd $dir/trace && rm -f *.o
cd $dir/classifier && rm -f *.o
cd $dir/dsdv && rm -f *.o

cd $dir/bluetooth && rm -f *.o

cd $dir && make


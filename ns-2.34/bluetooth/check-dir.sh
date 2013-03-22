#!/bin/sh

# this script should run in the directory one level below 'bluetooth',
# normally one of the patch (ns-2.26, etc) directory.

tmpf=".tmp8e#r.`date +%y%m%d%H%M%S`"
tmpdir=`pwd`

cd ..
echo "$tmpf" > $tmpf
if diff $tmpf ../bluetooth/$tmpf > /dev/null 2>&1
then
    rm -rf $tmpf
    cd $tmpdir
else
    echo
    echo "You should run "$1" in the correct directory: "
    echo "[ns-2.xx/bluetooth/ns-2.xx]"
    echo
                                                                                
    rm -rf $tmpf
    cd $tmpdir
    exit 1
fi


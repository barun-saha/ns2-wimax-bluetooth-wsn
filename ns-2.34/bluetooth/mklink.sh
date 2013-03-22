#!/bin/sh

tmpdir=`pwd`
tmpdirname=`basename $tmpdir`

if [ "$tmpdirname" = "bluetooth" ]; then
    exit 0
fi

msg1 () {
    echo
    echo "ns-xxx/bluetooth directory/file exists."
    echo "Please rename or remove it first."
    echo
    echo "If this directory contains the latest ucbt source, "
    echo "please run './install-bt' there."
    echo
}

cd ..

if [ -d bluetooth ]; then
    cn=`ls -la bluetooth | wc -l `

    if [ $cn -eq 1 ]; then
	echo
	echo bluetooth/ is a symbolic link. Will be relinked to $tmpdir
	echo
    else
	msg1
	exit 1
    fi
elif [ -f bluetooth ]; then
    msg1
    exit 1
fi

rm -f bluetooth
ln -sf $tmpdirname bluetooth
cd $tmpdir


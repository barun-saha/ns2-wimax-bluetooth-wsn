all:linkucbt
	rm -rf ../ns
	cd .. ; make
	ln -f ../ns.exe . > /dev/null 2>&1 || ln -f ../ns . > /dev/null 2>&1

linkucbt:
	sh ./mklink.sh

clean:
	rm -rf *.o *~ ../aodv/*.o ns ns.exe ../trace/*.o ../dsdv/*.o
	touch ns-btnode.tcl

clean-all:clean
	cd test; make clean; cd ..

in:indent

indent:
	indent -kr bt-node.cc l2cap.cc bt-node.h l2cap.h bnep.cc bnep.h sdp.cc sdp.h 

wc:
	wc *.cc *.h *.tcl install-bt


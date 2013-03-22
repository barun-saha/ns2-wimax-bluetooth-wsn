
all: build test_fh test_inq tcp udp test_sco multislot rs scat pico pico-3mb dsdv test_sdp 3f 1f ms5n ms5n1 mr5n 2sco_inq rphsi scat-form-law oth 

oth:
	rm -rf t
	mkdir t
	mv *.out tcp.nam tcp.tr *-Off t

build:
#	cd ../; make

test_fh:build
	../ns test_fh.tcl > test_fh.out

test_inq:build
	../ns test_inq.tcl > test_inq.out

test_sdp:build
	../ns test_sdp.tcl > test_sdp.out

tcp:build
	../ns tcp.tcl > tcp.out

udp:build
	../ns udp.tcl > udp.out

pico:build
	../ns pico.tcl > pico.out

pico-3mb:build
	../ns pico-3mb.tcl > pico-3mb.out

multislot:build
	../ns multislot.tcl > multislot.out

dsdv:build
	../ns dsdv.tcl > dsdv.out

rs:build
	../ns rs.tcl > rs.out

3f:build
	../ns 3f.tcl > 3f.out

1f:build
	../ns 1f.tcl > 1f.out

ms5n:build
	../ns ms5n.tcl > ms5n.out

ms5n1:build
	../ns ms5n1.tcl > ms5n1.out

mr5n:build
	../ns mr5n.tcl > mr5n.out

2sco_inq:build
	../ns 2sco_inq.tcl > 2sco_inq.out

rphsi:build
	../ns rphsi.tcl > rphsi.out

scat-form-law:build
	../ns scat-form-law.tcl > scat-form-law.out

scat:build

test_sco:build
	../ns test_sco.tcl > test_sco.out

test_sco.out:test_sco
	true

test_sco.tcl:test_sco
	true

clean:
	rm -rf *.out out.tr out.nam

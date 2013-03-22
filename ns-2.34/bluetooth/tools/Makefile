all:setdest 

CFLAGS = -DSTL_NAMESPACE=@STL_NAMESPACE@ -DCPP_NAMESPACE=std
CPP     = c++

setdest:rng.cc rng.h setdest.cc setdest.h
	$(CPP) $(CFLAGS) -o setdest -Dstand_alone rng.cc setdest.cc

clean:
	rm -rf setdest.o rng.o setdest .rand_state 

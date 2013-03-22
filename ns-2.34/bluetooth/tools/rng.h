/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */
/*
 * Copyright (c) 1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * "@(#) $Header: /home/cvs/CVS/bt/tools/rng.h,v 1.1.1.1 2003/07/03 21:28:58 qw Exp $ (LBL)";
 */

/***********************************************************************\ 
* 
* File: RngStream.h for multiple streams of Random Numbers 
* Language: C++ 
* Copyright: Pierre L'Ecuyer, University of Montreal 
* Notice: This code can be used freely for personnal, academic, 
* or non-commercial purposes. For commercial purposes, 
* please contact P. L'Ecuyer at: lecuyer@iro.umontreal.ca 
* Date: 14 August 2001 
* 
* Incorporated into rng.h and modified to maintain backward 
* compatibility with ns-2.1b8.  Users can use their current scripts 
* unmodified with the new RNG.  To get the same results as with the 
* previous RNG, define OLD_RNG when compiling (e.g., make -D OLD_RNG).
* - Michele Weigle, University of North Carolina (mcweigle@cs.unc.edu)
* October 10, 2001
**********************************************************************/

/* new random number generator */

#ifndef _rng_h_
#define _rng_h_


#ifdef rng_stand_alone
#define stand_alone
#define rng_test
#endif

#include <math.h>
#include <stdlib.h>			// for atoi

#ifndef stand_alone
#include "config.h"
#endif   /* stand_alone */

#ifndef MAXINT
#define	MAXINT	2147483647	// XX [for now]
#endif

#ifdef OLD_RNG
/*
 * RNGImplementation is internal---do not use it, use RNG.
 */
class RNGImplementation {
public:
	RNGImplementation(long seed = 1L) {
		seed_ = seed;
	};
	void set_seed(long seed) { seed_ = seed; }
	long seed() { return seed_; }
	long next();			// return the next one
	double next_double();	 
private:
	long seed_;
};
#endif /* OLD_RNG */

/*
 * Use class RNG in real programs.
 */
class RNG 
#ifndef stand_alone
	: public TclObject 
#endif  /* stand_alone */
{

public:
	enum RNGSources { RAW_SEED_SOURCE, PREDEF_SEED_SOURCE, HEURISTIC_SEED_SOURCE };

#ifdef OLD_RNG
	RNG() : stream_(1L) {};
	inline int seed() { return stream_.seed(); }
#else
	RNG(const char* name = "");
	RNG(long seed);
	void init();
	long seed();
	void set_seed (long seed);
	long next();
	double next_double();
#endif /* OLD_RNG */

	RNG(RNGSources source, int seed = 1) { set_seed(source, seed); };
	void set_seed(RNGSources source, int seed = 1);
	inline static RNG* defaultrng() { return (default_); }

#ifndef OLD_RNG
	/*
	 * Added for new RNG
	 */
	static void set_package_seed (const unsigned long seed[6]); 
	/*
	  Sets the initial seed s 0 of the package to the six integers in the
	  vector seed. The first 3 integers in the seed must all be less than
	  m 1 = 4294967087, and not all 0; and the last 3 integers must all be
	  less than m 2 = 4294944443, and not all 0. If this method is not
	  called, the default initial seed is (12345, 12345, 12345, 12345,
	  12345, 12345).
	*/

	void reset_start_stream (); 
	/*
	  Reinitializes the stream to its initial state: C g and B g are set
	  to I g
	*/

	void reset_start_substream (); 
	/*
	  Reinitializes the stream to the beginning of its current substream:
	  C g is set to B g.
	*/

	void reset_next_substream (); 
	/*
	  Reinitializes the stream to the beginning of its next substream: N g
	  is computed, and C g and B g are set to N g .
	*/

	void set_antithetic (bool a); 
	/*
	  If a = true, the stream will start generating antithetic variates,
	  i.e., 1 - U instead of U,until this method is called again with a =
	  false.
	*/

	void increased_precis (bool incp); 
	/*
	  After calling this method with incp = true, each call to the
	  generator (direct or indirect) for this stream will return a uniform
	  random number with more bits of resolution (53 bits if machine
	  follows IEEE 754 standard) instead of 32 bits, and will advance the
	  state of the stream by 2 steps instead of 1. More precisely, if s is
	  a stream of the class RngStream, in the non­ antithetic case, the
	  instruction ``u = s.RandU01()'' will be equivalent to ``u =
	  (s.RandU01() + s.RandU01() * fact) % 1.0'' where the constant fact
	  is equal to 2 -24 . This also applies when calling RandU01
	  indirectly (e.g., via RandInt, etc.). By default, or if this method
	  is called again with incp = false, each to call RandU01 for this
	  stream advances the state by 1 step and returns a number with 32
	  bits of resolution.
	*/

	void set_seed (const unsigned long seed[6]); 
	/*
	  Sets the initial seed I g of the stream to the vector seed. The
	  vector seed should contain valid seed values as described in
	  SetPackageSeed. The state of the stream is then reset to this
	  initial seed. The states and seeds of the other streams are not
	  modified. As a result, after calling this method, the initial seeds
	  of the streams are no longer spaced Z values apart. We discourage
	  the use of this method; proper use of the Reset* methods is
	  preferable.
	*/

	void advance_state (long e, long c); 
	/*
	  Advances the state by n steps (see below for the meaning of n),
	  without modifying the states of other streams or the values of B g
	  and I g in the current object. If e > 0, then n =2 e + c;if e < 0,
	  then n = -2 -e + c;and if e = 0,then n = c. Note:c is allowed to
	  take negative values.  We discourage the use of this method.
	*/

	void get_state (unsigned long seed[6]) const; 
	/*
	  Returns in seed[0..5] the current state C g of this stream. This is
	  convenient if we want to save the state for subsequent use.
	*/

	void write_state () const; 
	/*
	  Writes (to standard output) the current state C g of this stream. 
	*/

	void write_state_full () const; 
	/*
	  Writes (to standard output) the value of all the internal variables 
	  of this stream: name, anti, incPrec, Ig, Bg, Cg.
	*/

	double rand_u01 (); 
	/*
	  Normally, returns a (pseudo)random number from the uniform
	  distribution over the interval (0, 1), after advancing the state by
	  one step. The returned number has 32 bits of precision in the sense
	  that it is always a multiple of 1/(2 32 - 208). However, if
	  IncreasedPrecis(true) has been called for this stream, the state is
	  advanced by two steps and the returned number has 53 bits of
	  precision.
	*/

	long rand_int (long i, long j); 
	/*
	  Returns a (pseudo)random number from the discrete uniform distribution
	  over the integers {i, i +1,...,j}. Makes one call to RandU01.
	*/
#endif /* !OLD_RNG */

#ifndef stand_alone
	int command(int argc, const char*const* argv);
#endif  /* stand_alone */

	// These are primitive but maybe useful.
	inline int uniform_positive_int() {  // range [0, MAXINT]
#ifdef OLD_RNG
		return (int)(stream_.next());
#else
		return (int)(next());
#endif /* OLD_RNG */
	}
	inline double uniform_double() { // range [0.0, 1.0)
#ifdef OLD_RNG
		return stream_.next_double();
#else
		return next_double();
#endif /* OLD_RNG */
	}

	// these are for backwards compatibility
 	// don't use them in new code
	inline int random() { return uniform_positive_int(); }
	inline double uniform() {return uniform_double();}

	// these are probably what you want to use
	inline int uniform(int k) 
	{ return (uniform_positive_int() % (unsigned)k); }
	inline double uniform(double r) 
	{ return (r * uniform());}
	inline double uniform(double a, double b)
	{ return (a + uniform(b - a)); }
	inline double exponential()
	{ return (-log(uniform())); }
	inline double exponential(double r)
	{ return (r * exponential());}
	// See "Wide-area traffic: the failure of poisson modeling", Vern 
	// Paxson and Sally Floyd, IEEE/ACM Transaction on Networking, 3(3),
	// pp. 226-244, June 1995, on characteristics of counting processes 
	// with Pareto interarrivals.
	inline double pareto(double scale, double shape) { 
		// When 1 < shape < 2, its mean is scale**shape, its 
		// variance is infinite.
		return (scale * (1.0/pow(uniform(), 1.0/shape)));
	}
        inline double paretoII(double scale, double shape) { 
		return (scale * ((1.0/pow(uniform(), 1.0/shape)) - 1));
	}
	double normal(double avg, double std);
	inline double lognormal(double avg, double std) { 
		return (exp (normal(avg, std))); 
	}

protected:   // need to be public?
#ifdef OLD_RNG
	RNGImplementation stream_;
#else
	double Cg_[6], Bg_[6], Ig_[6]; 
	/*
	  Vectors to store the current seed, the beginning of the current block
	  (substream) and the beginning of the current stream.
	*/

	bool anti_, inc_prec_; 
	/*
	  Variables to indicate whether to generate antithetic or increased
	  precision random numbers.
	*/

	char name_[100]; 
	/*
	  String to store the optional name of the current RngStream object. 
	*/

	static double next_seed_[6]; 
	/*
	  Static vector to store the beginning state of the next RngStream to 
	  be created (instantiated).
	*/

	double U01 (); 
	/*
	  The backbone uniform random number generator. 
	*/

	double U01d (); 
	/*
	  The backbone uniform random number generator with increased 
	  precision. 
	*/	
#endif /* OLD_RNG */
	static RNG* default_;
}; 

/*
 * Create an instance of this class to test RNGImplementation.
 * Do .verbose() for even more.
 */
#ifdef rng_test
class RNGTest {
public:
	RNGTest();
	void verbose();
	void first_n(RNG::RNGSources source, long seed, int n);
};
#endif /* rng_test */

#endif /* _rng_h_ */

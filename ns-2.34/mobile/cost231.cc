// COST231 Channel Model calculations: Raj Iyengar, RPI
//

#include <math.h>
#include <delay.h>
#include <packet.h>
#include <packet-stamp.h>
#include <antenna.h>
#include <mobilenode.h>
#include "propagation.h"
#include "wireless-phy.h"
#include "cost231.h"

#include <iostream> //RPI

static class Cost231Class: public TclClass {
public:
        Cost231Class() : TclClass("Propagation/Cost231") {}
        TclObject* create(int, const char*const*) {
                return (new Cost231);
        }
} class_cost231;

Cost231::Cost231()
{
}

/* 
 * L = 46.3 + 33.9 log(f) - 13.82 log(h_b) - C_H + [44.9 -6.55 log(h_B)]logd + C
 * L: median path loss
 * f: frequency of transmission in MHz
 * h_b: base station antenna effective height (in metres)
 * d: link distance in km
 * C_H: mobile station antenna height correction facore as described for the 
 *      Hata model for Urban Areas
 * C: 0 dB for medium cities and suburban areas, 3 dB for metropolitan areas
 */
double Cost231::cost231_formula(double Pt, double h_b, double h_m, double d, double f) // am using the COST231 formula for now.
{

    //NOTE: if the resulting powers are printed out, then we see that for some of the links, power is actally gained at the receiver
    //compared to what is sent out by the transmitter
    //This happens for MS<->MS links and BS<->BS links, due to antenna heights being the same in both cases, rendering COST231 inapplicable.

	//log() here is natural log
    // h_m: height of the mobile antenna, used in the calculation of C_H
    double loss_in_db, Pr_dB, Pr;
    double C_H; // the mobile antenna height correction factor
    double C = 3 ; // assuming medium cities for the time being, need to make this settable from tcl

	f=2000;
	
    //C_H = 0.8 + (1.1*log(f)/2.303 - 0.7)*h_m -1.56*log(f)/2.303; // from the COST231 wiki entry
								 // 2.303 is for the logarithm base change
	C_H  = (1.1*log10(f) - 0.7)*h_m - 1.56*log10(f) -0.8;


    // Commented by Barun : 21-Sep-2011
	//cout << "Cost231 params:" << endl;
   	//cout << "f = " << f << endl;
   	//cout << "d = " << d << endl;
   	//cout << "C_H = " << C_H << endl;
   	//cout << "C = " << C << endl;
   	//cout << "h_b = " << h_b << endl;
   	//cout << "h_m = " << h_m << endl;


    if (f==0 || h_b==0 || d==0 || Pt==0) {
    	fprintf(stderr, "ERROR: Mobile nodes are too close to the BS, please double check the distance and run it again f :%g, h_b :%g, d :%g, Pt :%g\n", f, h_b, d, Pt);
	exit(1);
    }
    
    loss_in_db = 46.3 + 33.9*log10(f) - 13.82 * log10(h_b) - C_H + (44.9-6.55*(log10(h_b)))*log10(d) + C;
//	cout << endl << "losses in dB: " << loss_in_db << endl;
    
    //printf("\t\t\tCost231: transmit power in watts: %g , in dB: %g\n",Pt,10*log10(Pt));
	double Pt_dB;
	Pt_dB = 10*log10(Pt);

//	cout << endl << "Transmit Power in Watts: " << Pt << endl;
//	cout << "Transmit Power in dB: " << Pt_dB << endl;
    //printf("\t\t\tCost231: path loss in dB: %g\n",loss_in_db);
    //return (pow (10.0,(double)(0.1*loss_in_db)));
	//Pr = 10.0*log(Pt)/2.303 + loss_in_db ; // ORIGINAL
	Pr_dB = Pt_dB - loss_in_db; //RPI
	Pr = pow (10.0,0.1*Pr_dB);

//	cout << endl << "Return power in dB: " << Pr_dB << endl;
//	cout << endl << "Return power in Watts: " << Pr << endl;
	//printf("\t\t\tCost231: path loss in Watts: %g\n",Pr);
	//printf("\t\t\tReturn power: %g\n",pow (10.0,0.1*Pr));
	//return (pow (10.0,0.1*Pr)); // resulting power after losses, in watts
	return Pr;
}

double Cost231::Pr(PacketStamp *t, PacketStamp *r, WirelessPhy *ifp)
{
    double rX, rY, rZ;		// location of receiver
    double tX, tY, tZ;		// location of transmitter
    double d;				// distance
    double h_b, h_m;		// height of recv and xmit antennas
    double Pr;			// received signal power
  
    double lambda = ifp->getLambda();	// wavelength
	double frequency = SPEED_OF_LIGHT/lambda; 
	frequency/=1000000.00; // need freq in mhz
    
    r->getNode()->getLoc(&rX, &rY, &rZ);
    t->getNode()->getLoc(&tX, &tY, &tZ);

    rX += r->getAntenna()->getX();
    rY += r->getAntenna()->getY();
    tX += t->getAntenna()->getX();
    tY += t->getAntenna()->getY();

    // find the distance between the two nodes
    d = sqrt((rX - tX) * (rX - tX) 
	   + (rY - tY) * (rY - tY) 
	   + (rZ - tZ) * (rZ - tZ));
    d/=1000.00; //dist in km, assuming ns-2 distances are in metres

		if(d< 0.00000000001)
		{
			printf("Distance is zero\n");
			exit(1);
		}
   
    h_b = rZ + r->getAntenna()->getZ(); 
    h_m = tZ + t->getAntenna()->getZ();
    Pr = cost231_formula(t->getTxPr(),h_b, h_m, d, frequency);
    return Pr;
}

// this really does nothing and i never call it, just left it here for legacy sake, i think channel.cc calls it.
double Cost231::getDist(double Pr, double Pt, double Gt, double Gr, double hr, double ht, double L, double lambda)
{
       /* Get quartic root */
       return sqrt(sqrt(Pt * Gt * Gr * (hr * hr * ht * ht) / Pr));
}

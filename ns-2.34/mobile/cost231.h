/* Based on the two ray ground code already existing in this folderi
 * Used to calculate path loss based on the erceg model.
 * Check the wiki entry for the erceg model
 */

#ifndef __cost231_h__
#define __cost231_h__

#include "packet-stamp.h"
#include "wireless-phy.h"
#include "propagation.h"

class Cost231 : public Propagation {
public:
  Cost231();
  virtual double Pr(PacketStamp *tx, PacketStamp *rx, WirelessPhy *ifp);
   /*virtual double Pr(PacketStamp *tx, PacketStamp *rx, WirelessPhy *ifp, Packet *p);
*/
  virtual double getDist(double Pr, double Pt, double Gt, double Gr,
			 double hr, double ht, double L, double lambda);

protected:
  double cost231_formula(double Pt, double ht, double hr, double L, double d); // the formula that calculates the bulk path loss
};


#endif 

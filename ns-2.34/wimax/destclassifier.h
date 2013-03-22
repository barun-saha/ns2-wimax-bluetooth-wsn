/* This software was developed at the National Institute of Standards and
 * Technology by employees of the Federal Government in the course of
 * their official duties. Pursuant to title 17 Section 105 of the United
 * States Code this software is not subject to copyright protection and
 * is in the public domain.
 * NIST assumes no responsibility whatsoever for its use by other parties,
 * and makes no guarantees, expressed or implied, about its quality,
 * reliability, or any other characteristic.
 * <BR>
 * We would appreciate acknowledgement if the software is used.
 * <BR>
 * NIST ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION AND
 * DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING
 * FROM THE USE OF THIS SOFTWARE.
 * </PRE></P>
 * @author  rouil
 */

#ifndef DESTCLASSIFIER_H
#define DESTCLASSIFIER_H

#include "sduclassifier.h"

/**
 * This class classifies the packet based on the destination address
 */
class DestClassifier : public SDUClassifier
{
 public:

  /**
   * Create a classifier in the given mac
   */
  DestClassifier ();

  /**
   * Create a classifier in the given mac
   * @param mac The mac where it is located
   */
  DestClassifier (Mac802_16 *mac);

  /**
   * Create a classifier in the given mac
   * @param mac The mac where it is located
   * @param priority The classifier's priority
   */
  DestClassifier (Mac802_16 *mac, int priority_);

  /**
   * Classify a packet and return the CID to use (or -1 if unknown)
   * @param p The packet to classify
   * @return The CID or -1
   */
  int classify (Packet * p);

 protected:

};

#endif

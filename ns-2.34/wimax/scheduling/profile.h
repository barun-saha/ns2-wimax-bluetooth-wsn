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

#ifndef PROFILE_H
#define PROFILE_H

#include "ofdmphy.h"

class SubFrame;

class Profile;
LIST_HEAD (profile, Profile);

/**
 * This class contains information about burst such as modulation, frequency...
 */
class Profile
{
 public:
  /**
   * Creates a profile with the given frequency and encoding
   * @param f The frequency information for the profile
   * @param enc The encoding type
   */
  Profile (SubFrame *subframe, int f, Ofdm_mod_rate enc);

  /**
   * Set the IUC number for this profile
   * @param iuc The IUC number for this profile
   */
  void setIUC( int iuc );

  /**
   * Return the frequency in unit of kHz
   * @return the frequency
   */
  int getIUC();

  /**
   * Return the encoding type
   * @return the encoding type
   */
  Ofdm_mod_rate getEncoding( );

  /**
   * Return the frequency in unit of kHz
   * @return the frequency
   */
  int getFrequency( );

  /**
   * Set the encoding type
   * @param enc the encoding type
   */
  void setEncoding( Ofdm_mod_rate enc );

  /**
   * Set the frequency in unit of kHz
   * @param f the frequency
   */
  void setFrequency( int f );

  // Chain element to the list
  inline void insert_entry(struct profile *head) {
    LIST_INSERT_HEAD(head, this, link);
  }
  
  // Return next element in the chained list
  Profile* next_entry(void) const { return link.le_next; }

  // Remove the entry from the list
  inline void remove_entry() { 
    LIST_REMOVE(this, link); 
  }

 private:
  /**
   * The type of modulation used by the burst
   */
  Ofdm_mod_rate encoding_;
  
  /**
   * The downlink frequency in kHz
   */
  int frequency_;
  
  /**
   * The Interval Usage Code for the profile
   */
  int iuc_;

  /**
   * The subframe containing this profile
   * Used to inform configuration change
   */
  SubFrame *subframe_;
  
  /*
   * Pointer to next in the list
   */
  LIST_ENTRY(Profile) link;
  //LIST_ENTRY(Profile); //for magic draw
  	
};

#endif

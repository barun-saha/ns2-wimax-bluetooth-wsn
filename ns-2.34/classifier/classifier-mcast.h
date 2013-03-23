/* -*-	Mode:C++; c-basic-offset:8; tab-width:8; indent-tabs-mode:t -*- */

/*
 * Copyright (C) 1999 by the University of Southern California
 * $Id: classifier-mcast.h,v 1.1.1.1 2008/04/11 18:40:31 rouil Exp $
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * The copyright of this module includes the following
 * linking-with-specific-other-licenses addition:
 *
 * In addition, as a special exception, the copyright holders of
 * this module give you permission to combine (via static or
 * dynamic linking) this module with free software programs or
 * libraries that are released under the GNU LGPL and with code
 * included in the standard release of ns-2 under the Apache 2.0
 * license or under otherwise-compatible licenses with advertising
 * requirements (or modified versions of such code, with unchanged
 * license).  You may copy and distribute such a system following the
 * terms of the GNU GPL for this module and the licenses of the
 * other code concerned, provided that you include the source code of
 * that other code when and as the GNU GPL requires distribution of
 * source code.
 *
 * Note that people who make modified versions of this module
 * are not obligated to grant this special exception for their
 * modified versions; it is their choice whether to do so.  The GNU
 * General Public License gives permission to release a modified
 * version without this exception; this exception also makes it
 * possible to release a modified version which carries forward this
 * exception.
 *
 */

#ifndef ns_classifier_mcast_h
#define ns_classifier_mcast_h

#include <stdlib.h>
#include "config.h"
#include "packet.h"
#include "ip.h"
#include "classifier.h"

class MCastClassifier : public Classifier {
public:
	MCastClassifier();
	~MCastClassifier();
	static const char STARSYM[]; //"source" field for shared trees
protected:
	virtual int command(int argc, const char*const* argv);
	virtual int classify(Packet *p);
	int findslot();
	enum {HASHSIZE = 256};
	struct hashnode {
		int slot;
		nsaddr_t src;
		nsaddr_t dst;
		hashnode* next;
		int iif; // for RPF checking
	};
	int hash(nsaddr_t src, nsaddr_t dst) const {
		u_int32_t s = src ^ dst;
		s ^= s >> 16;
		s ^= s >> 8;
		return (s & 0xff);
	}
	hashnode* ht_[HASHSIZE];
	hashnode* ht_star_[HASHSIZE]; // for search by group only (not <s,g>)

	void set_hash(hashnode* ht[], nsaddr_t src, nsaddr_t dst,
		      int slot, int iface);
	void clearAll();
	void clearHash(hashnode* h[], int size);
	hashnode* lookup(nsaddr_t src, nsaddr_t dst,
			 int iface = iface_literal::ANY_IFACE) const;
	hashnode* lookup_star(nsaddr_t dst,
			      int iface = iface_literal::ANY_IFACE) const;
	void change_iface(nsaddr_t src, nsaddr_t dst,
			  int oldiface, int newiface);
	void change_iface(nsaddr_t dst,
			  int oldiface, int newiface);
};
#endif
// $Id$

#ifndef __FDCDUMMYBACKEND__HH__
#define __FDCDUMMYBACKEND__HH__

#include "FDCBackEnd.hh"


class FDCDummyBackEnd : public FDCBackEnd
{
	public:
		virtual void read (byte phystrack, byte track, byte sector,
		                   byte side, int size, byte* buf);
		virtual void write(byte phystrack, byte track, byte sector,
		                   byte side, int size, const byte* buf);
		virtual void getSectorHeader(byte phystrack, byte track,
		                             byte sector, byte side, byte* buf);
		virtual void getTrackHeader(byte phystrack, byte track,
		                            byte side, byte* buf);

		virtual bool ready();
		virtual bool writeProtected();
		virtual bool doubleSided();
};

#endif

// $Id$

#ifndef __FDCDUMMYBACKEND__HH__
#define __FDCDUMMYBACKEND__HH__

#include "Disk.hh"

namespace openmsx {

class DummyDisk : public Disk
{
public:
	virtual void read (byte track, byte sector,
			   byte side, int size, byte* buf);
	virtual void write(byte track, byte sector,
			   byte side, int size, const byte* buf);
	virtual void getSectorHeader(byte track, byte sector,
				     byte side, byte* buf);
	virtual void getTrackHeader(byte track,
				    byte side, byte* buf);

	virtual bool ready();
	virtual bool writeProtected();
	virtual bool doubleSided();
};

} // namespace openmsx

#endif

// $Id$

#ifndef RAMDSKDISKIMAGE_HH
#define RAMDSKDISKIMAGE_HH

#include "SectorBasedDisk.hh"

namespace openmsx {

class RamDSKDiskImage : public SectorBasedDisk
{
public:
	explicit RamDSKDiskImage(unsigned size = 720 * 1024);
	virtual ~RamDSKDiskImage();

	virtual bool writeProtected();

private:
	// SectorBasedDisk
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);

	byte* diskdata;
};

} // namespace openmsx

#endif

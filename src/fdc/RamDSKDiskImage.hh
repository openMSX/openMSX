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

private:
	// SectorBasedDisk
	virtual void readSectorSBD(unsigned sector, byte* buf);
	virtual void writeSectorSBD(unsigned sector, const byte* buf);
	virtual bool isWriteProtectedImpl() const;

	byte* diskdata;
};

} // namespace openmsx

#endif

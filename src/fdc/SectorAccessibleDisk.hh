// $Id$

#ifndef SECTORACCESSIBLEDISK_HH
#define SECTORACCESSIBLEDISK_HH

#include "openmsx.hh"

namespace openmsx {

class SectorAccessibleDisk
{
public:
	static const unsigned SECTOR_SIZE = 512;

	virtual ~SectorAccessibleDisk();
	virtual void readSector(unsigned sector, byte* buf) = 0;
	virtual void writeSector(unsigned sector, const byte* buf) = 0;
	virtual unsigned getNbSectors() const = 0;
};

} // namespace openmsx

#endif

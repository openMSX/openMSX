// $Id$

#ifndef SECTORACCESSIBLEDISK_HH
#define SECTORACCESSIBLEDISK_HH

#include "Disk.hh"

namespace openmsx {

class SectorAccessibleDisk
{
public:
	virtual void readLogicalSector(unsigned sector, byte* buf) = 0;
	virtual void writeLogicalSector(unsigned sector, const byte* buf) = 0;
	virtual unsigned getNbSectors() const = 0 ;

protected:
	SectorAccessibleDisk();
	virtual ~SectorAccessibleDisk();
};

} // namespace openmsx

#endif

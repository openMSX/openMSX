// $Id$

#ifndef SECTORACCESSIBLEDISK_HH
#define SECTORACCESSIBLEDISK_HH

#include "WriteProtectableDisk.hh"
#include "openmsx.hh"

namespace openmsx {

class SectorAccessibleDisk : public virtual WriteProtectableDisk
{
public:
	static const unsigned SECTOR_SIZE = 512;

	virtual ~SectorAccessibleDisk();

	void readSector(unsigned sector, byte* buf);
	void writeSector(unsigned sector, const byte* buf);
	unsigned getNbSectors() const;

private:
	virtual void readSectorImpl(unsigned sector, byte* buf) = 0;
	virtual void writeSectorImpl(unsigned sector, const byte* buf) = 0;
	virtual unsigned getNbSectorsImpl() const = 0;
};

} // namespace openmsx

#endif

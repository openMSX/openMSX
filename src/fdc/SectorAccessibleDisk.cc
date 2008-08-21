// $Id$

#include "SectorAccessibleDisk.hh"
#include "DiskExceptions.hh"

namespace openmsx {

SectorAccessibleDisk::~SectorAccessibleDisk()
{
}

void SectorAccessibleDisk::readSector(unsigned sector, byte* buf)
{
	readSectorImpl(sector, buf);
}

void SectorAccessibleDisk::writeSector(unsigned sector, const byte* buf)
{
	if (isWriteProtected()) {
		throw WriteProtectedException("");
	}
	writeSectorImpl(sector, buf);
}

unsigned SectorAccessibleDisk::getNbSectors() const
{
	return getNbSectorsImpl();
}

} // namespace openmsx

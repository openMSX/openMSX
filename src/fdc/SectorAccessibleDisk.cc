// $Id$

#include "SectorAccessibleDisk.hh"
#include "DiskExceptions.hh"
#include "sha1.hh"

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
	sha1cache.clear();
	writeSectorImpl(sector, buf);
}

unsigned SectorAccessibleDisk::getNbSectors() const
{
	return getNbSectorsImpl();
}

const std::string& SectorAccessibleDisk::getSHA1Sum()
{
	if (sha1cache.empty()) {
		SHA1 sha1;
		unsigned nbSectors = getNbSectors();
		for (unsigned i = 0; i < nbSectors; ++i) {
			byte buf[SECTOR_SIZE];
			readSector(i, buf);
			sha1.update(buf, SECTOR_SIZE);
		}
		sha1cache = sha1.hex_digest();
	}
	return sha1cache;
}

} // namespace openmsx

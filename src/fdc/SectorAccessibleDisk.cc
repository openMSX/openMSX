// $Id$

#include "SectorAccessibleDisk.hh"
#include "EmptyDiskPatch.hh"
#include "IPSPatch.hh"
#include "DiskExceptions.hh"
#include "sha1.hh"

namespace openmsx {

SectorAccessibleDisk::SectorAccessibleDisk()
	: patch(new EmptyDiskPatch(*this))
	, forcedWriteProtect(false)
{
}

SectorAccessibleDisk::~SectorAccessibleDisk()
{
}

void SectorAccessibleDisk::readSector(unsigned sector, byte* buf)
{
	unsigned nbSectors = getNbSectors();
	if ((nbSectors != 0) && // in this case we want DriveEmptyException
	    (nbSectors <= sector)) {
		throw NoSuchSectorException("No such sector");
	}
	try {
		// in the end this calls readSectorImpl()
		patch->copyBlock(sector * SECTOR_SIZE, buf, SECTOR_SIZE);
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error: " + e.getMessage());
	}
}

void SectorAccessibleDisk::writeSector(unsigned sector, const byte* buf)
{
	if (isWriteProtected()) {
		throw WriteProtectedException("");
	}
	unsigned nbSectors = getNbSectors();
	if ((nbSectors != 0) && (nbSectors <= sector)) {
		throw NoSuchSectorException("No such sector");
	}
	try {
		writeSectorImpl(sector, buf);
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error: " + e.getMessage());
	}
	sha1cache.clear();
}

unsigned SectorAccessibleDisk::getNbSectors() const
{
	return getNbSectorsImpl();
}

void SectorAccessibleDisk::applyPatch(const Filename& patchFile)
{
	patch.reset(new IPSPatch(patchFile, patch));
}

void SectorAccessibleDisk::getPatches(std::vector<Filename>& result) const
{
	patch->getFilenames(result);
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

int SectorAccessibleDisk::readSectors (
	byte* buffer, unsigned startSector, unsigned nbSectors)
{
	try {
		for (unsigned i = 0; i < nbSectors; ++i) {
			readSector(startSector + i, &buffer[i * SECTOR_SIZE]);
		}
		return 0;
	} catch (MSXException& e) {
		return -1;
	}
}

int SectorAccessibleDisk::writeSectors(
	const byte* buffer, unsigned startSector, unsigned nbSectors)
{
	try {
		for (unsigned i = 0; i < nbSectors; ++i) {
			writeSector(startSector + i, &buffer[i * SECTOR_SIZE]);
		}
		return 0;
	} catch (MSXException& e) {
		return -1;
	}
}

bool SectorAccessibleDisk::isWriteProtected() const
{
	return forcedWriteProtect || isWriteProtectedImpl();
}

void SectorAccessibleDisk::forceWriteProtect()
{
	// can't be undone
	forcedWriteProtect = true;
}

} // namespace openmsx

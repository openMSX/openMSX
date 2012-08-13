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
	, peekMode(false)
{
}

SectorAccessibleDisk::~SectorAccessibleDisk()
{
}

void SectorAccessibleDisk::readSector(unsigned sector, byte* buf)
{
	if (!isDummyDisk() && // in that case we want DriveEmptyException
	    (sector > 1) && // allow reading sector 0 and 1 without calling
	                    // getNbSectors() because this potentially calls
	                    // detectGeometry() and that would cause an
	                    // infinite loop
	    (getNbSectors() <= sector)) {
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
	if (!isDummyDisk() && (getNbSectors() <= sector)) {
		throw NoSuchSectorException("No such sector");
	}
	try {
		writeSectorImpl(sector, buf);
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error: " + e.getMessage());
	}
	flushCaches();
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

bool SectorAccessibleDisk::hasPatches() const
{
	return !patch->isEmptyPatch();
}

Sha1Sum SectorAccessibleDisk::getSha1Sum()
{
	if (sha1cache.empty()) {
		try {
			setPeekMode(true);
			SHA1 sha1;
			unsigned nbSectors = getNbSectors();
			for (unsigned i = 0; i < nbSectors; ++i) {
				byte buf[SECTOR_SIZE];
				readSector(i, buf);
				sha1.update(buf, SECTOR_SIZE);
			}
			sha1cache = sha1.digest();
			setPeekMode(false);
		} catch (MSXException&) {
			setPeekMode(false);
			throw;
		}
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
	} catch (MSXException&) {
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
	} catch (MSXException&) {
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

bool SectorAccessibleDisk::isDummyDisk() const
{
	return false;
}

void SectorAccessibleDisk::flushCaches()
{
	sha1cache.clear();
}

} // namespace openmsx

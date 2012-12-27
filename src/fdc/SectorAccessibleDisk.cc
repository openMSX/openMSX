// $Id$

#include "SectorAccessibleDisk.hh"
#include "EmptyDiskPatch.hh"
#include "IPSPatch.hh"
#include "DiskExceptions.hh"
#include "sha1.hh"
#include "xrange.hh"
#include "memory.hh"

namespace openmsx {

#ifndef _MSC_VER
// This line is required according to the c++ standard, but because of a vc++
// extension, we get a link error in vc++ when we add this line. See also:
//   http://blogs.msdn.com/b/xiangfan/archive/2010/03/03/vc-s-evil-extension-implicit-definition-of-static-constant-member.aspx
const size_t SectorAccessibleDisk::SECTOR_SIZE;
#endif

SectorAccessibleDisk::SectorAccessibleDisk()
	: patch(make_unique<EmptyDiskPatch>(*this))
	, forcedWriteProtect(false)
	, peekMode(false)
{
}

SectorAccessibleDisk::~SectorAccessibleDisk()
{
}

void SectorAccessibleDisk::readSector(size_t sector, byte* buf)
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

void SectorAccessibleDisk::writeSector(size_t sector, const byte* buf)
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

size_t SectorAccessibleDisk::getNbSectors() const
{
	return getNbSectorsImpl();
}

void SectorAccessibleDisk::applyPatch(const Filename& patchFile)
{
	patch = make_unique<IPSPatch>(patchFile, std::move(patch));
}

std::vector<Filename> SectorAccessibleDisk::getPatches() const
{
	return patch->getFilenames();
}

bool SectorAccessibleDisk::hasPatches() const
{
	return !patch->isEmptyPatch();
}

Sha1Sum SectorAccessibleDisk::getSha1Sum()
{
	checkCaches();
	if (sha1cache.empty()) {
		try {
			setPeekMode(true);
			SHA1 sha1;
			for (auto i : xrange(getNbSectors())) {
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
	byte* buffer, size_t startSector, size_t nbSectors)
{
	try {
		for (auto i : xrange(nbSectors)) {
			readSector(startSector + i, &buffer[i * SECTOR_SIZE]);
		}
		return 0;
	} catch (MSXException&) {
		return -1;
	}
}

int SectorAccessibleDisk::writeSectors(
	const byte* buffer, size_t startSector, size_t nbSectors)
{
	try {
		for (auto i : xrange(nbSectors)) {
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

void SectorAccessibleDisk::checkCaches()
{
	// nothing
}

void SectorAccessibleDisk::flushCaches()
{
	sha1cache.clear();
}

} // namespace openmsx

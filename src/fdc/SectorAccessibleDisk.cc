#include "SectorAccessibleDisk.hh"
#include "EmptyDiskPatch.hh"
#include "IPSPatch.hh"
#include "DiskExceptions.hh"
#include "sha1.hh"
#include "xrange.hh"
#include <memory>

namespace openmsx {

SectorAccessibleDisk::SectorAccessibleDisk()
	: patch(std::make_unique<EmptyDiskPatch>(*this))
{
}

SectorAccessibleDisk::~SectorAccessibleDisk() = default;

void SectorAccessibleDisk::readSector(size_t sector, SectorBuffer& buf)
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
		patch->copyBlock(sector * sizeof(buf), buf.raw, sizeof(buf));
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error: ", e.getMessage());
	}
}

void SectorAccessibleDisk::writeSector(size_t sector, const SectorBuffer& buf)
{
	if (isWriteProtected()) {
		throw WriteProtectedException();
	}
	if (!isDummyDisk() && (getNbSectors() <= sector)) {
		throw NoSuchSectorException("No such sector");
	}
	try {
		writeSectorImpl(sector, buf);
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error: ", e.getMessage());
	}
	flushCaches();
}

size_t SectorAccessibleDisk::getNbSectors() const
{
	return getNbSectorsImpl();
}

void SectorAccessibleDisk::applyPatch(Filename patchFile)
{
	patch = std::make_unique<IPSPatch>(std::move(patchFile), std::move(patch));
}

std::vector<Filename> SectorAccessibleDisk::getPatches() const
{
	return patch->getFilenames();
}

bool SectorAccessibleDisk::hasPatches() const
{
	return !patch->isEmptyPatch();
}

Sha1Sum SectorAccessibleDisk::getSha1Sum(FilePool& filePool)
{
	checkCaches();
	if (sha1cache.empty()) {
		sha1cache = getSha1SumImpl(filePool);
	}
	return sha1cache;
}

Sha1Sum SectorAccessibleDisk::getSha1SumImpl(FilePool& /*filePool*/)
{
	try {
		setPeekMode(true);
		SHA1 sha1;
		for (auto i : xrange(getNbSectors())) {
			SectorBuffer buf;
			readSector(i, buf);
			sha1.update(buf.raw);
		}
		setPeekMode(false);
		return sha1.digest();
	} catch (MSXException&) {
		setPeekMode(false);
		throw;
	}
}

int SectorAccessibleDisk::readSectors (
	SectorBuffer* buffers, size_t startSector, size_t nbSectors)
{
	try {
		for (auto i : xrange(nbSectors)) {
			readSector(startSector + i, buffers[i]);
		}
		return 0;
	} catch (MSXException&) {
		return -1;
	}
}

int SectorAccessibleDisk::writeSectors(
	const SectorBuffer* buffers, size_t startSector, size_t nbSectors)
{
	try {
		for (auto i : xrange(nbSectors)) {
			writeSector(startSector + i, buffers[i]);
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

#include "SectorAccessibleDisk.hh"
#include "DiskImageUtils.hh"
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
	readSectors(&buf, sector, 1);
}

void SectorAccessibleDisk::readSectors(
	SectorBuffer* buffers, size_t startSector, size_t nbSectors)
{
	auto last = startSector + nbSectors - 1;
	if (!isDummyDisk() && // in that case we want DriveEmptyException
	    (last > 1) && // allow reading sector 0 and 1 without calling
	                  // getNbSectors() because this potentially calls
	                  // detectGeometry() and that would cause an
	                  // infinite loop
	    (getNbSectors() <= last)) {
		throw NoSuchSectorException("No such sector");
	}
	try {
		// in the end this calls readSectorsImpl()
		patch->copyBlock(startSector * sizeof(SectorBuffer),
		                 buffers[0].raw.data(),
				 nbSectors * sizeof(SectorBuffer));
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error: ", e.getMessage());
	}
}

void SectorAccessibleDisk::readSectorsImpl(
	SectorBuffer* buffers, size_t startSector, size_t nbSectors)
{
	// Default implementation reads one sector at a time. But subclasses can
	// override this method if they can do it more efficiently.
	for (auto i : xrange(nbSectors)) {
		readSectorImpl(startSector + i, buffers[i]);
	}
}

void SectorAccessibleDisk::readSectorImpl(size_t /*sector*/, SectorBuffer& /*buf*/)
{
	// subclass should override exactly one of
	//    readSectorImpl() or readSectorsImpl()
	assert(false);
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

void SectorAccessibleDisk::writeSectors(
	const SectorBuffer* buffers, size_t startSector, size_t nbSectors)
{
	// Simply write one-at-a-time. There's no possibility (nor any need) yet
	// to allow to optimize this
	for (auto i : xrange(nbSectors)) {
		writeSector(startSector + i, buffers[i]);
	}
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

		constexpr size_t MAX_CHUNK = 32;
		SectorBuffer buf[MAX_CHUNK];
		size_t total = getNbSectors();
		size_t sector = 0;
		while (sector < total) {
			auto chunk = std::min(MAX_CHUNK, total - sector);
			readSectors(buf, sector, chunk);
			sha1.update({buf[0].raw.data(), chunk * sizeof(SectorBuffer)});
			sector += chunk;
		}

		setPeekMode(false);
		return sha1.digest();
	} catch (MSXException&) {
		setPeekMode(false);
		throw;
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

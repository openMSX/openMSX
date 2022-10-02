#include "SectorAccessibleDisk.hh"
#include "DiskImageUtils.hh"
#include "EmptyDiskPatch.hh"
#include "IPSPatch.hh"
#include "DiskExceptions.hh"
#include "enumerate.hh"
#include "sha1.hh"
#include "xrange.hh"
#include <array>
#include <memory>

namespace openmsx {

SectorAccessibleDisk::SectorAccessibleDisk()
	: patch(std::make_unique<EmptyDiskPatch>(*this))
{
}

SectorAccessibleDisk::~SectorAccessibleDisk() = default;

void SectorAccessibleDisk::readSector(size_t sector, SectorBuffer& buf)
{
	readSectors(std::span{&buf, 1}, sector);
}

void SectorAccessibleDisk::readSectors(
	std::span<SectorBuffer> buffers, size_t startSector)
{
	auto last = startSector + buffers.size() - 1;
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
		                 std::span{buffers[0].raw.data(),
		                           buffers.size_bytes()});
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error: ", e.getMessage());
	}
}

void SectorAccessibleDisk::readSectorsImpl(
	std::span<SectorBuffer> buffers, size_t startSector)
{
	// Default implementation reads one sector at a time. But subclasses can
	// override this method if they can do it more efficiently.
	for (auto [i, buf] : enumerate(buffers)) {
		readSectorImpl(startSector + i, buf);
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
	std::span<const SectorBuffer> buffers, size_t startSector)
{
	// Simply write one-at-a-time. There's no possibility (nor any need) yet
	// to allow to optimize this
	for (auto [i, buf] : enumerate(buffers)) {
		writeSector(startSector + i, buf);
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

		std::array<SectorBuffer, 32> buf;
		size_t total = getNbSectors();
		size_t sector = 0;
		while (sector < total) {
			auto chunk = std::min(buf.size(), total - sector);
			auto sub = subspan(buf, 0, chunk);
			readSectors(sub, sector);
			sha1.update({sub[0].raw.data(), sub.size_bytes()});
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

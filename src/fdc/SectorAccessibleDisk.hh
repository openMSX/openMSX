#ifndef SECTORACCESSIBLEDISK_HH
#define SECTORACCESSIBLEDISK_HH

#include "DiskImageUtils.hh"
#include "Filename.hh"

#include "sha1.hh"

#include <memory>
#include <span>
#include <vector>

namespace openmsx {

class FilePool;
class PatchInterface;

class SectorAccessibleDisk
{
public:
	static constexpr size_t SECTOR_SIZE = sizeof(SectorBuffer);

	// sector stuff
	void readSector (size_t sector,       SectorBuffer& buf) const;
	void writeSector(size_t sector, const SectorBuffer& buf);
	void readSectors (std::span<      SectorBuffer> buffers, size_t startSector) const;
	void writeSectors(std::span<const SectorBuffer> buffers, size_t startSector);
	[[nodiscard]] size_t getNbSectors() const;

	// write protected stuff
	[[nodiscard]] bool isWriteProtected() const;
	void forceWriteProtect();

	[[nodiscard]] virtual bool isDummyDisk() const;

	// patch stuff
	void applyPatch(Filename patchFile);
	[[nodiscard]] std::vector<Filename> getPatches() const;
	[[nodiscard]] bool hasPatches() const;

	/** Calculate SHA1 of the content of this disk.
	 * This value is cached (and flushed on writes).
	 */
	[[nodiscard]] Sha1Sum getSha1Sum(FilePool& filePool);

	// should only be called by EmptyDiskPatch
	virtual void readSectorsImpl(
		std::span<SectorBuffer> buffers, size_t startSector);
	// Default readSectorsImpl() implementation delegates to readSectorImpl.
	// Subclasses should override exactly one of these two.
	virtual void readSectorImpl(size_t sector, SectorBuffer& buf);

protected:
	SectorAccessibleDisk();
	~SectorAccessibleDisk();

	// Peek-mode changes the behaviour of readSector(). ATM it only has
	// an effect on DirAsDSK. See comment in DirAsDSK::readSectorImpl()
	// for more details.
	void setPeekMode(bool peek) { peekMode = peek; }
	[[nodiscard]] bool isPeekMode() const { return peekMode; }

	virtual void checkCaches();
	virtual void flushCaches();
	virtual Sha1Sum getSha1SumImpl(FilePool& filePool);

private:
	virtual void writeSectorImpl(size_t sector, const SectorBuffer& buf) = 0;
	[[nodiscard]] virtual size_t getNbSectorsImpl() const = 0;
	[[nodiscard]] virtual bool isWriteProtectedImpl() const = 0;

private:
	std::unique_ptr<const PatchInterface> patch;
	Sha1Sum sha1cache;
	bool forcedWriteProtect = false;
	bool peekMode = false;
};

} // namespace openmsx

#endif

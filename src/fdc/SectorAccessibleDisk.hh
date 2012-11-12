// $Id$

#ifndef SECTORACCESSIBLEDISK_HH
#define SECTORACCESSIBLEDISK_HH

#include "Filename.hh"
#include "sha1.hh"
#include "openmsx.hh"
#include <vector>
#include <string>
#include <memory>

namespace openmsx {

class PatchInterface;

class SectorAccessibleDisk
{
public:
	static const unsigned SECTOR_SIZE = 512;

	virtual ~SectorAccessibleDisk();

	// sector stuff
	void readSector(unsigned sector, byte* buf);
	void writeSector(unsigned sector, const byte* buf);
	unsigned getNbSectors() const;

	// write protected stuff
	bool isWriteProtected() const;
	void forceWriteProtect();

	virtual bool isDummyDisk() const;

	// patch stuff
	void applyPatch(const Filename& patchFile);
	std::vector<Filename> getPatches() const;
	bool hasPatches() const;

	/** Calculate SHA1 of the content of this disk.
	 * This value is cached (and flushed on writes).
	 */
	virtual Sha1Sum getSha1Sum();

	// For compatibility with nowind
	//  - read/write multiple sectors instead of one-per-one
	//  - use error codes instead of exceptions
	//  - different order of parameters
	int readSectors (      byte* buffer, unsigned startSector,
	                 unsigned nbSectors);
	int writeSectors(const byte* buffer, unsigned startSector,
	                 unsigned nbSectors);

protected:
	SectorAccessibleDisk();

	// Peek-mode changes the behaviour of readSector(). ATM it only has
	// an effect on DirAsDSK. See comment in DirAsDSK::readSectorImpl()
	// for more details.
	void setPeekMode(bool peek) { peekMode = peek; }
	bool isPeekMode() const { return peekMode; }

	virtual void checkCaches();
	virtual void flushCaches();

private:
	virtual void readSectorImpl(unsigned sector, byte* buf) = 0;
	virtual void writeSectorImpl(unsigned sector, const byte* buf) = 0;
	virtual unsigned getNbSectorsImpl() const = 0;
	virtual bool isWriteProtectedImpl() const = 0;

	std::unique_ptr<const PatchInterface> patch;
	Sha1Sum sha1cache;
	bool forcedWriteProtect;
	bool peekMode;

	friend class EmptyDiskPatch;
};

} // namespace openmsx

#endif

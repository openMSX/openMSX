// $Id$

#ifndef SECTORACCESSIBLEDISK_HH
#define SECTORACCESSIBLEDISK_HH

#include "WriteProtectableDisk.hh"
#include "openmsx.hh"
#include <string>

namespace openmsx {

class SectorAccessibleDisk : public virtual WriteProtectableDisk
{
public:
	static const unsigned SECTOR_SIZE = 512;

	virtual ~SectorAccessibleDisk();

	void readSector(unsigned sector, byte* buf);
	void writeSector(unsigned sector, const byte* buf);
	unsigned getNbSectors() const;

	/** Calculate SHA1 of the content of this disk.
	 * This value is cached (and flushed on writes).
	 */
	const std::string& getSHA1Sum();

	// For compatibility with nowind
	//  - read/write multiple sectors instead of one-per-one
	//  - use error codes instead of exceptions
	//  - different order of parameters
	int readSectors (      byte* buffer, unsigned startSector,
	                 unsigned nbSectors);
	int writeSectors(const byte* buffer, unsigned startSector,
	                 unsigned nbSectors);

private:
	virtual void readSectorImpl(unsigned sector, byte* buf) = 0;
	virtual void writeSectorImpl(unsigned sector, const byte* buf) = 0;
	virtual unsigned getNbSectorsImpl() const = 0;

	std::string sha1cache;
};

} // namespace openmsx

#endif

// $Id$

#ifndef SECTORBASEDDISK_HH
#define SECTORBASEDDISK_HH

#include "Disk.hh"
#include "SectorAccessibleDisk.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class PatchInterface;

class SectorBasedDisk : public Disk, public SectorAccessibleDisk,
                        private noncopyable
{
public:
	static const unsigned SECTOR_SIZE = 512;

	// Disk
	virtual void read(byte track, byte sector, byte side,
	                  unsigned size, byte* buf);
	virtual void write(byte track, byte sector, byte side,
	                   unsigned size, const byte* buf);
	virtual void writeTrackData(byte track, byte side, const byte* data);
	virtual void readTrackData(byte track, byte side, byte* output);
	virtual bool ready();
	virtual void applyPatch(const std::string& patchFile);

	// SectorAccessibleDisk
	//  does apply IPS patches, does error checking
	virtual void readSector(unsigned sector, byte* buf);
	virtual void writeSector(unsigned sector, const byte* buf);
	unsigned getNbSectors() const;

	// low level sector routines
	//   doesn't apply IPS patches, doesn't do error checking
	virtual void readSectorImpl(unsigned sector, byte* buf) = 0;
	virtual void writeSectorImpl(unsigned sector, const byte* buf) = 0;

protected:
	explicit SectorBasedDisk(const std::string& name);
	virtual ~SectorBasedDisk();
	virtual void detectGeometry();

	void setNbSectors(unsigned num);

private:
	std::auto_ptr<const PatchInterface> patch;
	unsigned nbSectors;
};

} // namespace openmsx

#endif

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
	virtual void applyPatch(const Filename& patchFile);
	virtual void getPatches(std::vector<Filename>& result) const;

protected:
	explicit SectorBasedDisk(const Filename& name);
	virtual ~SectorBasedDisk();
	virtual void detectGeometry();

	void setNbSectors(unsigned num);

private:
	// SectorAccessibleDisk
	//  does apply IPS patches, does error checking
	virtual void readSectorImpl(unsigned sector, byte* buf);
	virtual void writeSectorImpl(unsigned sector, const byte* buf);
	unsigned getNbSectorsImpl() const;

	// Disk
	virtual void read(byte track, byte sector, byte side,
	                  unsigned size, byte* buf);
	virtual void readTrackData(byte track, byte side, byte* output);
	virtual bool ready();
	virtual void writeImpl(byte track, byte sector, byte side,
	                       unsigned size, const byte* buf);
	virtual void writeTrackDataImpl(byte track, byte side, const byte* data);

	// low level sector routines
	//   doesn't apply IPS patches, doesn't do error checking
	friend class EmptyDiskPatch;
	virtual void readSectorSBD(unsigned sector, byte* buf) = 0;
	virtual void writeSectorSBD(unsigned sector, const byte* buf) = 0;

	std::auto_ptr<const PatchInterface> patch;
	unsigned nbSectors;
};

} // namespace openmsx

#endif

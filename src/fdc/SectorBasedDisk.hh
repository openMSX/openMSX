// $Id$

#ifndef SECTORBASEDDISK_HH
#define SECTORBASEDDISK_HH

#include "Disk.hh"
#include "noncopyable.hh"

namespace openmsx {

class SectorBasedDisk : public Disk, private noncopyable
{
protected:
	explicit SectorBasedDisk(const DiskName& name);
	virtual ~SectorBasedDisk();
	virtual void detectGeometry();

	void setNbSectors(unsigned num);

private:
	// Disk
	virtual unsigned getNbSectorsImpl() const;
	virtual void read(byte track, byte sector, byte side,
	                  unsigned size, byte* buf);
	virtual void readTrackData(byte track, byte side, byte* output);
	virtual bool isReady() const;
	virtual void writeImpl(byte track, byte sector, byte side,
	                       unsigned size, const byte* buf);
	virtual void writeTrackDataImpl(byte track, byte side, const byte* data);

	unsigned nbSectors;
};

} // namespace openmsx

#endif

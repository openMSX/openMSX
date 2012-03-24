// $Id$

#ifndef SECTORBASEDDISK_HH
#define SECTORBASEDDISK_HH

#include "Disk.hh"
#include "noncopyable.hh"

namespace openmsx {

/** Abstract class for disk images that only represent the logical sector
  * information (so not the raw track data that is sometimes needed for
  * copy-protected disks).
  */
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
	virtual void writeImpl(byte track, byte sector, byte side,
	                       unsigned size, const byte* buf);
	virtual void writeTrackDataImpl(byte track, byte side, const byte* data);
	virtual void readTrack(byte track, byte side, RawTrack& output);
	virtual void writeTrackImpl(byte track, byte side, const RawTrack& input);

	unsigned nbSectors;
};

} // namespace openmsx

#endif

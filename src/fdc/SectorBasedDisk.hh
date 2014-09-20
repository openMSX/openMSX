#ifndef SECTORBASEDDISK_HH
#define SECTORBASEDDISK_HH

#include "Disk.hh"
#include "RawTrack.hh"
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
	virtual void detectGeometry();
	virtual void flushCaches();

	void setNbSectors(size_t num);

protected:
	~SectorBasedDisk() {}

private:
	// Disk
	virtual size_t getNbSectorsImpl() const;
	virtual void readTrack(byte track, byte side, RawTrack& output);
	virtual void writeTrackImpl(byte track, byte side, const RawTrack& input);

	size_t nbSectors;

	RawTrack cachedTrackData;
	int cachedTrackNum;
};

} // namespace openmsx

#endif

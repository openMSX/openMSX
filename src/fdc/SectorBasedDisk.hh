#ifndef SECTORBASEDDISK_HH
#define SECTORBASEDDISK_HH

#include "Disk.hh"
#include "RawTrack.hh"

namespace openmsx {

/** Abstract class for disk images that only represent the logical sector
  * information (so not the raw track data that is sometimes needed for
  * copy-protected disks).
  */
class SectorBasedDisk : public Disk
{
protected:
	explicit SectorBasedDisk(const DiskName& name);
	void detectGeometry() override;
	void flushCaches() override;

	void setNbSectors(size_t num);

protected:
	~SectorBasedDisk() {}

private:
	// Disk
	size_t getNbSectorsImpl() const override;
	void readTrack(byte track, byte side, RawTrack& output) override;
	void writeTrackImpl(byte track, byte side, const RawTrack& input) override;

	size_t nbSectors;

	RawTrack cachedTrackData;
	int cachedTrackNum;
};

} // namespace openmsx

#endif

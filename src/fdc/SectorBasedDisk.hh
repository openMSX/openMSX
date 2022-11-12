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
	explicit SectorBasedDisk(DiskName name);
	void detectGeometry() override;
	void flushCaches() override;

	void setNbSectors(size_t num);

protected:
	~SectorBasedDisk() override = default;

private:
	// Disk
	[[nodiscard]] size_t getNbSectorsImpl() const override;
	void readTrack(uint8_t track, uint8_t side, RawTrack& output) override;
	void writeTrackImpl(uint8_t track, uint8_t side, const RawTrack& input) override;

private:
	size_t nbSectors = size_t(-1); // to detect misuse
	RawTrack cachedTrackData;
	int cachedTrackNum = -1;
};

} // namespace openmsx

#endif

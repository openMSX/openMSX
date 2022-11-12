#ifndef DISK_HH
#define DISK_HH

#include "SectorAccessibleDisk.hh"
#include "DiskName.hh"
#include <cstdint>

namespace openmsx {

class RawTrack;

class Disk : public SectorAccessibleDisk
{
public:
	virtual ~Disk() = default;

	[[nodiscard]] const DiskName& getName() const { return name; }

	/** Replace a full track in this image with the given track. */
	void writeTrack(uint8_t track, uint8_t side, const RawTrack& input);

	/** Read a full track from this disk image. */
	virtual void readTrack (uint8_t track, uint8_t side, RawTrack& output) = 0;

	/** Has the content of this disk changed, by some other means than the
	 * MSX writing to the disk. In other words: should caches on the MSX
	 * side be dropped? (E.g. via the 'disk-changed-signal' that's present
	 * in (some) MSX disk interfaces). */
	[[nodiscard]] virtual bool hasChanged() const { return false; }

	[[nodiscard]] bool isDoubleSided();

protected:
	explicit Disk(DiskName name);
	[[nodiscard]] size_t physToLog(uint8_t track, uint8_t side, uint8_t sector);
	struct TSS { uint8_t track, side, sector; };
	[[nodiscard]] TSS logToPhys(size_t log);

	virtual void detectGeometry();
	virtual void detectGeometryFallback();

	void setSectorsPerTrack(unsigned num) { sectorsPerTrack = num; }
	[[nodiscard]] unsigned getSectorsPerTrack();
	void setNbSides(unsigned num) {	nbSides = num; }

	virtual void writeTrackImpl(uint8_t track, uint8_t side, const RawTrack& input) = 0;

private:
	const DiskName name;
	unsigned sectorsPerTrack;
	unsigned nbSides = 0;
};

} // namespace openmsx

#endif

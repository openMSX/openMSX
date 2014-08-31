#ifndef DISK_HH
#define DISK_HH

#include "SectorAccessibleDisk.hh"
#include "DiskName.hh"
#include "openmsx.hh"

namespace openmsx {

class RawTrack;

class Disk : public SectorAccessibleDisk
{
public:
	virtual ~Disk() {}

	const DiskName& getName() const { return name; }

	/** Replace a full track in this image with the given track. */
	        void writeTrack(byte track, byte side, const RawTrack& input);

	/** Read a full track from this disk image. */
	virtual void readTrack (byte track, byte side,       RawTrack& output) = 0;

	bool isDoubleSided();

protected:
	explicit Disk(const DiskName& name);
	size_t physToLog(byte track, byte side, byte sector);
	void logToPhys(size_t log, byte& track, byte& side, byte& sector);

	virtual void detectGeometry();
	virtual void detectGeometryFallback();

	void setSectorsPerTrack(unsigned num) { sectorsPerTrack = num; }
	unsigned getSectorsPerTrack();
	void setNbSides(unsigned num) {	nbSides = num; }

	virtual void writeTrackImpl(byte track, byte side, const RawTrack& input) = 0;

private:
	const DiskName name;
	unsigned sectorsPerTrack;
	unsigned nbSides;
};

} // namespace openmsx

#endif

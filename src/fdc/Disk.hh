// $Id$

#ifndef DISK_HH
#define DISK_HH

#include "SectorAccessibleDisk.hh"
#include "RawTrack.hh"
#include "DiskName.hh"
#include "openmsx.hh"

namespace openmsx {

class Disk : public SectorAccessibleDisk
{
public:
	// TODO This should be 6250 (see calculation in RawTrack), but it will
	//      soon be replaced.
	static const int RAWTRACK_SIZE = 6850;

	virtual ~Disk();

	const DiskName& getName() const;

	virtual void read(byte track, byte sector,
	                  byte side, unsigned size, byte* buf) = 0;
	void write(byte track, byte sector,
	           byte side, unsigned size, const byte* buf);
	virtual void getSectorHeader(byte track, byte sector,
	                             byte side, byte* buf);
	virtual void getTrackHeader(byte track,
	                            byte side, byte* buf);
	void writeTrackData(byte track, byte side, const byte* data);
	virtual void readTrackData(byte track, byte side, byte* output);

	/** Replace a full track in this image with the given track. */
	        void writeTrack(byte track, byte side, const RawTrack& input);

	/** Read a full track from this disk image. */
	virtual void readTrack (byte track, byte side,       RawTrack& output) = 0;

	bool isDoubleSided();

protected:
	explicit Disk(const DiskName& name);
	int physToLog(byte track, byte side, byte sector);
	void logToPhys(int log, byte& track, byte& side, byte& sector);

	virtual void detectGeometry();

	void setSectorsPerTrack(unsigned num);
	unsigned getSectorsPerTrack();
	void setNbSides(unsigned num);

	virtual void writeImpl(byte track, byte sector,
	                       byte side, unsigned size, const byte* buf) = 0;
	virtual void writeTrackDataImpl(byte track, byte side, const byte* data);
	virtual void writeTrackImpl(byte track, byte side, const RawTrack& input) = 0;

private:
	void detectGeometryFallback();

	const DiskName name;
	unsigned sectorsPerTrack;
	unsigned nbSides;
};

} // namespace openmsx

#endif

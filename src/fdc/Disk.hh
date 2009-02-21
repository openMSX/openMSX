// $Id$

#ifndef DISK_HH
#define DISK_HH

#include "SectorAccessibleDisk.hh"
#include "DiskName.hh"
#include "openmsx.hh"

namespace openmsx {

class Disk : public SectorAccessibleDisk
{
public:
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

	virtual bool isReady() const = 0;
	bool isDoubleSided() const;

protected:
	explicit Disk(const DiskName& name);
	int physToLog(byte track, byte side, byte sector);
	void logToPhys(int log, byte& track, byte& side, byte& sector);

	virtual void detectGeometry();

	void setSectorsPerTrack(unsigned num);
	void setNbSides(unsigned num);

	virtual void writeImpl(byte track, byte sector,
	                       byte side, unsigned size, const byte* buf) = 0;
	virtual void writeTrackDataImpl(byte track, byte side, const byte* data);

private:
	void detectGeometryFallback();

	const DiskName name;
	unsigned sectorsPerTrack;
	unsigned nbSides;
};

} // namespace openmsx

#endif

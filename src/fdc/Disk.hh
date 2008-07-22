// $Id$

#ifndef DISK_HH
#define DISK_HH

#include "openmsx.hh"
#include <string>

namespace openmsx {

class Disk
{
public:
	virtual ~Disk();

	const std::string& getName() const;

	virtual void read (byte track, byte sector,
	                   byte side, unsigned size, byte* buf) = 0;
	virtual void write(byte track, byte sector,
	                   byte side, unsigned size, const byte* buf) = 0;
	virtual void getSectorHeader(byte track, byte sector,
	                             byte side, byte* buf);
	virtual void getTrackHeader(byte track,
	                            byte side, byte* buf);
	virtual void initWriteTrack(byte track, byte side);
	virtual void writeTrackData(byte data);
	virtual void readTrackData(byte track, byte side, byte* output);

	virtual bool ready() = 0;
	virtual bool writeProtected() = 0;
	virtual bool doubleSided() = 0;

	virtual void applyPatch(const std::string& patchFile);

protected:
	static const int RAWTRACK_SIZE = 6850;

	explicit Disk(const std::string& name);
	int physToLog(byte track, byte side, byte sector);
	void logToPhys(int log, byte& track, byte& side, byte& sector);

	virtual void detectGeometry();
	virtual int getImageSize();

	int sectorsPerTrack;
	int nbSides;

private:
	void detectGeometryFallback();

	const std::string name;
};

} // namespace openmsx

#endif

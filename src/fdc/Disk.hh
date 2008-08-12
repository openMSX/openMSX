// $Id$

#ifndef DISK_HH
#define DISK_HH

#include "Filename.hh"
#include "openmsx.hh"
#include <string>

namespace openmsx {

class Disk
{
public:
	static const int RAWTRACK_SIZE = 6850;

	virtual ~Disk();

	const Filename& getName() const;

	virtual void read (byte track, byte sector,
	                   byte side, unsigned size, byte* buf) = 0;
	virtual void write(byte track, byte sector,
	                   byte side, unsigned size, const byte* buf) = 0;
	virtual void getSectorHeader(byte track, byte sector,
	                             byte side, byte* buf);
	virtual void getTrackHeader(byte track,
	                            byte side, byte* buf);
	virtual void writeTrackData(byte track, byte side, const byte* data);
	virtual void readTrackData(byte track, byte side, byte* output);

	virtual bool ready() = 0;
	virtual bool writeProtected() = 0;
	bool doubleSided();

	virtual void applyPatch(const std::string& patchFile);

protected:
	explicit Disk(const Filename& name);
	int physToLog(byte track, byte side, byte sector);
	void logToPhys(int log, byte& track, byte& side, byte& sector);

	virtual void detectGeometry();
	virtual int getImageSize();

	void setSectorsPerTrack(unsigned num);
	void setNbSides(unsigned num);

private:
	void detectGeometryFallback();

	const Filename name;
	unsigned sectorsPerTrack;
	unsigned nbSides;
};

} // namespace openmsx

#endif

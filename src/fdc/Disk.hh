// $Id$

#ifndef DISK_HH
#define DISK_HH

#include "WriteProtectableDisk.hh"
#include "Filename.hh"
#include "openmsx.hh"
#include <vector>

namespace openmsx {

class Disk : public virtual WriteProtectableDisk
{
public:
	static const int RAWTRACK_SIZE = 6850;

	virtual ~Disk();

	const Filename& getName() const;

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

	virtual bool ready() = 0;
	bool doubleSided();

	virtual void applyPatch(const Filename& patchFile);
	virtual void getPatches(std::vector<Filename>& result) const;

protected:
	explicit Disk(const Filename& name);
	int physToLog(byte track, byte side, byte sector);
	void logToPhys(int log, byte& track, byte& side, byte& sector);

	virtual void detectGeometry();
	virtual int getImageSize();

	void setSectorsPerTrack(unsigned num);
	void setNbSides(unsigned num);

	virtual void writeImpl(byte track, byte sector,
	                       byte side, unsigned size, const byte* buf) = 0;
	virtual void writeTrackDataImpl(byte track, byte side, const byte* data);

private:
	void detectGeometryFallback();

	const Filename name;
	unsigned sectorsPerTrack;
	unsigned nbSides;
};

} // namespace openmsx

#endif

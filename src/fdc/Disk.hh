// $Id$

#ifndef __DISK_HH__
#define __DISK_HH__

#include "MSXException.hh"
#include "openmsx.hh"

namespace openmsx {

class NoSuchSectorException : public MSXException {
public:
	NoSuchSectorException(const std::string &desc)
		: MSXException(desc) {}
};
class DiskIOErrorException  : public MSXException {
public:
	DiskIOErrorException(const std::string &desc)
		: MSXException(desc) {}
};
class DriveEmptyException  : public MSXException {
public:
	DriveEmptyException(const std::string &desc)
		: MSXException(desc) {}
};
class WriteProtectedException  : public MSXException {
public:
	WriteProtectedException(const std::string &desc)
		: MSXException(desc) {}
};


class Disk 
{
public: 
	virtual ~Disk();
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
	virtual void initReadTrack(byte track, byte side);
	virtual byte readTrackData();
	
	virtual bool ready() = 0;
	virtual bool writeProtected() = 0;
	virtual bool doubleSided() = 0;
	
	virtual void applyPatch(const std::string& patchFile);

protected:
	static const int RAWTRACK_SIZE = 6850;

	Disk();
	int physToLog(byte track, byte side, byte sector);
	void logToPhys(int log, byte &track, byte &side, byte &sector);

	virtual void detectGeometry();

	virtual int getImageSize();

	int sectorsPerTrack;
	int nbSides;

private:
	void detectGeometryFallback();
};

} // namespace openmsx

#endif

// $Id$

#ifndef __SECTORBASEDDISK_HH__
#define __SECTORBASEDDISK_HH__

#include "Disk.hh"

namespace openmsx {

class SectorBasedDisk : public Disk
{
public: 
	virtual void initWriteTrack(byte track, byte side);
	virtual void writeTrackData(byte data);
	virtual void initReadTrack(byte track, byte side);
	virtual byte readTrackData();
	virtual bool ready();

protected:
	static const int SECTOR_SIZE = 512;

	SectorBasedDisk();
	virtual ~SectorBasedDisk();
	virtual void detectGeometry();
	
	int nbSectors;

private:
	int getImageSize();

	byte writeTrackBuf[SECTOR_SIZE];
	int writeTrackBufCur;
	int writeTrackSectorCur;
	byte writeTrack_track;
	byte writeTrack_side;
	byte writeTrack_sector;
	int writeTrack_CRCcount;
	byte readTrackDataBuf[RAWTRACK_SIZE];
	int readTrackDataCount;
};

} // namespace openmsx

#endif

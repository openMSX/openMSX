// $Id$

#ifndef SECTORBASEDDISK_HH
#define SECTORBASEDDISK_HH

#include "Disk.hh"
#include "SectorAccessibleDisk.hh"
#include <memory>

namespace openmsx {

class PatchInterface;

class SectorBasedDisk : public Disk, public SectorAccessibleDisk
{
public: 
	static const unsigned SECTOR_SIZE = 512;

	virtual void read(byte track, byte sector, byte side,
	                  unsigned size, byte* buf);
	virtual void write(byte track, byte sector, byte side,
	                   unsigned size, const byte* buf);
	virtual void initWriteTrack(byte track, byte side);
	virtual void writeTrackData(byte data);
	virtual void initReadTrack(byte track, byte side);
	virtual byte readTrackData();
	virtual bool ready();
	virtual bool doubleSided();

	//virtual void readLogicalSector(unsigned sector, byte* buf) = 0;
	//virtual void writeLogicalSector(unsigned sector, const byte* buf) = 0;
	virtual void applyPatch(const std::string& patchFile);
	unsigned getNbSectors() const;

protected:
	SectorBasedDisk();
	virtual ~SectorBasedDisk();
	virtual void detectGeometry();
	
	unsigned nbSectors;

private:
	byte writeTrackBuf[SECTOR_SIZE];
	int writeTrackBufCur;
	int writeTrackSectorCur;
	byte writeTrack_track;
	byte writeTrack_side;
	byte writeTrack_sector;
	int writeTrack_CRCcount;
	byte readTrackDataBuf[RAWTRACK_SIZE];
	int readTrackDataCount;

	std::auto_ptr<const PatchInterface> patch;
};

} // namespace openmsx

#endif

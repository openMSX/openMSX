// $Id$

#include "SectorBasedDisk.hh"
#include "EmptyDiskPatch.hh"
#include "IPSPatch.hh"
#include <cassert>

namespace openmsx {

SectorBasedDisk::SectorBasedDisk()
	: patch(new EmptyDiskPatch(*this))
{
}

SectorBasedDisk::~SectorBasedDisk()
{
}

void SectorBasedDisk::read(byte track, byte sector, byte side,
                           unsigned size, byte* buf)
{
	if (size); // avoid warning
	assert(size == SECTOR_SIZE);
	unsigned logicalSector = physToLog(track, side, sector);
	if (logicalSector >= nbSectors) {
		throw NoSuchSectorException("No such sector");
	}
	try {
		patch->copyBlock(logicalSector * SECTOR_SIZE, buf, SECTOR_SIZE);
	} catch (MSXException& e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void SectorBasedDisk::write(byte track, byte sector, byte side, 
                            unsigned size, const byte* buf)
{
	if (size); // avoid warning
	assert(size == SECTOR_SIZE);
	if (writeProtected()) {
		throw WriteProtectedException("");
	}
	unsigned logicalSector = physToLog(track, side, sector);
	if (logicalSector >= nbSectors) {
		throw NoSuchSectorException("No such sector");
	}
	try {
		writeLogicalSector(logicalSector, buf);
	} catch (MSXException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void SectorBasedDisk::applyPatch(const std::string& patchFile)
{
	patch.reset(new IPSPatch(patchFile, patch));
}

void SectorBasedDisk::initWriteTrack(byte track, byte side)
{
	if (writeProtected()) {
		throw WriteProtectedException("");
	}
	
	writeTrackBufCur = 0;
	writeTrack_track = track;
	writeTrack_side = side;
	writeTrack_sector = 1;
	writeTrack_CRCcount = 0;
}

void SectorBasedDisk::writeTrackData(byte data)
{
	if (writeProtected()) {
		throw WriteProtectedException("");
	}
	
	// if it is a 0xF7 ("two CRC characters") then the previous 512
	// bytes could be actual sectordata bytes
	if (data == 0xF7) {
		if (writeTrack_CRCcount & 1) {
			// first CRC is sector header CRC, second CRC is actual
			// sector data CRC so write them 
			byte tempWriteBuf[512];
			for (int i = 0; i < 512; i++) {
				tempWriteBuf[i] =
				      writeTrackBuf[(writeTrackBufCur+i) & 511];
			}
			write(writeTrack_track, writeTrack_sector,
			      writeTrack_side, 512, tempWriteBuf);
			writeTrack_sector++; // update sector counter
		}
		writeTrack_CRCcount++; 
	} else {
		writeTrackBuf[writeTrackBufCur++] = data;
		writeTrackBufCur &= 511;
	}
}

void SectorBasedDisk::initReadTrack(byte track, byte side)
{
	// init following data structure
	// according to Alex Wulms
	// 122 bytes track header aka pre-gap
	// 9 * 628 bytes sectordata (sector header, data en closure gap)
	// 1080 bytes end-of-track gap
	//
	// This data comes from the TC8566AF manual
	// each track in IBM format contains
	// '4E' x 80, '00' x 12, 'C2' x 3
	// 'FC' x 1, '4E' x 50, sector data 1 to n
	// '4E' x ?? (closing gap)
	// each sector data contains
	// '00' x 12, 'A1' x 3, 'FE' x 1,
	// C,H,R,N,CRC(2bytes), '4E' x 22, '00' x 12,
	// 'A1' x 4,'FB'('F8') x 1, data(512 bytes),CRC(2bytes),'4E'(gap3)
	
	readTrackDataCount = 0;
	byte* tmppoint = readTrackDataBuf;
	// Fill track header
	for (int i = 0; i < 80; i++) *(tmppoint++) = 0x4E;
	for (int i = 0; i < 12; i++) *(tmppoint++) = 0x00;
	for (int i = 0; i <  3; i++) *(tmppoint++) = 0xC2;
	*(tmppoint++) = 0xFC;
	for (int i = 0; i < 50; i++) *(tmppoint++) = 0x4E;
	// Fill sector
	for (byte j = 0; j < 9; j++) {
		// Fill sector header
		for (int i = 0; i < 12; i++) *(tmppoint++) = 0x00;
		for (int i = 0; i <  3; i++) *(tmppoint++) = 0xA1;
		*(tmppoint++) = 0xFE;
		*(tmppoint++) = track;		//C: Cylinder number
		*(tmppoint++) = side;		//H: Head Address
		*(tmppoint++) = (byte)(j + 1);	//R: Record
		*(tmppoint++) = 0x02;		//N: Number (length of sector)
		*(tmppoint++) = 0x00;		//CRC byte 1
		*(tmppoint++) = 0x00;		//CRC byte 2
		// read sector data
		read(track, j + 1, side, 512, tmppoint);
		tmppoint += 512;
		// end of sector
		*(tmppoint++) = (byte)(j+1);	//CRC byte 1
		*(tmppoint++) = (byte)(j+1);	//CRC byte 2
		// TODO: check this number
		for (int i = 0; i < 55; i++) *(tmppoint++) = 0x4E;
		// TODO build correct CRC and read sector + place gap
	}
	// TODO look up value of this end-of-track gap
	for (int i = 0; i < 1080; i++) *(tmppoint++) = 0x4E;
}

byte SectorBasedDisk::readTrackData()
{
	if (readTrackDataCount == RAWTRACK_SIZE) {
		// end of command in any case
		return readTrackDataBuf[RAWTRACK_SIZE - 1];
	} else {
		return readTrackDataBuf[readTrackDataCount++];
	}
}

bool SectorBasedDisk::ready()
{
	return true;
}

bool SectorBasedDisk::doubleSided()
{
	return nbSides == 2;
}

unsigned SectorBasedDisk::getNbSectors() const
{
	return nbSectors;
}

void SectorBasedDisk::detectGeometry()
{
	// the following are just heuristics...
	
	if (nbSectors == 1440) {
		// explicitly check for 720kb filesize
		 
		// "trojka.dsk" is 720kb, but has bootsector and FAT media ID
		// for a single sided disk. From an emulator point of view it
		// must be accessed as a double sided disk.
		 
		// "SDSNAT2.DSK" has invalid media ID in both FAT and
		// bootsector, other data in the bootsector is invalid as well.
		// Altough the first byte of the bootsector is 0xE9 to indicate
		// valid bootsector data. The only way to detect the format is
		// to look at the diskimage filesize.
		
		sectorsPerTrack = 9;
		nbSides = 2;

	} else {
		// Don't check for "360kb -> single sided disk". The MSXMania
		// disks are double sided disk but are truncated at 360kb.
		Disk::detectGeometry();
	}
}

} // namespace openmsx

// $Id$

#include "FDC_DSK.hh"



FDC_DSK::FDC_DSK(const std::string &fileName)
{
	file = new File(fileName, DISK);
	nbSectors = file->size() / SECTOR_SIZE;
	writeTrackBuf = new byte[SECTOR_SIZE];
}

FDC_DSK::~FDC_DSK()
{
	delete file;
	delete[] writeTrackBuf;
}

void FDC_DSK::read(byte phystrack, byte track, byte sector, byte side,
                   int size, byte* buf)
{
	try {
		int logicalSector = physToLog(track, side, sector);
		if (logicalSector >= nbSectors)
			throw NoSuchSectorException("No such sector");
		file->seek(logicalSector * SECTOR_SIZE);
		file->read(buf, SECTOR_SIZE);
	} catch (FileException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void FDC_DSK::write(byte phystrack, byte track, byte sector, byte side, 
                    int size, const byte* buf)
{
	try {
		int logicalSector = physToLog(track, side, sector);
		if (logicalSector >= nbSectors)
			throw NoSuchSectorException("No such sector");
		file->seek(logicalSector * SECTOR_SIZE);
		file->write(buf, SECTOR_SIZE);
	} catch (FileException &e) {
		throw DiskIOErrorException("Disk I/O error");
	}
}

void FDC_DSK::readBootSector()
{
	if (nbSectors == 1440) {
		sectorsPerTrack = 9;
		nbSides = 2;
	} else if (nbSectors == 720) {
		sectorsPerTrack = 9;
		nbSides = 1;
	} else {
		FDCBackEnd::readBootSector();
	}
}

void FDC_DSK::initWriteTrack(byte phystrack, byte track, byte side)
{
	writeTrackBufCur=0;
	writeTrack_phystrack=phystrack;
	writeTrack_track=track;
	writeTrack_side=side;
	writeTrack_sector=1;
}

void FDC_DSK::writeTrackData(byte data)
{
	// if it is a 0xF7("two CRC characters") then the previous 512 bytes are actual sectordata bytes
	// so write them and update sector counter.
	if (data == 0xF7){
		write(writeTrack_phystrack,writeTrack_track,writeTrack_sector,writeTrack_side,511,writeTrackBuf);
		writeTrack_sector++;
	} else {
		writeTrackBuf[writeTrackBufCur++]=data;
		writeTrackBufCur&=511;
	}
}

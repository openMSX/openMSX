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
	writeTrack_CRCcount=0;

	PRT_DEBUG("tmpdebugcounter is "<<tmpdebugcounter);
	tmpdebugcounter=0;
}

void FDC_DSK::writeTrackData(byte data)
{
	tmpdebugcounter++;
	// if it is a 0xF7("two CRC characters") then the previous 512 bytes could be actual sectordata bytes
	if (data == 0xF7){
		if (writeTrack_CRCcount&1){ // first CRC is sector header CRC, second CRC is actual sector data CRC
			// so write them 
			byte* tempWriteBuf;
			tempWriteBuf = new byte[512] ;
			for (int i=0; i<512 ;i++ ){
				tempWriteBuf[i]=writeTrackBuf[(writeTrackBufCur+i)&511];
			}
			write(writeTrack_phystrack,writeTrack_track,writeTrack_sector,writeTrack_side,511,tempWriteBuf);
			writeTrack_sector++; // update sector counter.
		};
		writeTrack_CRCcount++; 
	} else {
		writeTrackBuf[writeTrackBufCur++]=data;
		writeTrackBufCur&=511;
	}
}

bool FDC_DSK::ready()
{
	return true;
}

bool FDC_DSK::writeProtected()
{
	return false;	// TODO
}

bool FDC_DSK::doubleSided()
{
	return nbSides == 2;
}

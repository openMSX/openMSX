// $Id$

#include "FDC_DSK.hh"



FDC_DSK::FDC_DSK(const std::string &fileName)
{
	file = new File(fileName, DISK);
	nbSectors = file->size() / SECTOR_SIZE;
	writeTrackBuf = new byte[SECTOR_SIZE];
	readTrackDataBuf = new byte[RAWTRACK_SIZE];
}

FDC_DSK::~FDC_DSK()
{
	delete file;
	delete[] writeTrackBuf;
	delete[] readTrackDataBuf;
}

void FDC_DSK::read(byte track, byte sector, byte side,
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

void FDC_DSK::write(byte track, byte sector, byte side, 
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

void FDC_DSK::initWriteTrack(byte track, byte side)
{
	writeTrackBufCur=0;
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
			write(writeTrack_track, writeTrack_sector, writeTrack_side,
			      512, tempWriteBuf);
			writeTrack_sector++; // update sector counter.
		};
		writeTrack_CRCcount++; 
	} else {
		writeTrackBuf[writeTrackBufCur++]=data;
		writeTrackBufCur&=511;
	}
}

void FDC_DSK::initReadTrack(byte track, byte side){
	readTrackDataCount=0;
	/* init following data structure
	122 bytes track header aka pre-gap
	9 * 628 bytes sectordata (sector header, data en closure gap)
	1080 bytes end-of-track gap
	*/
	byte* tmppoint;
	tmppoint=readTrackDataBuf;
	for (int i=0 ; i<122 ; i++ ) *(tmppoint++)=0 ; //TODO look up value of this first pre-gap
	for (byte j=0 ; j<9 ; j++){
		*(tmppoint++)=(byte)(j+1);
		//TODO build correct header and read sector +place gap
	};
	for (int i=0 ; i<1080 ; i++ ) *(tmppoint++)=0 ; //TODO look up value of this end-of-track gap



}

byte FDC_DSK::readTrackData()
{
	if (readTrackDataCount == RAWTRACK_SIZE){
		// end of command in any case
		return readTrackDataBuf[RAWTRACK_SIZE];
	} else
	return readTrackDataBuf[readTrackDataCount++];
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

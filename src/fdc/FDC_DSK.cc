// $Id$

#include "FDC_DSK.hh"



FDC_DSK::FDC_DSK(const FileContext *context, const std::string &fileName)
{
	file = new File(context, fileName);
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
	according to Alex Wulms
	122 bytes track header aka pre-gap
	9 * 628 bytes sectordata (sector header, data en closure gap)
	1080 bytes end-of-track gap

	This data comes from the TC8566AF manual
	each track in IBM format contains
	'4E' x 80, '00' x 12, 'C2' x 3
	'FC' x 1, '4E' x 50, sector data 1 to n
	'4E' x ?? (closing gap)
	each sector data contains
	'00' x 12, 'A1' x 3, 'FE' x 1,
	C,H,R,N,CRC(2bytes), '4E' x 22, '00' x 12,
	'A1' x 4,'FB'('F8') x 1, data(512 bytes),CRC(2bytes),'4E'(gap3)
	*/
	byte* tmppoint;
	tmppoint=readTrackDataBuf;
	// Fill track header
	for (int i=0 ; i<80 ; i++ ) *(tmppoint++)=0x4E;
	for (int i=0 ; i<12 ; i++ ) *(tmppoint++)=0x00;
	for (int i=0 ; i<3 ; i++ ) *(tmppoint++)=0xC2;
	*(tmppoint++)=0xFC;
	for (int i=0 ; i<50 ; i++ ) *(tmppoint++)=0x4E;
	// Fill sector
	for (byte j=0 ; j<9 ; j++){
		// Fill sector header
		for (int i=0 ; i<12 ; i++ ) *(tmppoint++)=0x00;
		for (int i=0 ; i<3 ; i++ ) *(tmppoint++)=0xA1;
		*(tmppoint++)=0xFE;
		*(tmppoint++)=track;	//C: Cylinder number
		*(tmppoint++)=side;	//H: Head Address
		*(tmppoint++)=(byte)(j+1); //R: Record
		*(tmppoint++)=0x02;	//N: Number (indicates length of sector)
		*(tmppoint++)=0x00; //CRC byte 1
		*(tmppoint++)=0x00; //CRC byte 2
		//read sector data
		read(track, j+1, side, 512, tmppoint);
		tmppoint+=512;
		//end of sector
		*(tmppoint++)=(byte)(j+1); //CRC byte 1
		*(tmppoint++)=(byte)(j+1); //CRC byte 2
		for (int i=0 ; i<55 ; i++ ) *(tmppoint++)=0x4E; // TODO: check this number
		//TODO build correct CRC and read sector +place gap
	};
	for (int i=0 ; i<1080 ; i++ ) *(tmppoint++)=0x4E ; //TODO look up value of this end-of-track gap


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

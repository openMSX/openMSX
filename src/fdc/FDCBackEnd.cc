// $Id$

#include "FDCBackEnd.hh"


FDCBackEnd::FDCBackEnd()
{
	nbSides = 0;
}

void FDCBackEnd::getTrackHeader(byte track, byte side, byte* buf)
{
	PRT_DEBUG("FDCBackEnd::getTrackHeader [unimplemented]");
}
void FDCBackEnd::getSectorHeader(byte track, byte sector, byte side, byte* buf)
{
	PRT_DEBUG("FDCBackEnd::getSectorHeader [unimplemented]");
}

void FDCBackEnd::initWriteTrack(byte track, byte side)
{
	PRT_DEBUG("FDCBackEnd::initWriteTrack [unimplemented]");
}

void FDCBackEnd::writeTrackData(byte data)
{
	PRT_DEBUG("FDCBackEnd::writeTrackData [unimplemented]");
}

void FDCBackEnd::initReadTrack(byte track, byte side)
{
	PRT_DEBUG("FDCBackEnd::initReadTrack [unimplemented]");
}

byte FDCBackEnd::readTrackData()
{
	PRT_DEBUG("FDCBackEnd::readTrackData [unimplemented]");
	return 0xF4;
}



void FDCBackEnd::readSector(byte* buf, int logSector)
{
	byte track, side, sector;
	logToPhys(logSector, track, side, sector);
	read(track, sector, side, 512, buf);
}

void FDCBackEnd::writeSector(const byte* buf, int logSector)
{
	byte track, side, sector;
	logToPhys(logSector, track, side, sector);
	write(track, sector, side, 512, buf);
}


int FDCBackEnd::physToLog(byte track, byte side, byte sector)
{
	if ((track == 0) && (side == 0) && (sector == 1))	// bootsector
		return 0;
	if (!nbSides) readBootSector();
	PRT_DEBUG("FDCBackEnd::physToLog(track "<<(int)track<<", side "<<(int)side <<", sector "<<(int)sector<<") returns "<< (int)(sectorsPerTrack * (side + nbSides * track) + (sector - 1)) );
	return sectorsPerTrack * (side + nbSides * track) + (sector - 1);
}

void FDCBackEnd::logToPhys(int log, byte &track, byte &side, byte &sector)
{
	if (!nbSides) readBootSector();
	track = log / (nbSides * sectorsPerTrack);
	side = (log / sectorsPerTrack) % nbSides;
	sector = (log % sectorsPerTrack) + 1;
}

void FDCBackEnd::readBootSector()
{
	byte buf[512];
	read(0, 1, 0, 512, buf);
	sectorsPerTrack = buf[0x18] + 256 * buf[0x19];
	nbSides         = buf[0x1A] + 256 * buf[0x1B];
	PRT_DEBUG("FDCBackEnd sectorsPerTrack " << sectorsPerTrack);
	PRT_DEBUG("FDCBackEnd nbSides         " << nbSides);
}


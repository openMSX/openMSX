// $Id$

#include "Disk.hh"


namespace openmsx {

Disk::Disk()
{
	nbSides = 0;
}

Disk::~Disk()
{
}

void Disk::getTrackHeader(byte /*track*/, byte /*side*/, byte* /*buf*/)
{
	PRT_DEBUG("Disk::getTrackHeader [unimplemented]");
}
void Disk::getSectorHeader(byte /*track*/, byte /*sector*/, byte /*side*/,
                           byte* /*buf*/)
{
	PRT_DEBUG("Disk::getSectorHeader [unimplemented]");
}

void Disk::initWriteTrack(byte /*track*/, byte /*side*/)
{
	PRT_DEBUG("Disk::initWriteTrack [unimplemented]");
}

void Disk::writeTrackData(byte /*data*/)
{
	PRT_DEBUG("Disk::writeTrackData [unimplemented]");
}

void Disk::initReadTrack(byte /*track*/, byte /*side*/)
{
	PRT_DEBUG("Disk::initReadTrack [unimplemented]");
}

byte Disk::readTrackData()
{
	PRT_DEBUG("Disk::readTrackData [unimplemented]");
	return 0xF4;
}



void Disk::readSector(byte* buf, int logSector)
{
	byte track, side, sector;
	logToPhys(logSector, track, side, sector);
	read(track, sector, side, 512, buf);
}

void Disk::writeSector(const byte* buf, int logSector)
{
	byte track, side, sector;
	logToPhys(logSector, track, side, sector);
	write(track, sector, side, 512, buf);
}


int Disk::physToLog(byte track, byte side, byte sector)
{
	if ((track == 0) && (side == 0) && (sector == 1)) {
		// bootsector
		return 0;
	}
	if (!nbSides) {
		readBootSector();
	}
	int result = sectorsPerTrack * (side + nbSides * track) + (sector - 1);
	//PRT_DEBUG("Disk::physToLog(track " << (int)track << ", side "
	//          << (int)side << ", sector " << (int)sector<< ") returns "
	//          << result);
	return result;
}

void Disk::logToPhys(int log, byte &track, byte &side, byte &sector)
{
	if (!nbSides) {
		// try to guess values from boot sector
		readBootSector();
	}
	track = log / (nbSides * sectorsPerTrack);
	side = (log / sectorsPerTrack) % nbSides;
	sector = (log % sectorsPerTrack) + 1;
}

void Disk::readBootSector()
{
	byte buf[512];
	read(0, 1, 0, 512, buf);
	sectorsPerTrack = buf[0x18] + 256 * buf[0x19];
	nbSides         = buf[0x1A] + 256 * buf[0x1B];
	//PRT_DEBUG("Disk sectorsPerTrack " << sectorsPerTrack);
	//PRT_DEBUG("Disk nbSides         " << nbSides);
}

} // namespace openmsx


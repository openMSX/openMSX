// $Id$

#include "FDCBackEnd.hh"


void FDCBackEnd::getTrackHeader(byte phystrack, byte track, byte side, byte* buf)
{
	PRT_DEBUG("FDCBackEnd::getTrackHeader [unimplemented]");
}
void FDCBackEnd::getSectorHeader(byte phystrack, byte track, byte sector, byte side, byte* buf)
{
	PRT_DEBUG("FDCBackEnd::getSectorHeader [unimplemented]");
}

void FDCBackEnd::readSector(byte* buf, int logSector)
{
	// TODO only works for double sided disks
	byte track  = logSector / 18;
	byte side   = (logSector%18) / 9;
	byte sector = (logSector%9) + 1;
	read(track, track, sector, side, 512, buf);
}

void FDCBackEnd::writeSector(const byte* buf, int logSector)
{
	// TODO only works for double sided disks
	byte track  = logSector / 18;
	byte side   = (logSector%18) / 9;
	byte sector = (logSector%9) + 1;
	write(track, track, sector, side, 512, buf);
}

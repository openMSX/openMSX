// $Id$

#include "FDCDummyBackEnd.hh"


void FDCDummyBackEnd::read(byte phystrack, byte track, byte sector,
                           byte side, int size, byte* buf)
{
	throw DriveEmptyException("No disk in drive");
}

void FDCDummyBackEnd::write(byte phystrack, byte track, byte sector,
                            byte side, int size, const byte* buf)
{
	throw DriveEmptyException("No disk in drive");
}

void FDCDummyBackEnd::getSectorHeader(byte phystrack, byte track,
                                      byte sector, byte side, byte* buf)
{
	throw DriveEmptyException("No disk in drive");
}

void FDCDummyBackEnd::getTrackHeader(byte phystrack, byte track,
                                     byte side, byte* buf)
{
	throw DriveEmptyException("No disk in drive");
}

bool FDCDummyBackEnd::ready()
{
	return false;
}

bool FDCDummyBackEnd::writeProtected()
{
	return true;	// TODO check
}

bool FDCDummyBackEnd::doubleSided()
{
	return false;	// TODO check
}

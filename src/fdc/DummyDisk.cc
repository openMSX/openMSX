// $Id$

#include "DummyDisk.hh"


void DummyDisk::read(byte track, byte sector,
                           byte side, int size, byte* buf)
{
	throw DriveEmptyException("No disk in drive");
}

void DummyDisk::write(byte track, byte sector,
                            byte side, int size, const byte* buf)
{
	throw DriveEmptyException("No disk in drive");
}

void DummyDisk::getSectorHeader(byte track, byte sector,
                                      byte side, byte* buf)
{
	throw DriveEmptyException("No disk in drive");
}

void DummyDisk::getTrackHeader(byte track,
                                     byte side, byte* buf)
{
	throw DriveEmptyException("No disk in drive");
}

bool DummyDisk::ready()
{
	return false;
}

bool DummyDisk::writeProtected()
{
	return true;	// TODO check
}

bool DummyDisk::doubleSided()
{
	return false;	// TODO check
}

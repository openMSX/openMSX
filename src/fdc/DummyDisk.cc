// $Id$

#include "DummyDisk.hh"

namespace openmsx {

DummyDisk::DummyDisk()
{
	nbSectors = 0;
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

void DummyDisk::readLogicalSector(unsigned /*sector*/, byte* /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

void DummyDisk::writeLogicalSector(unsigned /*sector*/, const byte* /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

} // namespace openmsx

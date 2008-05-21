// $Id$

#include "DummyDisk.hh"
#include "DiskExceptions.hh"

namespace openmsx {

DummyDisk::DummyDisk()
	: SectorBasedDisk("")
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

void DummyDisk::readSectorImpl(unsigned /*sector*/, byte* /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

void DummyDisk::writeSectorImpl(unsigned /*sector*/, const byte* /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

} // namespace openmsx

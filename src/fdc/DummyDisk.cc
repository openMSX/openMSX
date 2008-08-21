// $Id$

#include "DummyDisk.hh"
#include "DiskExceptions.hh"

namespace openmsx {

DummyDisk::DummyDisk()
	: SectorBasedDisk(Filename(""))
{
	setNbSectors(0);
}

bool DummyDisk::ready()
{
	return false;
}

bool DummyDisk::isWriteProtectedImpl() const
{
	return true;	// TODO check
}

void DummyDisk::readSectorSBD(unsigned /*sector*/, byte* /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

void DummyDisk::writeSectorSBD(unsigned /*sector*/, const byte* /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

} // namespace openmsx

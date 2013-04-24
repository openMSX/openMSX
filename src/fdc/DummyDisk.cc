#include "DummyDisk.hh"
#include "DiskExceptions.hh"

namespace openmsx {

DummyDisk::DummyDisk()
	: SectorBasedDisk(Filename(""))
{
	setNbSectors(0);
}

bool DummyDisk::isDummyDisk() const
{
	return true;
}

bool DummyDisk::isWriteProtectedImpl() const
{
	return true; // TODO check
}

void DummyDisk::readSectorImpl(size_t /*sector*/, byte* /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

void DummyDisk::writeSectorImpl(size_t /*sector*/, const byte* /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

} // namespace openmsx

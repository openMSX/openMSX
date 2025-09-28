#include "DummyDisk.hh"

#include "DiskExceptions.hh"

namespace openmsx {

DummyDisk::DummyDisk()
	: SectorBasedDisk(DiskName(Filename()))
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

void DummyDisk::readSectorImpl(size_t /*sector*/, SectorBuffer& /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

void DummyDisk::writeSectorImpl(size_t /*sector*/, const SectorBuffer& /*buf*/)
{
	throw DriveEmptyException("No disk in drive");
}

} // namespace openmsx

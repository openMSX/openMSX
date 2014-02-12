#include "DiskDrive.hh"
#include "DiskExceptions.hh"

namespace openmsx {

// class DiskDrive

DiskDrive::~DiskDrive()
{
}


// class DummyDrive

bool DummyDrive::isDiskInserted() const
{
	return false;
}

bool DummyDrive::isWriteProtected() const
{
	return true;
}

bool DummyDrive::isDoubleSided() const
{
	return false;
}

bool DummyDrive::isTrack00() const
{
	return false; // National_FS-5500F1 2nd drive detection depends on this
}

void DummyDrive::setSide(bool /*side*/)
{
	// ignore
}

void DummyDrive::step(bool /*direction*/, EmuTime::param /*time*/)
{
	// ignore
}

void DummyDrive::setMotor(bool /*status*/, EmuTime::param /*time*/)
{
	// ignore
}

bool DummyDrive::indexPulse(EmuTime::param /*time*/)
{
	return false;
}

EmuTime DummyDrive::getTimeTillIndexPulse(EmuTime::param /*time*/, int /*count*/)
{
	return EmuTime::infinity;
}

void DummyDrive::setHeadLoaded(bool /*status*/, EmuTime::param /*time*/)
{
	// ignore
}

bool DummyDrive::headLoaded(EmuTime::param /*time*/)
{
	return false;
}

void DummyDrive::writeTrack(const RawTrack& /*track*/)
{
	throw DriveEmptyException("No drive selected");
}

void DummyDrive::readTrack(RawTrack& /*track*/)
{
	throw DriveEmptyException("No drive selected");
}

EmuTime DummyDrive::getNextSector(EmuTime::param /*time*/, RawTrack& /*track*/,
                                  RawTrack::Sector& /*sector*/)
{
	return EmuTime::infinity;
}

bool DummyDrive::diskChanged()
{
	return false;
}

bool DummyDrive::peekDiskChanged() const
{
	return false;
}

bool DummyDrive::isDummyDrive() const
{
	return true;
}

} // namespace openmsx

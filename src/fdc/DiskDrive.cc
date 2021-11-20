#include "DiskDrive.hh"
#include "DiskExceptions.hh"

namespace openmsx {

// class DummyDrive

bool DummyDrive::isDiskInserted() const
{
	return false;
}

bool DummyDrive::isWriteProtected() const
{
	return true;
}

bool DummyDrive::isDoubleSided()
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

bool DummyDrive::getSide() const
{
	return false;
}

void DummyDrive::step(bool /*direction*/, EmuTime::param /*time*/)
{
	// ignore
}

void DummyDrive::setMotor(bool /*status*/, EmuTime::param /*time*/)
{
	// ignore
}

bool DummyDrive::getMotor() const
{
	return false;
}

bool DummyDrive::indexPulse(EmuTime::param /*time*/)
{
	return false;
}

EmuTime DummyDrive::getTimeTillIndexPulse(EmuTime::param /*time*/, int /*count*/)
{
	return EmuTime::infinity();
}

unsigned DummyDrive::getTrackLength()
{
	return RawTrack::STANDARD_SIZE;
}

void DummyDrive::writeTrackByte(int /*idx*/, byte /*val*/, bool /*addIdam*/)
{
	throw DriveEmptyException("No drive selected");
}

byte DummyDrive::readTrackByte(int /*idx*/)
{
	throw DriveEmptyException("No drive selected");
}

EmuTime DummyDrive::getNextSector(EmuTime::param /*time*/, RawTrack::Sector& /*sector*/)
{
	return EmuTime::infinity();
}

void DummyDrive::flushTrack()
{
	// ignore
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

void DummyDrive::applyWd2793ReadTrackQuirk()
{
	// nothing
}

void DummyDrive::invalidateWd2793ReadTrackQuirk()
{
	// nothing
}

} // namespace openmsx

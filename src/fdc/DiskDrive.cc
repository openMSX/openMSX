// $Id$

#include "DiskDrive.hh"
#include "DiskExceptions.hh"

namespace openmsx {

// class DiskDrive

DiskDrive::~DiskDrive()
{
}


// class DummyDrive

bool DummyDrive::isReady() const
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

void DummyDrive::step(bool /*direction*/, const EmuTime& /*time*/)
{
	// ignore
}

void DummyDrive::setMotor(bool /*status*/, const EmuTime& /*time*/)
{
	// ignore
}

bool DummyDrive::indexPulse(const EmuTime& /*time*/)
{
	return false;
}

int DummyDrive::indexPulseCount(const EmuTime& /*begin*/,
                                const EmuTime& /*end*/)
{
	return 0;
}

EmuTime DummyDrive::getTimeTillSector(byte /*sector*/, const EmuTime& time)
{
	return time;
}

EmuTime DummyDrive::getTimeTillIndexPulse(const EmuTime& time)
{
	return time;
}

void DummyDrive::setHeadLoaded(bool /*status*/, const EmuTime& /*time*/)
{
	// ignore
}

bool DummyDrive::headLoaded(const EmuTime& /*time*/)
{
	return false;
}

void DummyDrive::read(byte /*sector*/, byte* /*buf*/,
                      byte& /*onDiskTrack*/, byte& /*onDiskSector*/,
                      byte& /*onDiskSide*/,  int&  /*onDiskSize*/)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::write(byte /*sector*/, const byte* /*buf*/,
                       byte& /*onDiskTrack*/, byte& /*onDiskSector*/,
                       byte& /*onDiskSide*/,  int& /*onDiskSize*/)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::getSectorHeader(byte /*sector*/, byte* /*buf*/)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::getTrackHeader(byte* /*buf*/)
{
	throw DriveEmptyException("No drive connected");
}

void DummyDrive::writeTrackData(const byte* /*data*/)
{
	// ignore ???
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

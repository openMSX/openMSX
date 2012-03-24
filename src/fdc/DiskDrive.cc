// $Id$

#include "DiskDrive.hh"
#include "DiskExceptions.hh"
#include <cassert>

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

EmuTime DummyDrive::getTimeTillSector(byte /*sector*/, EmuTime::param time)
{
	return time;
}

EmuTime DummyDrive::getTimeTillIndexPulse(EmuTime::param time)
{
	return time;
}

void DummyDrive::setHeadLoaded(bool /*status*/, EmuTime::param /*time*/)
{
	// ignore
}

bool DummyDrive::headLoaded(EmuTime::param /*time*/)
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

void DummyDrive::writeTrack(const RawTrack& /*track*/)
{
	assert(false);
}

void DummyDrive::readTrack(RawTrack& /*track*/)
{
	assert(false);
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

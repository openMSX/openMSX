// $Id$

#include "DriveMultiplexer.hh"


DriveMultiplexer::DriveMultiplexer(DiskDrive* drive[4])
{
	selected = NO_DRIVE;
	this->drive[DRIVE_A]  = drive[0];
	this->drive[DRIVE_B]  = drive[1];
	this->drive[DRIVE_C]  = drive[2];
	this->drive[DRIVE_D]  = drive[3];
	this->drive[NO_DRIVE] = new DummyDrive();
}

DriveMultiplexer::~DriveMultiplexer()
{
	delete drive[NO_DRIVE];
}

void DriveMultiplexer::selectDrive(DriveNum num)
{
	selected = num;
}

bool DriveMultiplexer::ready()
{
	return drive[selected]->ready();
}

bool DriveMultiplexer::writeProtected()
{
	return drive[selected]->writeProtected();
}

bool DriveMultiplexer::doubleSided()
{
	return drive[selected]->doubleSided();
}

void DriveMultiplexer::setSide(bool side)
{
	drive[selected]->setSide(side);
}

void DriveMultiplexer::step(bool direction, const EmuTime &time)
{
	drive[selected]->step(direction, time);
}

bool DriveMultiplexer::track00(const EmuTime &time)
{
	return drive[selected]->track00(time);
}

void DriveMultiplexer::setMotor(bool status, const EmuTime &time)
{
	drive[selected]->setMotor(status, time);
}

bool DriveMultiplexer::indexPulse(const EmuTime &time)
{
	return drive[selected]->indexPulse(time);
}

int DriveMultiplexer::indexPulseCount(const EmuTime &begin,
                                     const EmuTime &end)
{
	return drive[selected]->indexPulseCount(begin, end);
}

void DriveMultiplexer::setHeadLoaded(bool status, const EmuTime &time)
{
	drive[selected]->setHeadLoaded(status, time);
}

bool DriveMultiplexer::headLoaded(const EmuTime &time)
{
	return drive[selected]->headLoaded(time);
}

void DriveMultiplexer::read(byte sector, int size, byte* buf)
{
	drive[selected]->read(sector, size, buf);
}

void DriveMultiplexer::write(byte sector, int size, const byte* buf)
{
	drive[selected]->write(sector, size, buf);
}

void DriveMultiplexer::getSectorHeader(byte sector, byte* buf)
{
	drive[selected]->getSectorHeader(sector, buf);
}

void DriveMultiplexer::getTrackHeader(byte track, byte* buf)
{
	drive[selected]->getTrackHeader(track, buf);
}

void DriveMultiplexer::initWriteTrack(byte track)
{
	drive[selected]->initWriteTrack(track);
}

void DriveMultiplexer::writeTrackData(byte data)
{
	drive[selected]->writeTrackData(data);
}

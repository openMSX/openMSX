// $Id$

#include "DriveMultiplexer.hh"


namespace openmsx {

DriveMultiplexer::DriveMultiplexer(DiskDrive* drv[4])
{
	motor = false;
	side = false;
	selected = NO_DRIVE;
	drive[DRIVE_A]  = drv[0];
	drive[DRIVE_B]  = drv[1];
	drive[DRIVE_C]  = drv[2];
	drive[DRIVE_D]  = drv[3];
	drive[NO_DRIVE] = new DummyDrive();
}

DriveMultiplexer::~DriveMultiplexer()
{
	delete drive[NO_DRIVE];
}

void DriveMultiplexer::selectDrive(DriveNum num, const EmuTime &time)
{
	if (selected != num) {
		drive[selected]->setMotor(false, time);
		selected = num;
		drive[selected]->setSide(side);
		drive[selected]->setMotor(motor, time);
	}
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

void DriveMultiplexer::setSide(bool side_)
{
	side = side_;
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
	motor = status;
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

void DriveMultiplexer::read(byte sector, byte* buf,
                            byte &onDiskTrack, byte &onDiskSector,
                            byte &onDiskSide,  int  &onDiskSize)
{
	drive[selected]->read(sector, buf, onDiskTrack,
	                      onDiskSector, onDiskSide, onDiskSize);
}

void DriveMultiplexer::write(byte sector, const byte* buf,
                             byte &onDiskTrack, byte &onDiskSector,
                             byte &onDiskSide,  int  &onDiskSize)
{
	drive[selected]->write(sector, buf, onDiskTrack,
	                       onDiskSector, onDiskSide, onDiskSize);
}

void DriveMultiplexer::getSectorHeader(byte sector, byte* buf)
{
	drive[selected]->getSectorHeader(sector, buf);
}

void DriveMultiplexer::getTrackHeader(byte* buf)
{
	drive[selected]->getTrackHeader(buf);
}

void DriveMultiplexer::initWriteTrack()
{
	drive[selected]->initWriteTrack();
}

void DriveMultiplexer::writeTrackData(byte data)
{
	drive[selected]->writeTrackData(data);
}

} // namespace openmsx

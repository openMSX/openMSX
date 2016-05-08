#include "DriveMultiplexer.hh"
#include "serialize.hh"

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
	drive[NO_DRIVE] = &dummyDrive;
}

void DriveMultiplexer::selectDrive(DriveNum num, EmuTime::param time)
{
	if (selected != num) {
		drive[selected]->setMotor(false, time);
		selected = num;
		drive[selected]->setSide(side);
		drive[selected]->setMotor(motor, time);
	}
}

bool DriveMultiplexer::isDiskInserted() const
{
	return drive[selected]->isDiskInserted();
}

bool DriveMultiplexer::isWriteProtected() const
{
	return drive[selected]->isWriteProtected();
}

bool DriveMultiplexer::isDoubleSided() const
{
	return drive[selected]->isDoubleSided();
}

void DriveMultiplexer::setSide(bool side_)
{
	side = side_;
	drive[selected]->setSide(side);
}

void DriveMultiplexer::step(bool direction, EmuTime::param time)
{
	drive[selected]->step(direction, time);
}

bool DriveMultiplexer::isTrack00() const
{
	return drive[selected]->isTrack00();
}

void DriveMultiplexer::setMotor(bool status, EmuTime::param time)
{
	motor = status;
	drive[selected]->setMotor(status, time);
}

bool DriveMultiplexer::indexPulse(EmuTime::param time)
{
	return drive[selected]->indexPulse(time);
}

EmuTime DriveMultiplexer::getTimeTillIndexPulse(EmuTime::param time, int count)
{
	return drive[selected]->getTimeTillIndexPulse(time, count);
}

void DriveMultiplexer::setHeadLoaded(bool status, EmuTime::param time)
{
	drive[selected]->setHeadLoaded(status, time);
}

bool DriveMultiplexer::headLoaded(EmuTime::param time)
{
	return drive[selected]->headLoaded(time);
}

void DriveMultiplexer::writeTrack(const RawTrack& track)
{
	drive[selected]->writeTrack(track);
}

void DriveMultiplexer::readTrack(RawTrack& track)
{
	drive[selected]->readTrack(track);
}

EmuTime DriveMultiplexer::getNextSector(EmuTime::param time, RawTrack& track,
                                        RawTrack::Sector& sector)
{
	return drive[selected]->getNextSector(time, track, sector);
}

bool DriveMultiplexer::diskChanged()
{
	return drive[selected]->diskChanged();
}

bool DriveMultiplexer::peekDiskChanged() const
{
	return drive[selected]->peekDiskChanged();
}

bool DriveMultiplexer::isDummyDrive() const
{
	return drive[selected]->isDummyDrive();
}


static std::initializer_list<enum_string<DriveMultiplexer::DriveNum>> driveNumInfo = {
	{ "A",    DriveMultiplexer::DRIVE_A },
	{ "B",    DriveMultiplexer::DRIVE_B },
	{ "C",    DriveMultiplexer::DRIVE_C },
	{ "D",    DriveMultiplexer::DRIVE_D },
	{ "none", DriveMultiplexer::NO_DRIVE }
};
SERIALIZE_ENUM(DriveMultiplexer::DriveNum, driveNumInfo);

template<typename Archive>
void DriveMultiplexer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("selected", selected);
	ar.serialize("motor", motor);
	ar.serialize("side", side);
}
INSTANTIATE_SERIALIZE_METHODS(DriveMultiplexer);

} // namespace openmsx

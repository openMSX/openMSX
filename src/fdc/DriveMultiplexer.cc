#include "DriveMultiplexer.hh"

#include "serialize.hh"

namespace openmsx {

DriveMultiplexer::DriveMultiplexer(std::span<std::unique_ptr<DiskDrive>, 4> drv)
{
	drive[Drive::A] = drv[0].get();
	drive[Drive::B] = drv[1].get();
	drive[Drive::C] = drv[2].get();
	drive[Drive::D] = drv[3].get();
	drive[Drive::NONE] = &dummyDrive;
}

void DriveMultiplexer::selectDrive(Drive num, EmuTime time)
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

bool DriveMultiplexer::isDiskInserted(Drive num) const
{
	return drive[num]->isDiskInserted();
}

bool DriveMultiplexer::isWriteProtected() const
{
	return drive[selected]->isWriteProtected();
}

bool DriveMultiplexer::isDoubleSided()
{
	return drive[selected]->isDoubleSided();
}

void DriveMultiplexer::setSide(bool side_)
{
	side = side_;
	drive[selected]->setSide(side);
}

bool DriveMultiplexer::getSide() const
{
	return side;
}

void DriveMultiplexer::step(bool direction, EmuTime time)
{
	drive[selected]->step(direction, time);
}

bool DriveMultiplexer::isTrack00() const
{
	return drive[selected]->isTrack00();
}

void DriveMultiplexer::setMotor(bool status, EmuTime time)
{
	motor = status;
	drive[selected]->setMotor(status, time);
}

bool DriveMultiplexer::getMotor() const
{
	return motor;
}

bool DriveMultiplexer::indexPulse(EmuTime time)
{
	return drive[selected]->indexPulse(time);
}

EmuTime DriveMultiplexer::getTimeTillIndexPulse(EmuTime time, int count)
{
	return drive[selected]->getTimeTillIndexPulse(time, count);
}

unsigned DriveMultiplexer::getTrackLength()
{
	return drive[selected]->getTrackLength();
}

void DriveMultiplexer::writeTrackByte(int idx, uint8_t val, bool addIdam)
{
	drive[selected]->writeTrackByte(idx, val, addIdam);
}

uint8_t DriveMultiplexer::readTrackByte(int idx)
{
	return drive[selected]->readTrackByte(idx);
}

EmuTime DriveMultiplexer::getNextSector(EmuTime time, RawTrack::Sector& sector)
{
	return drive[selected]->getNextSector(time, sector);
}

void DriveMultiplexer::flushTrack()
{
	drive[selected]->flushTrack();
}

bool DriveMultiplexer::diskChanged()
{
	return drive[selected]->diskChanged();
}

bool DriveMultiplexer::diskChanged(Drive num)
{
	return drive[num]->diskChanged();
}

bool DriveMultiplexer::peekDiskChanged() const
{
	return drive[selected]->peekDiskChanged();
}

bool DriveMultiplexer::peekDiskChanged(Drive num) const
{
	return drive[num]->peekDiskChanged();
}

bool DriveMultiplexer::isDummyDrive() const
{
	return drive[selected]->isDummyDrive();
}

void DriveMultiplexer::applyWd2793ReadTrackQuirk()
{
	drive[selected]->applyWd2793ReadTrackQuirk();
}

void DriveMultiplexer::invalidateWd2793ReadTrackQuirk()
{
	drive[selected]->invalidateWd2793ReadTrackQuirk();
}


static constexpr auto driveNumInfo = std::to_array<enum_string<DriveMultiplexer::Drive>>({
	{ "A",    DriveMultiplexer::Drive::A },
	{ "B",    DriveMultiplexer::Drive::B },
	{ "C",    DriveMultiplexer::Drive::C },
	{ "D",    DriveMultiplexer::Drive::D },
	{ "none", DriveMultiplexer::Drive::NONE },
});
SERIALIZE_ENUM(DriveMultiplexer::Drive, driveNumInfo);

template<typename Archive>
void DriveMultiplexer::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("selected", selected,
	             "motor",    motor,
	             "side",     side);
}
INSTANTIATE_SERIALIZE_METHODS(DriveMultiplexer);

} // namespace openmsx

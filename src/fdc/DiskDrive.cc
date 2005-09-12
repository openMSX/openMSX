// $Id$

#include "DiskDrive.hh"
#include "Disk.hh"
#include "DiskChanger.hh"
#include "EventDistributor.hh"
#include "LedEvent.hh"
#include "FileManipulator.hh"
#include "CommandController.hh"
#include <bitset>

using std::string;
using std::vector;

namespace openmsx {

/// class DiskDrive ///

DiskDrive::~DiskDrive()
{
}

/// class DummyDrive ///

DummyDrive::DummyDrive()
{
}

DummyDrive::~DummyDrive()
{
}

bool DummyDrive::ready()
{
	return false;
}

bool DummyDrive::writeProtected()
{
	return true;
}

bool DummyDrive::doubleSided()
{
	return false;
}

void DummyDrive::setSide(bool /*side*/)
{
	// ignore
}

void DummyDrive::step(bool /*direction*/, const EmuTime& /*time*/)
{
	// ignore
}

bool DummyDrive::track00(const EmuTime& /*time*/)
{
	return false; // National_FS-5500F1 2nd drive detection depends on this
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

void DummyDrive::initWriteTrack()
{
	// ignore ???
}

void DummyDrive::writeTrackData(byte /*data*/)
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

bool DummyDrive::dummyDrive()
{
	return true;
}


/// class RealDrive ///

static std::bitset<RealDrive::MAX_DRIVES> drivesInUse;

RealDrive::RealDrive(CommandController& commandController,
                     EventDistributor& eventDistributor_,
                     FileManipulator& fileManipulator, const EmuTime& time)
	: headPos(0), motorStatus(false), motorTimer(time)
	, headLoadStatus(false), headLoadTimer(time)
	, eventDistributor(eventDistributor_)
{
	int i = 0;
	while (drivesInUse[i]) {
		if (++i == MAX_DRIVES) {
			throw FatalError("Too many disk drives.");
		}
	}
	drivesInUse[i] = true;
	string driveName = string("disk") + static_cast<char>('a' + i);

	if (commandController.hasCommand(driveName)) {
		throw FatalError("Duplicated drive name: " + driveName);
	}
	changer.reset(new DiskChanger(driveName, commandController,
		                      fileManipulator));
}

RealDrive::~RealDrive()
{
	int driveNum = changer->getDriveName()[4] - 'a';
	drivesInUse[driveNum] = false;
}

bool RealDrive::ready()
{
	return changer->getDisk().ready();
}

bool RealDrive::writeProtected()
{
	// write protected bit is always 0 when motor is off
	// verified on NMS8280
	return motorStatus && changer->getDisk().writeProtected();
}

void RealDrive::step(bool direction, const EmuTime& /*time*/)
{
	if (direction) {
		// step in
		if (headPos < MAX_TRACK) {
			headPos++;
		}
	} else {
		// step out
		if (headPos > 0) {
			headPos--;
		}
	}
	PRT_DEBUG("DiskDrive track " << headPos);
}

bool RealDrive::track00(const EmuTime& /*time*/)
{
	// track00 bit is always 0 when motor is off
	// verified on NMS8280
	return motorStatus && (headPos == 0);
}

void RealDrive::setMotor(bool status, const EmuTime& time)
{
	if (motorStatus != status) {
		motorStatus = status;
		motorTimer.advance(time);
		/* The following is a hack to emulate the drive LED behaviour.
		 * This is in real life dependent on the FDC and should be
		 * investigated in detail to implement it properly... TODO */
		eventDistributor.distributeEvent(
			new LedEvent(LedEvent::FDD, motorStatus));
	}
}

bool RealDrive::indexPulse(const EmuTime& time)
{
	if (!motorStatus && changer->getDisk().ready()) {
		return false;
	}
	int angle = motorTimer.getTicksTill(time) % TICKS_PER_ROTATION;
	return angle < INDEX_DURATION;
}

int RealDrive::indexPulseCount(const EmuTime& begin,
                               const EmuTime& end)
{
	if (!motorStatus && changer->getDisk().ready()) {
		return 0;
	}
	int t1 = motorTimer.before(begin) ? motorTimer.getTicksTill(begin) : 0;
	int t2 = motorTimer.before(end)   ? motorTimer.getTicksTill(end)   : 0;
	return (t2 / TICKS_PER_ROTATION) - (t1 / TICKS_PER_ROTATION);
}

EmuTime RealDrive::getTimeTillSector(byte sector, const EmuTime& time)
{
	if (!motorStatus || !changer->getDisk().ready()) { // TODO is this correct?
		return time;
	}
	// TODO this really belongs in the Disk class
	int sectorAngle = ((sector - 1) * (TICKS_PER_ROTATION / 9)) %
	                  TICKS_PER_ROTATION;

	int angle = motorTimer.getTicksTill(time) % TICKS_PER_ROTATION;
	int delta = sectorAngle - angle;
	if (delta < 0) delta += TICKS_PER_ROTATION;
	assert((0 <= delta) && (delta < TICKS_PER_ROTATION));
	//std::cout << "DEBUG a1: " << angle
	//          << " a2: " << sectorAngle
	//          << " delta: " << delta << std::endl;

	EmuDuration dur = Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND>::
	                      duration(delta);
	return time + dur;
}

EmuTime RealDrive::getTimeTillIndexPulse(const EmuTime& time)
{
	if (!motorStatus || !changer->getDisk().ready()) { // TODO is this correct?
		return time;
	}
	int delta = TICKS_PER_ROTATION -
	            (motorTimer.getTicksTill(time) % TICKS_PER_ROTATION);
	EmuDuration dur = Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND>::
	                      duration(delta);
	return time + dur;
}

void RealDrive::setHeadLoaded(bool status, const EmuTime& time)
{
	if (headLoadStatus != status) {
		headLoadStatus = status;
		headLoadTimer.advance(time);
	}
}

bool RealDrive::headLoaded(const EmuTime& time)
{
	return headLoadStatus &&
	       (headLoadTimer.getTicksTill(time) > 10);
}

bool RealDrive::diskChanged()
{
	return changer->diskChanged();
}

bool RealDrive::peekDiskChanged() const
{
	return changer->peekDiskChanged();
}

bool RealDrive::dummyDrive()
{
	return false;
}


/// class SingleSidedDrive ///

SingleSidedDrive::SingleSidedDrive(
		CommandController& commandController,
		EventDistributor& eventDistributor,
		FileManipulator& fileManipulator,
		const EmuTime& time)
	: RealDrive(commandController, eventDistributor, fileManipulator, time)
{
}

SingleSidedDrive::~SingleSidedDrive()
{
}

bool SingleSidedDrive::doubleSided()
{
	return false;
}

void SingleSidedDrive::setSide(bool /*side*/)
{
	// ignore
}

void SingleSidedDrive::read(byte sector, byte* buf,
                            byte& onDiskTrack, byte& onDiskSector,
                            byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = 0;
	onDiskSize = 512;
	changer->getDisk().read(headPos, sector, 0, 512, buf);
}

void SingleSidedDrive::write(byte sector, const byte* buf,
                             byte& onDiskTrack, byte& onDiskSector,
                             byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = 0;
	onDiskSize = 512;
	changer->getDisk().write(headPos, sector, 0, 512, buf);
}

void SingleSidedDrive::getSectorHeader(byte sector, byte* buf)
{
	changer->getDisk().getSectorHeader(headPos, sector, 0, buf);
}

void SingleSidedDrive::getTrackHeader(byte* buf)
{
	changer->getDisk().getTrackHeader(headPos, 0, buf);
}

void SingleSidedDrive::initWriteTrack()
{
	changer->getDisk().initWriteTrack(headPos, 0);
}

void SingleSidedDrive::writeTrackData(byte data)
{
	changer->getDisk().writeTrackData(data);
}


/// class DoubleSidedDrive ///

DoubleSidedDrive::DoubleSidedDrive(
		CommandController& commandController,
		EventDistributor& eventDistributor,
		FileManipulator& fileManipulator,
		const EmuTime& time)
	: RealDrive(commandController, eventDistributor, fileManipulator, time)
{
	side = 0;
}

DoubleSidedDrive::~DoubleSidedDrive()
{
}

bool DoubleSidedDrive::doubleSided()
{
	return changer->getDisk().doubleSided();
}

void DoubleSidedDrive::setSide(bool side_)
{
	side = side_ ? 1 : 0;
}

void DoubleSidedDrive::read(byte sector, byte* buf,
                            byte& onDiskTrack, byte& onDiskSector,
                            byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = side;
	onDiskSize = 512;
	changer->getDisk().read(headPos, sector, side, 512, buf);
}

void DoubleSidedDrive::write(byte sector, const byte* buf,
                             byte& onDiskTrack, byte& onDiskSector,
                             byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = side;
	onDiskSize = 512;
	changer->getDisk().write(headPos, sector, side, 512, buf);
}

void DoubleSidedDrive::getSectorHeader(byte sector, byte* buf)
{
	changer->getDisk().getSectorHeader(headPos, sector, side, buf);
}

void DoubleSidedDrive::getTrackHeader(byte* buf)
{
	changer->getDisk().getTrackHeader(headPos, side, buf);
}

void DoubleSidedDrive::initWriteTrack()
{
	changer->getDisk().initWriteTrack(headPos, side);
}

void DoubleSidedDrive::writeTrackData(byte data)
{
	changer->getDisk().writeTrackData(data);
}

} // namespace openmsx

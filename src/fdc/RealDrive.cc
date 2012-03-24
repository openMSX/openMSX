// $Id$

#include "RealDrive.hh"
#include "Disk.hh"
#include "DiskChanger.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "LedStatus.hh"
#include "CommandController.hh"
#include "ThrottleManager.hh"
#include "CliComm.hh"
#include "GlobalSettings.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include <bitset>

using std::string;
using std::vector;

namespace openmsx {

enum {
	// Turn off 'loading indicator' (even if the MSX program keeps the
	// drive motor turning)
	LOADING_TIMEOUT = 0, // must be zero for backwards compatibility
	// Delayed motor off
	MOTOR_TIMEOUT,
};

static const unsigned MAX_DRIVES = 26; // a-z
typedef std::bitset<MAX_DRIVES> DrivesInUse;

RealDrive::RealDrive(MSXMotherBoard& motherBoard_, EmuDuration::param motorTimeout_,
                     bool doubleSided)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, loadingIndicator(new LoadingIndicator(
		motherBoard.getReactor().getGlobalSettings().getThrottleManager()))
	, motorTimeout(motorTimeout_)
	, motorTimer(getCurrentTime())
	, headLoadTimer(getCurrentTime())
	, headPos(0), side(0), motorStatus(false), headLoadStatus(false)
	, doubleSizedDrive(doubleSided)
{
	MSXMotherBoard::SharedStuff& info =
		motherBoard.getSharedStuff("drivesInUse");
	if (info.counter == 0) {
		assert(info.stuff == NULL);
		info.stuff = new DrivesInUse();
	}
	++info.counter;
	DrivesInUse& drivesInUse = *reinterpret_cast<DrivesInUse*>(info.stuff);

	unsigned i = 0;
	while (drivesInUse[i]) {
		if (++i == MAX_DRIVES) {
			throw MSXException("Too many disk drives.");
		}
	}
	drivesInUse[i] = true;
	string driveName = "diskX"; driveName[4] = char('a' + i);

	if (motherBoard.getCommandController().hasCommand(driveName)) {
		throw MSXException("Duplicated drive name: " + driveName);
	}
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, driveName, "add");
	changer.reset(new DiskChanger(motherBoard, driveName, true));
}

RealDrive::~RealDrive()
{
	doSetMotor(false, getCurrentTime()); // to send LED event

	MSXMotherBoard::SharedStuff& info =
		motherBoard.getSharedStuff("drivesInUse");
	assert(info.counter);
	assert(info.stuff);
	DrivesInUse& drivesInUse = *reinterpret_cast<DrivesInUse*>(info.stuff);

	const string& driveName = changer->getDriveName();
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, driveName, "remove");
	unsigned driveNum = driveName[4] - 'a';
	assert(drivesInUse[driveNum]);
	drivesInUse[driveNum] = false;

	--info.counter;
	if (info.counter == 0) {
		assert(drivesInUse.none());
		delete &drivesInUse;
		info.stuff = NULL;
	}
}

bool RealDrive::isDiskInserted() const
{
	return !changer->getDisk().isDummyDisk();
}

bool RealDrive::isWriteProtected() const
{
	// write protected bit is always 0 when motor is off
	// verified on NMS8280
	return motorStatus && changer->getDisk().isWriteProtected();
}

bool RealDrive::isDoubleSided() const
{
	return doubleSizedDrive ? changer->getDisk().isDoubleSided()
	                        : false;
}

void RealDrive::setSide(bool side_)
{
	if (doubleSizedDrive) {
		side = side_ ? 1 : 0;
	} else {
		assert(side == 0);
	}
}

void RealDrive::step(bool direction, EmuTime::param time)
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
	// ThrottleManager heuristic:
	//  If the motor is turning and there is head movement, assume the
	//  MSX program is (still) loading/saving to disk
	if (motorStatus) setLoading(time);
}

bool RealDrive::isTrack00() const
{
	// track00 bit is always 0 when motor is off
	// verified on NMS8280
	return motorStatus && (headPos == 0);
}

void RealDrive::setMotor(bool status, EmuTime::param time)
{
	// If status = true, motor is immediately turned on. If status = false,
	// the motor is only turned off after some (configurable) amount of
	// time (can be zero). Let's call the last passed status parameter the
	// 'logical' motor status.
	//
	// Loading indicator heuristic:
	// Loading indicator only reacts to _changes_ in the _logical_ motor
	// status. So when the motor is turned off, we immediately assume the
	// MSX program is done loading (or saving) (we don't wait for the motor
	// timeout). Turning the motor on when it already was logically on has
	// no effect. But turning it back on while it was logically off but
	// still in the motor-off-timeout phase does reset the loading
	// indicator.
	//
	if (status) {
		// (Try to) remove scheduled action to turn motor off.
		if (removeSyncPoint(MOTOR_TIMEOUT)) {
			// If there actually was such an action scheduled, we
			// need to turn on the loading indicator.
			assert(motorStatus);
			setLoading(time);
			return;
		}
		if (motorStatus) {
			// Motor was still turning, we're done.
			// Note: no effect on loading indicator.
			return;
		}
		// Actually turn motor on (it was off before).
		doSetMotor(true, time);
		setLoading(time);
	} else {
		if (!motorStatus) {
			// Motor was already off, we're done.
			return;
		}
		if (pendingSyncPoint(MOTOR_TIMEOUT)) {
			// We had already scheduled an action to turn the motor
			// off, we're done.
			return;
		}
		// Heuristic:
		//  Immediately react to 'logical' motor status, even if the
		//  motor will (possibly) still keep rotating for a few
		//  seconds.
		removeSyncPoint(LOADING_TIMEOUT);
		loadingIndicator->update(false);

		// Turn the motor off after some timeout (timeout could be 0)
		setSyncPoint(time + motorTimeout, MOTOR_TIMEOUT);
	}
}

void RealDrive::doSetMotor(bool status, EmuTime::param time)
{
	motorStatus = status;
	motorTimer.advance(time);

	// TODO The following is a hack to emulate the drive LED behaviour.
	//      This should be moved to the FDC mapping code.
	// TODO Each drive should get it's own independent LED.
	motherBoard.getLedStatus().setLed(LedStatus::FDD, status);
}

void RealDrive::setLoading(EmuTime::param time)
{
	assert(motorStatus);
	loadingIndicator->update(true);

	// ThrottleManager heuristic:
	//  We want to avoid getting stuck in 'loading state' when the MSX
	//  program forgets to turn off the motor.
	removeSyncPoint(LOADING_TIMEOUT);
	setSyncPoint(time + EmuDuration::sec(1), LOADING_TIMEOUT);
}

void RealDrive::executeUntil(EmuTime::param time, int userData)
{
	if (userData == LOADING_TIMEOUT) {
		loadingIndicator->update(false);
	} else {
		assert(userData == MOTOR_TIMEOUT);
		doSetMotor(false, time);
	}
}

bool RealDrive::indexPulse(EmuTime::param time)
{
	// Tested on real NMS8250:
	//  Only when there's a disk inserted and when the motor is spinning
	//  there are index pulses generated.
	if (!(motorStatus && isDiskInserted())) {
		return false;
	}
	int angle = motorTimer.getTicksTill(time) % TICKS_PER_ROTATION;
	return angle < INDEX_DURATION;
}

EmuTime RealDrive::getTimeTillSector(byte sector, EmuTime::param time)
{
	if (!motorStatus || !isDiskInserted()) { // TODO is this correct?
		return time;
	}
	// TODO this really belongs in the Disk class
	int delta;
	if (sector == 0) {
		// there is no sector 0 on normal disks, but it triggers with
		// the following command:
		//    openmsx -machine Sony_HB-F700D -ext msxdos2 -diska dos2.dsk
		// we'll just search for a complete rotation
		delta = TICKS_PER_ROTATION - 1;
	} else {
		int sectorAngle = ((sector - 1) * (TICKS_PER_ROTATION / 9)) %
				  TICKS_PER_ROTATION;

		int angle = motorTimer.getTicksTill(time) % TICKS_PER_ROTATION;
		delta = sectorAngle - angle;
		if (delta < 0) delta += TICKS_PER_ROTATION;
	}
	assert((0 <= delta) && (delta < TICKS_PER_ROTATION));

	EmuDuration dur = Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND>::
	                      duration(delta);
	return time + dur;
}

EmuTime RealDrive::getTimeTillIndexPulse(EmuTime::param time)
{
	if (!motorStatus || !isDiskInserted()) { // TODO is this correct?
		return time;
	}
	int delta = TICKS_PER_ROTATION -
	            (motorTimer.getTicksTill(time) % TICKS_PER_ROTATION);
	EmuDuration dur = MotorClock::duration(delta);
	return time + dur;
}

void RealDrive::setHeadLoaded(bool status, EmuTime::param time)
{
	if (headLoadStatus != status) {
		headLoadStatus = status;
		headLoadTimer.advance(time);
	}
}

bool RealDrive::headLoaded(EmuTime::param time)
{
	return headLoadStatus &&
	       (headLoadTimer.getTicksTill(time) > 10);
}

void RealDrive::read(byte sector, byte* buf,
                     byte& onDiskTrack, byte& onDiskSector,
                     byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = side;
	onDiskSize = 512;
	changer->getDisk().read(headPos, sector, side, 512, buf);
}

void RealDrive::write(byte sector, const byte* buf,
                      byte& onDiskTrack, byte& onDiskSector,
                      byte& onDiskSide,  int&  onDiskSize)
{
	onDiskTrack = headPos;
	onDiskSector = sector;
	onDiskSide = side;
	onDiskSize = 512;
	changer->getDisk().write(headPos, sector, side, 512, buf);
}

void RealDrive::getSectorHeader(byte sector, byte* buf)
{
	changer->getDisk().getSectorHeader(headPos, sector, side, buf);
}

void RealDrive::getTrackHeader(byte* buf)
{
	changer->getDisk().getTrackHeader(headPos, side, buf);
}

void RealDrive::writeTrackData(const byte* data)
{
	changer->getDisk().writeTrackData(headPos, side, data);
}

void RealDrive::writeTrack(const RawTrack& track)
{
	changer->getDisk().writeTrack(headPos, side, track);
}

void RealDrive::readTrack(RawTrack& track)
{
	changer->getDisk().readTrack(headPos, side, track);
}

EmuTime RealDrive::getNextSector(
	EmuTime::param time, RawTrack& track, RawTrack::Sector& sector)
{
	int idx = motorTimer.getTicksTill(time) % TICKS_PER_ROTATION;
	changer->getDisk().readTrack(headPos, side, track);
	if (!track.decodeNextSector(idx, sector)) {
		return EmuTime::infinity;
	}
	int ticks = sector.addrIdx - idx;
	if (ticks < 0) ticks += TICKS_PER_ROTATION;
	assert(0 <= ticks); assert(ticks < TICKS_PER_ROTATION);
	return time + MotorClock::duration(ticks);
}

bool RealDrive::diskChanged()
{
	return changer->diskChanged();
}

bool RealDrive::peekDiskChanged() const
{
	return changer->peekDiskChanged();
}

bool RealDrive::isDummyDrive() const
{
	return false;
}

// version 1: initial version
// version 2: removed 'timeOut', added MOTOR_TIMEOUT schedulable
template<typename Archive>
void RealDrive::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<Schedulable>(*this);
	ar.serialize("motorTimer", motorTimer);
	ar.serialize("headLoadTimer", headLoadTimer);
	ar.serialize("changer", *changer);
	ar.serialize("headPos", headPos);
	ar.serialize("side", side);
	ar.serialize("motorStatus", motorStatus);
	ar.serialize("headLoadStatus", headLoadStatus);
	if (ar.isLoader()) {
		// Right after a loadstate, the 'loading indicator' state may
		// be wrong, but that's OK. It's anyway only a heuristic and
		// it will be correct after at most one second.

		// This is a workaround for the fact that we can have multiple drives
		// (and only one is on), in which case the 2nd drive will turn off the
		// LED again which the first drive just turned on. TODO: fix by modelling
		// individual drive LEDs properly. See also
		// http://sourceforge.net/tracker/index.php?func=detail&aid=1540929&group_id=38274&atid=421864
		if (motorStatus) {
			motherBoard.getLedStatus().setLed(LedStatus::FDD, true);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(RealDrive);

} // namespace openmsx

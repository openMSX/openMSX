#include "RealDrive.hh"
#include "Disk.hh"
#include "DiskChanger.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "LedStatus.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "GlobalSettings.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "memory.hh"

using std::string;
using std::vector;

namespace openmsx {

RealDrive::RealDrive(MSXMotherBoard& motherBoard_, EmuDuration::param motorTimeout_,
                     bool signalsNeedMotorOn_, bool doubleSided)
	: syncLoadingTimeout(motherBoard_.getScheduler())
	, syncMotorTimeout  (motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, loadingIndicator(
		motherBoard.getReactor().getGlobalSettings().getThrottleManager())
	, motorTimeout(motorTimeout_)
	, motorTimer(getCurrentTime())
	, headLoadTimer(getCurrentTime())
	, headPos(0), side(0), startAngle(0)
	, motorStatus(false), headLoadStatus(false)
	, doubleSizedDrive(doubleSided)
	, signalsNeedMotorOn(signalsNeedMotorOn_)
{
	drivesInUse = motherBoard.getSharedStuff<DrivesInUse>("drivesInUse");

	unsigned i = 0;
	while ((*drivesInUse)[i]) {
		if (++i == MAX_DRIVES) {
			throw MSXException("Too many disk drives.");
		}
	}
	(*drivesInUse)[i] = true;
	string driveName = "diskX"; driveName[4] = char('a' + i);

	if (motherBoard.getCommandController().hasCommand(driveName)) {
		throw MSXException("Duplicated drive name: " + driveName);
	}
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, driveName, "add");
	changer = make_unique<DiskChanger>(motherBoard, driveName, true, doubleSizedDrive);
}

RealDrive::~RealDrive()
{
	doSetMotor(false, getCurrentTime()); // to send LED event

	const auto& driveName = changer->getDriveName();
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, driveName, "remove");

	unsigned driveNum = driveName[4] - 'a';
	assert((*drivesInUse)[driveNum]);
	(*drivesInUse)[driveNum] = false;
}

bool RealDrive::isDiskInserted() const
{
	// The game 'Trojka' mentions on the disk label that it works on a
	// single-sided drive. The 2nd side of the disk actually contains a
	// copy protection (obviously only checked on machines with a double
	// sided drive). This copy-protection works fine in openMSX (when using
	// a proper DMK disk image). The real disk also runs fine on machines
	// with single sided drives. Though when we initially ran this game in
	// an emulated machine with a single sided drive, the copy-protection
	// check didn't pass. Initially we emulated single sided drives by
	// simply ignoring the side-select signal. Now when the 2nd side is
	// selected on a single sided drive, we disable the drive-ready signal.
	// This makes the 'Trojka' copy-protection check pass.
	// TODO verify that this is indeed how single sided drives behave
	if (!doubleSizedDrive && (side != 0)) return false;

	return !changer->getDisk().isDummyDisk();
}

bool RealDrive::isWriteProtected() const
{
	// On a NMS8280 the write protected signal is never active when the
	// drive motor is turned off. See also isTrack00().
	if (signalsNeedMotorOn && !motorStatus) return false;
	return changer->getDisk().isWriteProtected();
}

bool RealDrive::isDoubleSided() const
{
	return doubleSizedDrive ? changer->getDisk().isDoubleSided()
	                        : false;
}

void RealDrive::setSide(bool side_)
{
	side = side_ ? 1 : 0; // also for single-sided drives
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
	// ThrottleManager heuristic:
	//  If the motor is turning and there is head movement, assume the
	//  MSX program is (still) loading/saving to disk
	if (motorStatus) setLoading(time);
}

bool RealDrive::isTrack00() const
{
	// On a Philips-NMS8280 the track00 signal is never active when the
	// drive motor is turned off. On a National-FS-5500F2 the motor status
	// doesn't matter, the single/dual drive detection routine (during
	// the MSX boot sequence) even depends on this signal with motor off.
	if (signalsNeedMotorOn && !motorStatus) return false;
	return headPos == 0;
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
		if (syncMotorTimeout.removeSyncPoint()) {
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
		if (syncMotorTimeout.pendingSyncPoint()) {
			// We had already scheduled an action to turn the motor
			// off, we're done.
			return;
		}
		// Heuristic:
		//  Immediately react to 'logical' motor status, even if the
		//  motor will (possibly) still keep rotating for a few
		//  seconds.
		syncLoadingTimeout.removeSyncPoint();
		loadingIndicator.update(false);

		// Turn the motor off after some timeout (timeout could be 0)
		syncMotorTimeout.setSyncPoint(time + motorTimeout);
	}
}

unsigned RealDrive::getCurrentAngle(EmuTime::param time) const
{
	if (motorStatus) {
		// rotating, take passed time into account
		auto deltaAngle = motorTimer.getTicksTillUp(time);
		return (startAngle + deltaAngle) % TICKS_PER_ROTATION;
	} else {
		// not rotating, angle didn't change
		return startAngle;
	}
}

void RealDrive::doSetMotor(bool status, EmuTime::param time)
{
	startAngle = getCurrentAngle(time);
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
	loadingIndicator.update(true);

	// ThrottleManager heuristic:
	//  We want to avoid getting stuck in 'loading state' when the MSX
	//  program forgets to turn off the motor.
	syncLoadingTimeout.removeSyncPoint();
	syncLoadingTimeout.setSyncPoint(time + EmuDuration::sec(1));
}

void RealDrive::execLoadingTimeout()
{
	loadingIndicator.update(false);
}

void RealDrive::execMotorTimeout(EmuTime::param time)
{
	doSetMotor(false, time);
}

bool RealDrive::indexPulse(EmuTime::param time)
{
	// Tested on real NMS8250:
	//  Only when there's a disk inserted and when the motor is spinning
	//  there are index pulses generated.
	if (!(motorStatus && isDiskInserted())) {
		return false;
	}
	return getCurrentAngle(time) < INDEX_DURATION;
}

EmuTime RealDrive::getTimeTillIndexPulse(EmuTime::param time, int count)
{
	if (!motorStatus || !isDiskInserted()) { // TODO is this correct?
		return EmuTime::infinity;
	}
	unsigned delta = TICKS_PER_ROTATION - getCurrentAngle(time);
	auto dur1 = MotorClock::duration(delta);
	auto dur2 = MotorClock::duration(TICKS_PER_ROTATION) * (count - 1);
	return time + dur1 + dur2;
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

void RealDrive::writeTrack(const RawTrack& track)
{
	changer->getDisk().writeTrack(headPos, side, track);
}

void RealDrive::readTrack(RawTrack& track)
{
	changer->getDisk().readTrack(headPos, side, track);
}

static inline unsigned divUp(unsigned a, unsigned b)
{
	return (a + b - 1) / b;
}
EmuTime RealDrive::getNextSector(
	EmuTime::param time, RawTrack& track, RawTrack::Sector& sector)
{
	int currentAngle = getCurrentAngle(time);
	changer->getDisk().readTrack(headPos, side, track);
	unsigned trackLen = track.getLength();
	unsigned idx = divUp(currentAngle * trackLen, TICKS_PER_ROTATION);
	if (!track.decodeNextSector(idx, sector)) {
		return EmuTime::infinity;
	}
	int sectorAngle = divUp(sector.addrIdx * TICKS_PER_ROTATION, trackLen);
	int delta = sectorAngle - currentAngle;
	if (delta < 0) delta += TICKS_PER_ROTATION;
	assert(0 <= delta); assert(unsigned(delta) < TICKS_PER_ROTATION);
	return time + MotorClock::duration(delta);
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
// version 3: added 'startAngle'
// version 4: removed 'userData' from Schedulable
template<typename Archive>
void RealDrive::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("syncLoadingTimeout", syncLoadingTimeout);
		ar.serialize("syncMotorTimeout",   syncMotorTimeout);
	} else {
		Schedulable::restoreOld(ar, {&syncLoadingTimeout, &syncMotorTimeout});
	}
	ar.serialize("motorTimer", motorTimer);
	ar.serialize("headLoadTimer", headLoadTimer);
	ar.serialize("changer", *changer);
	ar.serialize("headPos", headPos);
	ar.serialize("side", side);
	ar.serialize("motorStatus", motorStatus);
	ar.serialize("headLoadStatus", headLoadStatus);
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("startAngle", startAngle);
	} else {
		assert(ar.isLoader());
		startAngle = 0;
	}
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

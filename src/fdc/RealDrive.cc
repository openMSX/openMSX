#include "RealDrive.hh"
#include "Disk.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "LedStatus.hh"
#include "MSXCommandController.hh"
#include "CliComm.hh"
#include "GlobalSettings.hh"
#include "MSXException.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include <memory>

namespace openmsx {

RealDrive::RealDrive(MSXMotherBoard& motherBoard_, EmuDuration::param motorTimeout_,
                     bool signalsNeedMotorOn_, bool doubleSided,
                     DiskDrive::TrackMode trackMode_)
	: syncLoadingTimeout(motherBoard_.getScheduler())
	, syncMotorTimeout  (motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, loadingIndicator(
		motherBoard.getReactor().getGlobalSettings().getThrottleManager())
	, motorTimeout(motorTimeout_)
	, motorTimer(getCurrentTime())
	, headPos(0), side(0), startAngle(0)
	, motorStatus(false)
	, doubleSizedDrive(doubleSided)
	, signalsNeedMotorOn(signalsNeedMotorOn_)
	, trackMode(trackMode_)
	, trackValid(false), trackDirty(false)
{
	drivesInUse = motherBoard.getSharedStuff<DrivesInUse>("drivesInUse");

	unsigned i = 0;
	while ((*drivesInUse)[i]) {
		if (++i == MAX_DRIVES) {
			throw MSXException("Too many disk drives.");
		}
	}
	(*drivesInUse)[i] = true;
	std::string driveName = "diskX"; driveName[4] = char('a' + i);

	if (motherBoard.getMSXCommandController().hasCommand(driveName)) {
		throw MSXException("Duplicated drive name: ", driveName);
	}
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, driveName, "add");
	changer.emplace(motherBoard, driveName, true, doubleSizedDrive,
	                [this]() { invalidateTrack(); });
}

RealDrive::~RealDrive()
{
	try {
		flushTrack();
	} catch (MSXException&) {
		// ignore
	}
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

bool RealDrive::isDoubleSided()
{
	return doubleSizedDrive ? changer->getDisk().isDoubleSided()
	                        : false;
}

void RealDrive::setSide(bool side_)
{
	invalidateTrack();
	side = side_ ? 1 : 0; // also for single-sided drives
}

bool RealDrive::getSide() const
{
	return side;
}

unsigned RealDrive::getMaxTrack() const
{
	constexpr unsigned MAX_TRACK = 85;
	switch (trackMode) {
	case DiskDrive::TrackMode::NORMAL:
		return MAX_TRACK;
	case DiskDrive::TrackMode::YAMAHA_FD_03:
		// Yamaha FD-03: Tracks are calibrated around 4 steps per track, max 3 step further than base
		return MAX_TRACK * 4 + 3;
	default:
		UNREACHABLE;
		return 0;
	}
}

std::optional<unsigned> RealDrive::getDiskReadTrack() const
{
	// Translate head-position to track number on disk.
	// Normally this is a 1-to-1 mapping, but on the Yamaha-FD-03 the head
	// moves 4 steps for every track on disk. This means it can be
	// between disk tracks so that it can't read valid data.
	switch (trackMode) {
	case DiskDrive::TrackMode::NORMAL:
		return headPos;
	case DiskDrive::TrackMode::YAMAHA_FD_03:
		// Tracks are at a multiple of 4 steps.
		// But also make sure the track at 1 step lower and 1 step up correctly reads.
		// This makes the driver (calibration routine) use a trackoffset of 0 (so at a multiple of 4 steps).
		if ((headPos >= 2) && ((headPos % 4) != 2)) {
			return (headPos - 2) / 4;
		} else {
			return {};
		}
	default:
		UNREACHABLE;
		return {};
	}
}

std::optional<unsigned> RealDrive::getDiskWriteTrack() const
{
	switch (trackMode) {
	case DiskDrive::TrackMode::NORMAL:
		return headPos;
	case DiskDrive::TrackMode::YAMAHA_FD_03:
		// For writes allow only exact multiples of 4. But track 0 is at step 4
		if ((headPos >= 4) && ((headPos % 4) == 0)) {
			return (headPos - 4) / 4;
		} else {
			return {};
		}
	default:
		UNREACHABLE;
		return {};
	}
}

void RealDrive::step(bool direction, EmuTime::param time)
{
	invalidateTrack();

	if (direction) {
		// step in
		if (headPos < getMaxTrack()) {
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

bool RealDrive::getMotor() const
{
	// note: currently unused because of the implementation in DriveMultiplexer
	// note: currently returns the actual motor status, could be different from the
	//       last set status because of 'syncMotorTimeout'.
	return motorStatus;
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
	if (!status) {
		invalidateTrack(); // flush and ignore further writes
	}

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
		return EmuTime::infinity();
	}
	unsigned delta = TICKS_PER_ROTATION - getCurrentAngle(time);
	auto dur1 = MotorClock::duration(delta);
	auto dur2 = MotorClock::duration(TICKS_PER_ROTATION) * (count - 1);
	return time + dur1 + dur2;
}

void RealDrive::invalidateTrack()
{
	try {
		flushTrack();
	} catch (MSXException&) {
		// ignore
	}
	trackValid = false;
}

void RealDrive::getTrack()
{
	if (!motorStatus) {
		// cannot read track when disk isn't rotating
		assert(!trackValid);
		return;
	}
	if (!trackValid) {
		if (auto rdTrack = getDiskReadTrack()) {
			changer->getDisk().readTrack(*rdTrack, side, track);
		} else {
			track.clear(track.getLength());
		}
		trackValid = true;
		trackDirty = false;
	}
}

unsigned RealDrive::getTrackLength()
{
	getTrack();
	return track.getLength();
}

void RealDrive::writeTrackByte(int idx, byte val, bool addIdam)
{
	getTrack();
	// It's possible 'trackValid==false', but that's fine because in that
	// case track won't be flushed to disk anyway.
	track.write(idx, val, addIdam);
	trackDirty = true;
}

byte RealDrive::readTrackByte(int idx)
{
	getTrack();
	return trackValid ? track.read(idx) : 0;
}

static constexpr unsigned divUp(unsigned a, unsigned b)
{
	return (a + b - 1) / b;
}
EmuTime RealDrive::getNextSector(EmuTime::param time, RawTrack::Sector& sector)
{
	getTrack();
	int currentAngle = getCurrentAngle(time);
	unsigned trackLen = track.getLength();
	unsigned idx = divUp(currentAngle * trackLen, TICKS_PER_ROTATION);

	// 'addrIdx' points to the 'FE' byte in the 'A1 A1 A1 FE' sequence.
	// This method also returns the moment in time when this 'FE' byte is
	// located below the drive head. But when searching for the next sector
	// header, the FDC needs to see this full sequence. So if the rotation
	// distance is only 3 bytes or less we need to skip to the next sector
	// header. IOW we need a sector header that's at least 4 bytes removed
	// from the current position.
	if (auto s = track.decodeNextSector(idx + 4)) {
		sector = *s;
	} else {
		return EmuTime::infinity();
	}
	int sectorAngle = divUp(sector.addrIdx * TICKS_PER_ROTATION, trackLen);

	// note that if there is only one sector in this track, we have
	// to do a full rotation.
	int delta = sectorAngle - currentAngle;
	if (delta < 4) delta += TICKS_PER_ROTATION;
	assert(4 <= delta); assert(unsigned(delta) < (TICKS_PER_ROTATION + 4));

	return time + MotorClock::duration(delta);
}

void RealDrive::flushTrack()
{
	if (trackValid && trackDirty) {
		if (auto wrTrack = getDiskWriteTrack()) {
			changer->getDisk().writeTrack(*wrTrack, side, track);
		}
		trackDirty = false;
	}
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

void RealDrive::applyWd2793ReadTrackQuirk()
{
	track.applyWd2793ReadTrackQuirk();
}

void RealDrive::invalidateWd2793ReadTrackQuirk()
{
	trackValid = false;
}


// version 1: initial version
// version 2: removed 'timeOut', added MOTOR_TIMEOUT schedulable
// version 3: added 'startAngle'
// version 4: removed 'userData' from Schedulable
// version 5: added 'track', 'trackValid', 'trackDirty'
// version 6: removed 'headLoadStatus' and 'headLoadTimer'
template<typename Archive>
void RealDrive::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("syncLoadingTimeout", syncLoadingTimeout,
		             "syncMotorTimeout",   syncMotorTimeout);
	} else {
		Schedulable::restoreOld(ar, {&syncLoadingTimeout, &syncMotorTimeout});
	}
	ar.serialize("motorTimer", motorTimer,
	             "changer",    *changer,
	             "headPos",    headPos,
	             "side",       side,
	             "motorStatus", motorStatus);
	if (ar.versionAtLeast(version, 3)) {
		ar.serialize("startAngle", startAngle);
	} else {
		assert(Archive::IS_LOADER);
		startAngle = 0;
	}
	if (ar.versionAtLeast(version, 5)) {
		ar.serialize("track",      track);
		ar.serialize("trackValid", trackValid);
		ar.serialize("trackDirty", trackDirty);
	}
	if constexpr (Archive::IS_LOADER) {
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

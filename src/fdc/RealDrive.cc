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

static const unsigned MAX_DRIVES = 26; // a-z
typedef std::bitset<MAX_DRIVES> DrivesInUse;

RealDrive::RealDrive(MSXMotherBoard& motherBoard_, bool doubleSided)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
	, loadingIndicator(new LoadingIndicator(
		motherBoard.getReactor().getGlobalSettings().getThrottleManager()))
	, motorTimer(motherBoard.getCurrentTime())
	, headLoadTimer(motherBoard.getCurrentTime())
	, headPos(0), side(0), motorStatus(false), headLoadStatus(false)
	, timeOut(false)
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
	string driveName = string("disk") + char('a' + i);

	if (motherBoard.getCommandController().hasCommand(driveName)) {
		throw MSXException("Duplicated drive name: " + driveName);
	}
	motherBoard.getMSXCliComm().update(CliComm::HARDWARE, driveName, "add");
	changer.reset(new DiskChanger(
		driveName, motherBoard.getCommandController(),
		motherBoard.getReactor().getDiskManipulator(), motherBoard, true));
}

RealDrive::~RealDrive()
{
	setMotor(false, motherBoard.getCurrentTime()); // to send LED event

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
	resetTimeOut(time);
}

bool RealDrive::isTrack00() const
{
	// track00 bit is always 0 when motor is off
	// verified on NMS8280
	return motorStatus && (headPos == 0);
}

void RealDrive::setMotor(bool status, EmuTime::param time)
{
	if (motorStatus != status) {
		motorStatus = status;
		motorTimer.advance(time);
		/* The following is a hack to emulate the drive LED behaviour.
		 * This is in real life dependent on the FDC and should be
		 * investigated in detail to implement it properly... TODO */
		motherBoard.getLedStatus().setLed(LedStatus::FDD, motorStatus);
		updateLoadingState();
	}

}

bool RealDrive::indexPulse(EmuTime::param time)
{
	if (!motorStatus && isDiskInserted()) {
		return false;
	}
	int angle = motorTimer.getTicksTill(time) % TICKS_PER_ROTATION;
	return angle < INDEX_DURATION;
}

int RealDrive::indexPulseCount(EmuTime::param begin,
                               EmuTime::param end)
{
	if (!motorStatus && isDiskInserted()) {
		return 0;
	}
	int t1 = motorTimer.before(begin) ? motorTimer.getTicksTill(begin) : 0;
	int t2 = motorTimer.before(end)   ? motorTimer.getTicksTill(end)   : 0;
	return (t2 / TICKS_PER_ROTATION) - (t1 / TICKS_PER_ROTATION);
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
	EmuDuration dur = Clock<TICKS_PER_ROTATION * ROTATIONS_PER_SECOND>::
	                      duration(delta);
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

void RealDrive::executeUntil(EmuTime::param /*time*/, int /*userData*/)
{
	timeOut = true;
	updateLoadingState();
}

void RealDrive::updateLoadingState()
{
	loadingIndicator->update(motorStatus && (!timeOut));
}


const std::string& RealDrive::schedName() const
{
	static const string schedName = "RealDrive";
	return schedName;
}

void RealDrive::resetTimeOut(EmuTime::param time)
{
	timeOut = false;
	updateLoadingState();
	removeSyncPoint();
	Clock<1000> now(time);
	setSyncPoint(now + 1000);
}

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
	ar.serialize("timeOut", timeOut);
	if (ar.isLoader()) {
		updateLoadingState();

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

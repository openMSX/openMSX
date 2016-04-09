#include "Trackball.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
#include "Math.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include <algorithm>

// * Implementation based on information we received from 'n_n'.
//   It might not be 100% accurate. But games like 'Hole in one' already work.
// * Initially the 'trackball detection' code didn't work properly in openMSX
//   while it did work in meisei. Meisei had some special cases implemented for
//   the first read after reset. After some investigation I figured out some
//   code without special cases that also works as expected. Most software
//   seems to work now, though the detailed behaviour is still not tested
//   against the real hardware.

using std::string;
using std::shared_ptr;

namespace openmsx {

class TrackballState final : public StateChange
{
public:
	TrackballState() {} // for serialize
	TrackballState(EmuTime::param time_, int deltaX_, int deltaY_,
	                                     byte press_, byte release_)
		: StateChange(time_)
		, deltaX(deltaX_), deltaY(deltaY_)
		, press(press_), release(release_) {}
	int  getDeltaX()  const { return deltaX; }
	int  getDeltaY()  const { return deltaY; }
	byte getPress()   const { return press; }
	byte getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("deltaX", deltaX);
		ar.serialize("deltaY", deltaY);
		ar.serialize("press", press);
		ar.serialize("release", release);
	}
private:
	int deltaX, deltaY;
	byte press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, TrackballState, "TrackballState");


Trackball::Trackball(MSXEventDistributor& eventDistributor_,
                     StateChangeDistributor& stateChangeDistributor_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, lastSync(EmuTime::zero)
	, targetDeltaX(0), targetDeltaY(0)
	, currentDeltaX(0), currentDeltaY(0)
	, lastValue(0)
	, status(JOY_BUTTONA | JOY_BUTTONB)
	, smooth(true)
{
}

Trackball::~Trackball()
{
	if (isPluggedIn()) {
		Trackball::unplugHelper(EmuTime::dummy());
	}
}


// Pluggable
const string& Trackball::getName() const
{
	static const string name("trackball");
	return name;
}

string_ref Trackball::getDescription() const
{
	return "MSX Trackball";
}

void Trackball::plugHelper(Connector& /*connector*/, EmuTime::param time)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
	lastSync = time;
	targetDeltaX = targetDeltaY = 0;
	currentDeltaX = currentDeltaY = 0;
}

void Trackball::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
byte Trackball::read(EmuTime::param time)
{
	// From the Sony GB-7 Service manual:
	//  http://cdn.preterhuman.net/texts/computing/msx/sonygb7sm.pdf
	// * The counter seems to be 8-bit wide, though only 4 bits (bit 7 and
	//   2-0) are connected to the MSX. Looking at the M60226 block diagram
	//   in more detail shows that the (up/down-)counters have a 'HP'
	//   input, in the GB7 this input is hardwired to GND. My *guess* is
	//   that HP stands for either 'Half-' or 'High-precision' and that it
	//   selects between either 4 or 8 bits saturation.
	//   The bug report '#477 Trackball emulation overflow values too
	//   easily' contains a small movie. Some very rough calculations
	//   indicate that when you move a cursor 50 times per second, 8 pixels
	//   per step, you get about the same speed as in that move.
	//   So even though both 4 and 8 bit clipping *seem* to be possible,
	//   this code only implements 4 bit clipping.
	// * It also contains a test program to read the trackball position.
	//   This program first reads the (X or Y) value and only then toggles
	//   pin 8. This seems to suggest the actual (X or Y) value is always
	//   present on reads and toggling pin 8 resets this value and switches
	//   to the other axis.
	syncCurrentWithTarget(time);
	auto delta = (lastValue & 4) ? currentDeltaY : currentDeltaX;
	return (status & ~0x0F) | ((delta + 8) & 0x0F);
}

void Trackball::write(byte value, EmuTime::param time)
{
	syncCurrentWithTarget(time);
	byte diff = lastValue ^ value;
	lastValue = value;
	if (diff & 0x4) {
		// pin 8 flipped
		if (value & 4) {
			targetDeltaX = Math::clip<-8, 7>(targetDeltaX - currentDeltaX);
			currentDeltaX = 0;
		} else {
			targetDeltaY = Math::clip<-8, 7>(targetDeltaY - currentDeltaY);
			currentDeltaY = 0;
		}
	}
}

void Trackball::syncCurrentWithTarget(EmuTime::param time)
{
	// In the past we only had 'targetDeltaXY' (was named 'deltaXY' then).
	// 'currentDeltaXY' was introduced to (slightly) smooth-out the
	// discrete host mouse movement events over time. This method performs
	// that smoothing calculation.
	//
	// In emulation, the trackball movement is driven by host mouse
	// movement events. These events are discrete. So for example if we
	// receive a host mouse event with offset (3,-4) we immediately adjust
	// 'targetDeltaXY' by that offset. However in reality mouse movement
	// doesn't make such discrete jumps. If you look at small enough time
	// intervals you'll see that the offset smoothly varies from (0,0) to
	// (3,-4).
	//
	// Most often this discretization step doesn't matter. However the BIOS
	// routine GTPAD to read mouse/trackball (more specifically PAD(12) and
	// PAD(16) has an heuristic to distinguish the mouse from the trackball
	// protocol. I won't go into detail, but in short it's reading the
	// mouse/trackball two times shortly after each other and checks
	// whether the offset of the 2nd read is centered around 0 (for mouse)
	// or 8 (for trackball). There's only about 1 millisecond between the
	// two reads, so in reality the mouse/trackball won't have moved much
	// during that time. However in emulation because of the discretization
	// we can get unlucky and do see a large offset for the 2nd read. This
	// confuses the BIOS (it thinks it's talking to a mouse instead of
	// trackball) and it results in erratic trackball movement.
	//
	// Thus to work around this problem (=make the heuristic in the BIOS
	// work) we smear-out host mouse events over (emulated) time. Instead
	// of immediately following the host movement, we limit changes in the
	// emulated offsets to a rate of 1 step per millisecond. This
	// introduces some delay, but usually (for not too fast movements) it's
	// not noticeable.

	if (!smooth) {
		// for backwards-compatible replay files
		currentDeltaX = targetDeltaX;
		currentDeltaY = targetDeltaY;
		return;
	}

	static const EmuDuration INTERVAL = EmuDuration::msec(1);

	int maxSteps = (time - lastSync) / INTERVAL;
	lastSync += INTERVAL * maxSteps;

	if (targetDeltaX >= currentDeltaX) {
		currentDeltaX = std::min<int>(currentDeltaX + maxSteps, targetDeltaX);
	} else {
		currentDeltaX = std::max<int>(currentDeltaX - maxSteps, targetDeltaX);
	}
	if (targetDeltaY >= currentDeltaY) {
		currentDeltaY = std::min<int>(currentDeltaY + maxSteps, targetDeltaY);
	} else {
		currentDeltaY = std::max<int>(currentDeltaY - maxSteps, targetDeltaY);
	}
}

// MSXEventListener
void Trackball::signalEvent(const shared_ptr<const Event>& event,
                            EmuTime::param time)
{
	switch (event->getType()) {
	case OPENMSX_MOUSE_MOTION_EVENT: {
		auto& mev = checked_cast<const MouseMotionEvent&>(*event);
		static const int SCALE = 2;
		int dx = mev.getX() / SCALE;
		int dy = mev.getY() / SCALE;
		if ((dx != 0) || (dy != 0)) {
			createTrackballStateChange(time, dx, dy, 0, 0);
		}
		break;
	}
	case OPENMSX_MOUSE_BUTTON_DOWN_EVENT: {
		auto& butEv = checked_cast<const MouseButtonEvent&>(*event);
		switch (butEv.getButton()) {
		case MouseButtonEvent::LEFT:
			createTrackballStateChange(time, 0, 0, JOY_BUTTONA, 0);
			break;
		case MouseButtonEvent::RIGHT:
			createTrackballStateChange(time, 0, 0, JOY_BUTTONB, 0);
			break;
		default:
			// ignore other buttons
			break;
		}
		break;
	}
	case OPENMSX_MOUSE_BUTTON_UP_EVENT: {
		auto& butEv = checked_cast<const MouseButtonEvent&>(*event);
		switch (butEv.getButton()) {
		case MouseButtonEvent::LEFT:
			createTrackballStateChange(time, 0, 0, 0, JOY_BUTTONA);
			break;
		case MouseButtonEvent::RIGHT:
			createTrackballStateChange(time, 0, 0, 0, JOY_BUTTONB);
			break;
		default:
			// ignore other buttons
			break;
		}
		break;
	}
	default:
		// ignore
		break;
	}
}

void Trackball::createTrackballStateChange(
	EmuTime::param time, int deltaX, int deltaY, byte press, byte release)
{
	stateChangeDistributor.distributeNew(std::make_shared<TrackballState>(
		time, deltaX, deltaY, press, release));
}

// StateChangeListener
void Trackball::signalStateChange(const shared_ptr<StateChange>& event)
{
	auto ts = dynamic_cast<TrackballState*>(event.get());
	if (!ts) return;

	targetDeltaX = Math::clip<-8, 7>(targetDeltaX + ts->getDeltaX());
	targetDeltaY = Math::clip<-8, 7>(targetDeltaY + ts->getDeltaY());
	status = (status & ~ts->getPress()) | ts->getRelease();
}

void Trackball::stopReplay(EmuTime::param time)
{
	syncCurrentWithTarget(time);
	// TODO Get actual mouse button(s) state. Is it worth the trouble?
	byte release = (JOY_BUTTONA | JOY_BUTTONB) & ~status;
	if ((currentDeltaX != 0) || (currentDeltaY != 0) || (release != 0)) {
		stateChangeDistributor.distributeNew(
			std::make_shared<TrackballState>(
				time, -currentDeltaX, -currentDeltaY, 0, release));
	}
}

// version 1: initial version
// version 2: replaced deltaXY with targetDeltaXY and currentDeltaXY
template<typename Archive>
void Trackball::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("lastSync" ,lastSync);
		ar.serialize("targetDeltaX", targetDeltaX);
		ar.serialize("targetDeltaY", targetDeltaY);
		ar.serialize("currentDeltaX", currentDeltaX);
		ar.serialize("currentDeltaY", currentDeltaY);
	} else {
		ar.serialize("deltaX", targetDeltaX);
		ar.serialize("deltaY", targetDeltaY);
		currentDeltaX = targetDeltaX;
		currentDeltaY = targetDeltaY;
		smooth = false;
	}
	ar.serialize("lastValue", lastValue);
	ar.serialize("status", status);

	if (ar.isLoader() && isPluggedIn()) {
		plugHelper(*getConnector(), EmuTime::dummy());
	}
}
INSTANTIATE_SERIALIZE_METHODS(Trackball);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Trackball, "Trackball");

} // namespace openmsx

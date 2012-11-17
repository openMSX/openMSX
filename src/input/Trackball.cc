// $Id$

#include "Trackball.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
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

class TrackballState : public StateChange
{
public:
	TrackballState() {} // for serialize
	TrackballState(EmuTime::param time, int deltaX_, int deltaY_,
	                                    byte press_, byte release_)
		: StateChange(time)
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
	, deltaX(0), deltaY(0)
	, lastValue(0)
	, status(JOY_BUTTONA | JOY_BUTTONB)
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

void Trackball::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
	deltaX = deltaY = 0;
}

void Trackball::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
byte Trackball::read(EmuTime::param /*time*/)
{
	// From the Sony GB-7 Service manual:
	// * The counter seems to be 8-bit wide, though only 4 bits (bit 7 and
	//   2-0) are connected to the MSX.
	// * It also contains a test program to read the trackball position.
	//   This program first reads the (X or Y) value and only then toggles
	//   pin 8. This seems to suggest the actual (X or Y) value is always
	//   present on reads and toggling pin 8 resets this value and switches
	//   to the other axis.
	signed char& delta = (lastValue & 4) ? deltaY : deltaX;
	unsigned t = delta + 128;
	return (status & ~0x0F) | ((t & 0x80) >> 4) | (t & 0x07);
}

void Trackball::write(byte value, EmuTime::param /*time*/)
{
	byte diff = lastValue ^ value;
	lastValue = value;
	if (diff & 0x4) {
		// pin 8 flipped
		if (value & 4) {
			deltaX = 0;
		} else {
			deltaY = 0;
		}
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

	deltaX = std::min(127, std::max(-128, deltaX + ts->getDeltaX()));
	deltaY = std::min(127, std::max(-128, deltaY + ts->getDeltaY()));
	status = (status & ~ts->getPress()) | ts->getRelease();
}

void Trackball::stopReplay(EmuTime::param time)
{
	// TODO Get actual mouse button(s) state. Is it worth the trouble?
	byte release = (JOY_BUTTONA | JOY_BUTTONB) & ~status;
	if ((deltaX != 0) || (deltaY != 0) || (release != 0)) {
		stateChangeDistributor.distributeNew(
			std::make_shared<TrackballState>(
				time, -deltaX, -deltaY, 0, release));
	}
}


template<typename Archive>
void Trackball::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("deltaX", deltaX);
	ar.serialize("deltaY", deltaY);
	ar.serialize("lastValue", lastValue);
	ar.serialize("status", status);

	if (ar.isLoader() && isPluggedIn()) {
		plugHelper(*getConnector(), EmuTime::dummy());
	}
}
INSTANTIATE_SERIALIZE_METHODS(Trackball);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Trackball, "Trackball");

} // namespace openmsx

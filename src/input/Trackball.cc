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

// Implementation based on information we received from 'n_n'.
//
// It might not be 100% accurate. But games like 'Hole in one' already work.

using std::string;

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
	, status(JOY_BUTTONA | JOY_BUTTONB | 8)
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

const string& Trackball::getDescription() const
{
	static const string desc("Trackball.");
	return desc;
}

void Trackball::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void Trackball::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
byte Trackball::read(EmuTime::param /*time*/)
{
	return status;
}

void Trackball::write(byte value, EmuTime::param /*time*/)
{
	byte diff = lastValue ^ value;
	lastValue = value;

	if (diff & 0x4) {
		// pin 8 flipped
		signed char& delta = (value & 4) ? deltaY : deltaX;
		int d4 = std::min(7, std::max<int>(-8, delta));
		delta -= d4;
		status = (status & ~0x0F) | (d4 + 8);
	}
}

// MSXEventListener
void Trackball::signalEvent(shared_ptr<const Event> event, EmuTime::param time)
{
	switch (event->getType()) {
	case OPENMSX_MOUSE_MOTION_EVENT: {
		const MouseMotionEvent& motionEvent =
			checked_cast<const MouseMotionEvent&>(*event);
		static const int SCALE = 2;
		int dx = motionEvent.getX() / SCALE;
		int dy = motionEvent.getY() / SCALE;
		if ((dx != 0) || (dy != 0)) {
			createTrackballStateChange(time, dx, dy, 0, 0);
		}
		break;
	}
	case OPENMSX_MOUSE_BUTTON_DOWN_EVENT: {
		const MouseButtonEvent& buttonEvent =
			checked_cast<const MouseButtonEvent&>(*event);
		switch (buttonEvent.getButton()) {
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
		const MouseButtonEvent& buttonEvent =
			checked_cast<const MouseButtonEvent&>(*event);
		switch (buttonEvent.getButton()) {
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
	stateChangeDistributor.distributeNew(shared_ptr<StateChange>(
		new TrackballState(time, deltaX, deltaY, press, release)));
}

// StateChangeListener
void Trackball::signalStateChange(shared_ptr<StateChange> event)
{
	TrackballState* ts = dynamic_cast<TrackballState*>(event.get());
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
		stateChangeDistributor.distributeNew(shared_ptr<StateChange>(
			new TrackballState(time, -deltaX, -deltaY, 0, release)));
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

// $Id$

#include "ArkanoidPad.hh"
#include "MSXEventDistributor.hh"
#include "InputEvents.hh"
#include "checked_cast.hh"
#include "serialize.hh"

// implemented accoring to the info here: http://www.msx.org/forumtopic7661.html
// this is absolutely not accurate, but good enough to make the pad work in the
// Arkanoid games.

using std::string;

namespace openmsx {

const int SCALE = 2;

ArkanoidPad::ArkanoidPad(MSXEventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
	, status(0x3E)
	, shiftreg(0)
	, dialpos(236) // center position
	, readShiftRegMode(false)
	, lastTimeShifted(false)
{
	eventDistributor.registerEventListener(*this);
}

ArkanoidPad::~ArkanoidPad()
{
	eventDistributor.unregisterEventListener(*this);
}


// Pluggable
const string& ArkanoidPad::getName() const
{
	static const string name("arkanoidpad");
	return name;
}

const string& ArkanoidPad::getDescription() const
{
	static const string desc("Arkanoid pad.");
	return desc;
}

void ArkanoidPad::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
}

void ArkanoidPad::unplugHelper(EmuTime::param /*time*/)
{
}

// JoystickDevice
byte ArkanoidPad::read(EmuTime::param /*time*/)
{
	return status | ((shiftreg & 256) >> 8);
}

void ArkanoidPad::write(byte value, EmuTime::param /*time*/)
{
	if (!readShiftRegMode) {
		if (value & 0x4) { // pin 8 from low to high
			readShiftRegMode = true;
			shiftreg = dialpos;
		}
	} else if (!(value & 0x4)) {
		readShiftRegMode = false;
	}
	if (!lastTimeShifted) {
		if (value & 0x1) {
			lastTimeShifted = true;
			shiftreg = shiftreg << 1;
		}
	} else if (!(value & 0x1)) {
		lastTimeShifted = false;
	}
}

// EventListener
void ArkanoidPad::signalEvent(shared_ptr<const Event> event, EmuTime::param /*time*/)
{
	switch (event->getType()) {
	case OPENMSX_MOUSE_MOTION_EVENT: {
		const MouseMotionEvent& motionEvent =
			checked_cast<const MouseMotionEvent&>(*event);
		dialpos += (motionEvent.getX() / SCALE);
		if (dialpos > 309) dialpos = 309;
		if (dialpos < 163) dialpos = 163;
		break;
	}
	case OPENMSX_MOUSE_BUTTON_DOWN_EVENT: {
		// any button will press the Arkanoid Pad button
		status &= 0xFD; // reset bit 1
		break;
	}
	case OPENMSX_MOUSE_BUTTON_UP_EVENT: {
		// any button will unpress the Arkanoid Pad button
		status |= 0x02; // set bit 1
		break;
	}
	default:
		// ignore
		break;
	}
}


template<typename Archive>
void ArkanoidPad::serialize(Archive& ar, unsigned /*version*/)
{
	// is this enough?? TODO: check
	ar.serialize("shiftreg", shiftreg);

	// Don't serialzie status, dialpos.
	// These are controlled via (mouse button/motion) events
}
INSTANTIATE_SERIALIZE_METHODS(ArkanoidPad);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, ArkanoidPad, "ArkanoidPad");

} // namespace openmsx

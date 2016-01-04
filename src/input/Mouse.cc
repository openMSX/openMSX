#include "Mouse.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
#include "Clock.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "unreachable.hh"
#include <SDL.h>

using std::string;
using std::min;
using std::max;
using std::shared_ptr;

namespace openmsx {

static const int TRESHOLD = 2;
static const int SCALE = 2;
static const int PHASE_XHIGH = 0;
static const int PHASE_XLOW  = 1;
static const int PHASE_YHIGH = 2;
static const int PHASE_YLOW  = 3;
static const int STROBE = 0x04;


class MouseState final : public StateChange
{
public:
	MouseState() {} // for serialize
	MouseState(EmuTime::param time_, int deltaX_, int deltaY_,
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

REGISTER_POLYMORPHIC_CLASS(StateChange, MouseState, "MouseState");

Mouse::Mouse(MSXEventDistributor& eventDistributor_,
             StateChangeDistributor& stateChangeDistributor_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, lastTime(EmuTime::zero)
{
	status = JOY_BUTTONA | JOY_BUTTONB;
	phase = PHASE_YLOW;
	xrel = yrel = curxrel = curyrel = 0;
	absHostX = absHostY = 0;
	mouseMode = true;
}

Mouse::~Mouse()
{
	if (isPluggedIn()) {
		Mouse::unplugHelper(EmuTime::dummy());
	}
}


// Pluggable
const string& Mouse::getName() const
{
	static const string name("mouse");
	return name;
}

string_ref Mouse::getDescription() const
{
	return "MSX mouse";
}

void Mouse::plugHelper(Connector& /*connector*/, EmuTime::param time)
{
	if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT)) {
		// left mouse button pressed, joystick emulation mode
		mouseMode = false;
	} else {
		// not pressed, mouse mode
		mouseMode = true;
		lastTime = time;
	}
	plugHelper2();
}

void Mouse::plugHelper2()
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void Mouse::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// JoystickDevice
byte Mouse::read(EmuTime::param /*time*/)
{
	if (mouseMode) {
		switch (phase) {
		case PHASE_XHIGH:
			return ((xrel >> 4) & 0x0F) | status;
		case PHASE_XLOW:
			return  (xrel       & 0x0F) | status;
		case PHASE_YHIGH:
			return ((yrel >> 4) & 0x0F) | status;
		case PHASE_YLOW:
			return  (yrel       & 0x0F) | status;
		default:
			UNREACHABLE; return 0;
		}
	} else {
		emulateJoystick();
		return status;
	}
}

void Mouse::emulateJoystick()
{
	status &= ~(JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT);

	int deltax = curxrel; curxrel = 0;
	int deltay = curyrel; curyrel = 0;
	int absx = (deltax > 0) ? deltax : -deltax;
	int absy = (deltay > 0) ? deltay : -deltay;

	if ((absx < TRESHOLD) && (absy < TRESHOLD)) {
		return;
	}

	// tan(pi/8) ~= 5/12
	if (deltax > 0) {
		if (deltay > 0) {
			if ((12 * absx) > (5 * absy)) {
				status |= JOY_RIGHT;
			}
			if ((12 * absy) > (5 * absx)) {
				status |= JOY_DOWN;
			}
		} else {
			if ((12 * absx) > (5 * absy)) {
				status |= JOY_RIGHT;
			}
			if ((12 * absy) > (5 * absx)) {
				status |= JOY_UP;
			}
		}
	} else {
		if (deltay > 0) {
			if ((12 * absx) > (5 * absy)) {
				status |= JOY_LEFT;
			}
			if ((12 * absy) > (5 * absx)) {
				status |= JOY_DOWN;
			}
		} else {
			if ((12 * absx) > (5 * absy)) {
				status |= JOY_LEFT;
			}
			if ((12 * absy) > (5 * absx)) {
				status |= JOY_UP;
			}
		}
	}
}

void Mouse::write(byte value, EmuTime::param time)
{
	if (mouseMode) {
		// TODO figure out the exact timeout value. Is there even such
		//   an exact value or can it vary between different mouse
		//   models?
		//
		// Initially we used a timeout of 1 full second. This caused bug
		//    [3520394] Mouse behaves badly (unusable) in HiBrid
		// Slightly lowering the value to around 0.94s was already
		// enough to fix that bug. Later we found that to make FRS's
		// joytest program work we need a value that is less than the
		// duration of one (NTSC) frame. See bug
		//    #474 Mouse doesn't work properly on Joytest v2.2
		// We still don't know the exact value that an actual MSX mouse
		// uses, but 1.5ms is also the timeout value that is used for
		// JoyMega, so it seems like a reasonable value.
		if ((time - lastTime) > EmuDuration::usec(1500)) {
			phase = PHASE_YLOW;
		}
		lastTime = time;

		switch (phase) {
		case PHASE_XHIGH:
			if ((value & STROBE) == 0) phase = PHASE_XLOW;
			break;
		case PHASE_XLOW:
			if ((value & STROBE) != 0) phase = PHASE_YHIGH;
			break;
		case PHASE_YHIGH:
			if ((value & STROBE) == 0) phase = PHASE_YLOW;
			break;
		case PHASE_YLOW:
			if ((value & STROBE) != 0) {
				phase = PHASE_XHIGH;
				xrel = curxrel; yrel = curyrel;
				curxrel = 0; curyrel = 0;
			}
			break;
		}
	} else {
		// ignore
	}
}


// MSXEventListener
void Mouse::signalEvent(const shared_ptr<const Event>& event, EmuTime::param time)
{
	switch (event->getType()) {
	case OPENMSX_MOUSE_MOTION_EVENT: {
		auto& mev = checked_cast<const MouseMotionEvent&>(*event);
		if (mev.getX() || mev.getY()) {
			// note: X/Y are negated, do this already in this
			//  routine to keep replays bw-compat. In a new
			//  savestate version it may (or may not) be cleaner
			//  to perform this operation closer to the MSX code.
			createMouseStateChange(time, -mev.getX(), -mev.getY(), 0, 0);
		}
		break;
	}
	case OPENMSX_MOUSE_BUTTON_DOWN_EVENT: {
		auto& butEv = checked_cast<const MouseButtonEvent&>(*event);
		switch (butEv.getButton()) {
		case MouseButtonEvent::LEFT:
			createMouseStateChange(time, 0, 0, JOY_BUTTONA, 0);
			break;
		case MouseButtonEvent::RIGHT:
			createMouseStateChange(time, 0, 0, JOY_BUTTONB, 0);
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
			createMouseStateChange(time, 0, 0, 0, JOY_BUTTONA);
			break;
		case MouseButtonEvent::RIGHT:
			createMouseStateChange(time, 0, 0, 0, JOY_BUTTONB);
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

void Mouse::createMouseStateChange(
	EmuTime::param time, int deltaX, int deltaY, byte press, byte release)
{
	stateChangeDistributor.distributeNew(std::make_shared<MouseState>(
		time, deltaX, deltaY, press, release));
}

void Mouse::signalStateChange(const shared_ptr<StateChange>& event)
{
	auto ms = dynamic_cast<MouseState*>(event.get());
	if (!ms) return;

	// This is almost the same as
	//    relMsxXY = ms->getDeltaXY() / SCALE
	// except that it doesn't accumulate rounding errors
	int oldMsxX = absHostX / SCALE;
	int oldMsxY = absHostY / SCALE;
	absHostX += ms->getDeltaX();
	absHostY += ms->getDeltaY();
	int newMsxX = absHostX / SCALE;
	int newMsxY = absHostY / SCALE;
	int relMsxX = newMsxX - oldMsxX;
	int relMsxY = newMsxY - oldMsxY;

	// Verified with a real MSX-mouse (Philips SBC3810):
	//   this value is not clipped to -128 .. 127.
	curxrel += relMsxX;
	curyrel += relMsxY;
	status = (status & ~ms->getPress()) | ms->getRelease();
}

void Mouse::stopReplay(EmuTime::param time)
{
	// TODO read actual host mouse button state
	int dx = 0 - curxrel;
	int dy = 0 - curyrel;
	byte release = (JOY_BUTTONA | JOY_BUTTONB) & ~status;
	if ((dx != 0) || (dy != 0) || (release != 0)) {
		createMouseStateChange(time, dx, dy, 0, release);
	}
}

// version 1: Initial version, the variables curxrel, curyrel and status were
//            not serialized.
// version 2: Also serialize the above variables, this is required for
//            record/replay, see comment in Keyboard.cc for more details.
// version 3: variables '(cur){x,y}rel' are scaled to MSX coordinates
// version 4: simplified type of 'lastTime' from Clock<> to EmuTime
template<typename Archive>
void Mouse::serialize(Archive& ar, unsigned version)
{
	if (ar.isLoader() && isPluggedIn()) {
		// Do this early, because if something goes wrong while loading
		// some state below, then unplugHelper() gets called and that
		// will assert when plugHelper2() wasn't called yet.
		plugHelper2();
	}

	if (ar.versionBelow(version, 4)) {
		assert(ar.isLoader());
		Clock<1000> tmp(EmuTime::zero);
		ar.serialize("lastTime", tmp);
		lastTime = tmp.getTime();
	} else {
		ar.serialize("lastTime", lastTime);
	}
	ar.serialize("faze", phase); // TODO fix spelling if there's ever a need
	                             // to bump the serialization verion
	ar.serialize("xrel", xrel);
	ar.serialize("yrel", yrel);
	ar.serialize("mouseMode", mouseMode);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("curxrel", curxrel);
		ar.serialize("curyrel", curyrel);
		ar.serialize("status",  status);
	}
	if (ar.versionBelow(version, 3)) {
		xrel    /= SCALE;
		yrel    /= SCALE;
		curxrel /= SCALE;
		curyrel /= SCALE;

	}
	// no need to serialize absHostX,Y
}
INSTANTIATE_SERIALIZE_METHODS(Mouse);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Mouse, "Mouse");

} // namespace openmsx

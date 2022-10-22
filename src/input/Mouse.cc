#include "Mouse.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Event.hh"
#include "StateChange.hh"
#include "Clock.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "unreachable.hh"
#include "Math.hh"
#include <SDL.h>
#include <algorithm>

namespace openmsx {

static constexpr int TRESHOLD = 2;
static constexpr int SCALE = 2;
static constexpr int PHASE_XHIGH = 0;
static constexpr int PHASE_XLOW  = 1;
static constexpr int PHASE_YHIGH = 2;
static constexpr int PHASE_YLOW  = 3;
static constexpr int STROBE = 0x04;


class MouseState final : public StateChange
{
public:
	MouseState() = default; // for serialize
	MouseState(EmuTime::param time_, int deltaX_, int deltaY_,
	           byte press_, byte release_)
		: StateChange(time_)
		, deltaX(deltaX_), deltaY(deltaY_)
		, press(press_), release(release_) {}
	[[nodiscard]] int  getDeltaX()  const { return deltaX; }
	[[nodiscard]] int  getDeltaY()  const { return deltaY; }
	[[nodiscard]] byte getPress()   const { return press; }
	[[nodiscard]] byte getRelease() const { return release; }
	template<typename Archive> void serialize(Archive& ar, unsigned version)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("deltaX",  deltaX,
		             "deltaY",  deltaY,
		             "press",   press,
		             "release", release);
		if (ar.versionBelow(version, 2)) {
			assert(Archive::IS_LOADER);
			// Old versions stored host (=unscaled) mouse movement
			// in the replay-event-log. Apply the (old) algorithm
			// to scale host to msx mouse movement.
			// In principle the code snippet below does:
			//    delta{X,Y} /= SCALE
			// except that it doesn't accumulate rounding errors
			int oldMsxX = absHostX / SCALE;
			int oldMsxY = absHostY / SCALE;
			absHostX += deltaX;
			absHostY += deltaY;
			int newMsxX = absHostX / SCALE;
			int newMsxY = absHostY / SCALE;
			deltaX = newMsxX - oldMsxX;
			deltaY = newMsxY - oldMsxY;
		}
	}
private:
	int deltaX, deltaY; // msx mouse movement
	byte press, release;
public:
	inline static int absHostX = 0, absHostY = 0; // (only) for old savestates
};
SERIALIZE_CLASS_VERSION(MouseState, 2);

REGISTER_POLYMORPHIC_CLASS(StateChange, MouseState, "MouseState");

Mouse::Mouse(MSXEventDistributor& eventDistributor_,
             StateChangeDistributor& stateChangeDistributor_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, lastTime(EmuTime::zero())
	, phase(PHASE_YLOW)
	, xrel(0)
	, yrel(0)
	, curxrel(0)
	, curyrel(0)
	, fractionalX(0)
	, fractionalY(0)
	, status(JOY_BUTTONA | JOY_BUTTONB)
	, mouseMode(true)
{
}

Mouse::~Mouse()
{
	if (isPluggedIn()) {
		Mouse::unplugHelper(EmuTime::dummy());
	}
}


// Pluggable
std::string_view Mouse::getName() const
{
	return "mouse";
}

std::string_view Mouse::getDescription() const
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

	int deltaX = curxrel; curxrel = 0;
	int deltaY = curyrel; curyrel = 0;
	int absX = (deltaX > 0) ? deltaX : -deltaX;
	int absY = (deltaY > 0) ? deltaY : -deltaY;

	if ((absX < TRESHOLD) && (absY < TRESHOLD)) {
		return;
	}

	// tan(pi/8) ~= 5/12
	if (deltaX > 0) {
		if (deltaY > 0) {
			if ((12 * absX) > (5 * absY)) {
				status |= JOY_RIGHT;
			}
			if ((12 * absY) > (5 * absX)) {
				status |= JOY_DOWN;
			}
		} else {
			if ((12 * absX) > (5 * absY)) {
				status |= JOY_RIGHT;
			}
			if ((12 * absY) > (5 * absX)) {
				status |= JOY_UP;
			}
		}
	} else {
		if (deltaY > 0) {
			if ((12 * absX) > (5 * absY)) {
				status |= JOY_LEFT;
			}
			if ((12 * absY) > (5 * absX)) {
				status |= JOY_DOWN;
			}
		} else {
			if ((12 * absX) > (5 * absY)) {
				status |= JOY_LEFT;
			}
			if ((12 * absY) > (5 * absX)) {
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
#if 0
				// Real MSX mice don't have overflow protection,
				// verified on a Philips SBC3810 MSX mouse.
				xrel = curxrel; yrel = curyrel;
				curxrel = 0; curyrel = 0;
#else
				// Nevertheless we do emulate it here. See
				// sdsnatcher's post of 30 aug 2018 for a
				// motivation for this difference:
				//   https://github.com/openMSX/openMSX/issues/892
				xrel = std::clamp(curxrel, -127, 127);
				yrel = std::clamp(curyrel, -127, 127);
				curxrel -= xrel;
				curyrel -= yrel;
#endif
			}
			break;
		}
	} else {
		// ignore
	}
}


// MSXEventListener
void Mouse::signalMSXEvent(const Event& event, EmuTime::param time) noexcept
{
	visit(overloaded{
		[&](const MouseMotionEvent& e) {
			if (e.getX() || e.getY()) {
				// Note: regular C/C++ division rounds towards
				// zero, so different direction for positive and
				// negative values. But we get smoother output
				// with a uniform rounding direction.
				auto qrX = Math::div_mod_floor(e.getX() + fractionalX, SCALE);
				auto qrY = Math::div_mod_floor(e.getY() + fractionalY, SCALE);
				fractionalX = qrX.remainder;
				fractionalY = qrY.remainder;

				// Note: hostXY is negated when converting to MsxXY
				createMouseStateChange(time, -qrX.quotient, -qrY.quotient, 0, 0);
			}
		},
		[&](const MouseButtonDownEvent& e) {
			switch (e.getButton()) {
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
		},
		[&](const MouseButtonUpEvent& e) {
			switch (e.getButton()) {
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
		},
		[](const EventBase&) { /*ignore*/ }
	}, event);
}

void Mouse::createMouseStateChange(
	EmuTime::param time, int deltaX, int deltaY, byte press, byte release)
{
	stateChangeDistributor.distributeNew<MouseState>(
		time, deltaX, deltaY, press, release);
}

void Mouse::signalStateChange(const StateChange& event)
{
	const auto* ms = dynamic_cast<const MouseState*>(&event);
	if (!ms) return;

	// Verified with a real MSX-mouse (Philips SBC3810):
	//   this value is not clipped to -128 .. 127.
	curxrel += ms->getDeltaX();
	curyrel += ms->getDeltaY();
	status = (status & ~ms->getPress()) | ms->getRelease();
}

void Mouse::stopReplay(EmuTime::param time) noexcept
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
	// (Only) for loading old savestates
	MouseState::absHostX = MouseState::absHostY = 0;

	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			// Do this early, because if something goes wrong while loading
			// some state below, then unplugHelper() gets called and that
			// will assert when plugHelper2() wasn't called yet.
			plugHelper2();
		}
	}

	if (ar.versionBelow(version, 4)) {
		assert(Archive::IS_LOADER);
		Clock<1000> tmp(EmuTime::zero());
		ar.serialize("lastTime", tmp);
		lastTime = tmp.getTime();
	} else {
		ar.serialize("lastTime", lastTime);
	}
	ar.serialize("faze",      phase, // TODO fix spelling if there's ever a need
	                                 // to bump the serialization verion
	             "xrel",      xrel,
	             "yrel",      yrel,
	             "mouseMode", mouseMode);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("curxrel", curxrel,
		             "curyrel", curyrel,
		             "status",  status);
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

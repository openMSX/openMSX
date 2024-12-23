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

static constexpr int THRESHOLD = 2;
static constexpr int SCALE = 2;
static constexpr int PHASE_XHIGH1 = 0;
static constexpr int PHASE_XLOW1  = 1;
static constexpr int PHASE_YHIGH1 = 2;
static constexpr int PHASE_YLOW1  = 3;
static constexpr int PHASE_XHIGH2 = 4;
static constexpr int PHASE_XLOW2  = 5;
static constexpr int PHASE_YHIGH2 = 6;
static constexpr int PHASE_YLOW2  = 7;
static constexpr int STROBE = 0x04;


class MouseState final : public StateChange
{
public:
	MouseState() = default; // for serialize
	MouseState(EmuTime::param time_, int deltaX_, int deltaY_,
	           uint8_t press_, uint8_t release_)
		: StateChange(time_)
		, deltaX(deltaX_), deltaY(deltaY_)
		, press(press_), release(release_) {}
	[[nodiscard]] int     getDeltaX()  const { return deltaX; }
	[[nodiscard]] int     getDeltaY()  const { return deltaY; }
	[[nodiscard]] uint8_t getPress()   const { return press; }
	[[nodiscard]] uint8_t getRelease() const { return release; }
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
	uint8_t press, release;
public:
	inline static int absHostX = 0, absHostY = 0; // (only) for old savestates
};
SERIALIZE_CLASS_VERSION(MouseState, 2);

REGISTER_POLYMORPHIC_CLASS(StateChange, MouseState, "MouseState");

Mouse::Mouse(MSXEventDistributor& eventDistributor_,
             StateChangeDistributor& stateChangeDistributor_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, phase(PHASE_YLOW2)
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
uint8_t Mouse::read(EmuTime::param /*time*/)
{
	if (mouseMode) {
		switch (phase) {
		case PHASE_XHIGH1: case PHASE_XHIGH2:
			return ((xRel >> 4) & 0x0F) | status;
		case PHASE_XLOW1: case PHASE_XLOW2:
			return  (xRel       & 0x0F) | status;
		case PHASE_YHIGH1: case PHASE_YHIGH2:
			return ((yRel >> 4) & 0x0F) | status;
		case PHASE_YLOW1: case PHASE_YLOW2:
			return  (yRel       & 0x0F) | status;
		default:
			UNREACHABLE;
		}
	} else {
		emulateJoystick();
		return status;
	}
}

void Mouse::emulateJoystick()
{
	status &= ~(JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT);

	int deltaX = curXRel; curXRel = 0;
	int deltaY = curYRel; curYRel = 0;
	int absX = (deltaX > 0) ? deltaX : -deltaX;
	int absY = (deltaY > 0) ? deltaY : -deltaY;

	if ((absX < THRESHOLD) && (absY < THRESHOLD)) {
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

void Mouse::write(uint8_t value, EmuTime::param time)
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
			// Timeout to YLOW2 so that MSX software bypassing the BIOS
			// and potentially not performing the alternate cycle
			// (which normally is used for trackball detection)
			// still gets continuous delta updates in each scan round
			phase = PHASE_YLOW2;
		}
		lastTime = time;

		switch (phase) {
		case PHASE_XHIGH1: case PHASE_XHIGH2:
		case PHASE_YHIGH1: case PHASE_YHIGH2:
			if ((value & STROBE) == 0) ++phase;
			break;
		case PHASE_XLOW1: case PHASE_XLOW2:
			if ((value & STROBE) != 0) ++phase;
			break;
		case PHASE_YLOW1:
			if ((value & STROBE) != 0) {
				phase = PHASE_XHIGH2;
				// Keep the delta zero in the alternate cycle of the scan round
				// to avoid trackball detection for large (absolute) values
				// Timeout resets the state to YLOW2 so that each scan round syncs to XHIGH1
				xRel = yRel = 0;
			}
			break;
		case PHASE_YLOW2:
			if ((value & STROBE) != 0) {
				phase = PHASE_XHIGH1;
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
				// Only introduce the next delta when we are about to enter the main cycle
				// to avoid trackball detection in the alternate cycle
				// See https://github.com/openMSX/openMSX/pull/1791#discussion_r1860252195
				xRel = std::clamp(curXRel, -127, 127);
				yRel = std::clamp(curYRel, -127, 127);
				curXRel -= xRel;
				curYRel -= yRel;
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
			case SDL_BUTTON_LEFT:
				createMouseStateChange(time, 0, 0, JOY_BUTTONA, 0);
				break;
			case SDL_BUTTON_RIGHT:
				createMouseStateChange(time, 0, 0, JOY_BUTTONB, 0);
				break;
			default:
				// ignore other buttons
				break;
			}
		},
		[&](const MouseButtonUpEvent& e) {
			switch (e.getButton()) {
			case SDL_BUTTON_LEFT:
				createMouseStateChange(time, 0, 0, 0, JOY_BUTTONA);
				break;
			case SDL_BUTTON_RIGHT:
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
	EmuTime::param time, int deltaX, int deltaY, uint8_t press, uint8_t release)
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
	curXRel += ms->getDeltaX();
	curYRel += ms->getDeltaY();
	status = (status & ~ms->getPress()) | ms->getRelease();
}

void Mouse::stopReplay(EmuTime::param time) noexcept
{
	// TODO read actual host mouse button state
	int dx = 0 - curXRel;
	int dy = 0 - curYRel;
	uint8_t release = (JOY_BUTTONA | JOY_BUTTONB) & ~status;
	if ((dx != 0) || (dy != 0) || (release != 0)) {
		createMouseStateChange(time, dx, dy, 0, release);
	}
}

// version 1: Initial version, the variables curXRel, curYRel and status were
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
	             "xrel",      xRel,
	             "yrel",      yRel,
	             "mouseMode", mouseMode);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("curxrel", curXRel,
		             "curyrel", curYRel,
		             "status",  status);
	}
	if (ar.versionBelow(version, 3)) {
		xRel    /= SCALE;
		yRel    /= SCALE;
		curXRel /= SCALE;
		curYRel /= SCALE;

	}
	// no need to serialize absHostX,Y
}
INSTANTIATE_SERIALIZE_METHODS(Mouse);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Mouse, "Mouse");

} // namespace openmsx

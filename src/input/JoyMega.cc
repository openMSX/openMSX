#include "JoyMega.hh"
#include "PluggingController.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "InputEvents.hh"
#include "InputEventGenerator.hh"
#include "StateChange.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "memory.hh"
#include "unreachable.hh"
#include "build-info.hh"

using std::string;
using std::shared_ptr;

namespace openmsx {

#if PLATFORM_ANDROID
static const int THRESHOLD = 32768 / 4;
#else
static const int THRESHOLD = 32768 / 10;
#endif

void JoyMega::registerAll(MSXEventDistributor& eventDistributor,
                           StateChangeDistributor& stateChangeDistributor,
                           PluggingController& controller)
{
#ifdef SDL_JOYSTICK_DISABLED
	(void)eventDistributor;
	(void)stateChangeDistributor;
	(void)controller;
#else
	unsigned numJoysticks = SDL_NumJoysticks();
	for (unsigned i = 0; i < numJoysticks; i++) {
		if (SDL_Joystick* joystick = SDL_JoystickOpen(i)) {
			// Avoid devices that have axes but no buttons, like accelerometers.
			// SDL 1.2.14 in Linux has an issue where it rejects a device from
			// /dev/input/event* if it has no buttons but does not reject a
			// device from /dev/input/js* if it has no buttons, while
			// accelerometers do end up being symlinked as a joystick in
			// practice.
			if (InputEventGenerator::joystickNumButtons(joystick) != 0) {
				controller.registerPluggable(
					make_unique<JoyMega>(
						eventDistributor,
						stateChangeDistributor,
						joystick));
			}
		}
	}
#endif
}

class JoyMegaState final : public StateChange
{
public:
	JoyMegaState() {} // for serialize
	JoyMegaState(EmuTime::param time_, unsigned joyNum_,
	             unsigned press_, unsigned release_)
		: StateChange(time_)
		, joyNum(joyNum_), press(press_), release(release_)
	{
		assert((press != 0) || (release != 0));
		assert((press & release) == 0);
	}
	unsigned getJoystick() const { return joyNum; }
	unsigned getPress()    const { return press; }
	unsigned getRelease()  const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("joyNum", joyNum);
		ar.serialize("press", press);
		ar.serialize("release", release);
	}
private:
	unsigned joyNum;
	unsigned press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, JoyMegaState, "JoyMegaState");

#ifndef SDL_JOYSTICK_DISABLED
// Note: It's OK to open/close the same SDL_Joystick multiple times (we open it
// once per MSX machine). The SDL documentation doesn't state this, but I
// checked the implementation and a SDL_Joystick uses a 'reference count' on
// the open/close calls.
JoyMega::JoyMega(MSXEventDistributor& eventDistributor_,
                 StateChangeDistributor& stateChangeDistributor_,
                 SDL_Joystick* joystick_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, joystick(joystick_)
	, joyNum(SDL_JoystickIndex(joystick_))
	, name("joymegaX") // 'X' is filled in below
	, desc(string(SDL_JoystickName(joyNum)))
	, lastTime(EmuTime::zero)
{
	const_cast<string&>(name)[7] = char('1' + joyNum);
}

JoyMega::~JoyMega()
{
	if (isPluggedIn()) {
		JoyMega::unplugHelper(EmuTime::dummy());
	}
	SDL_JoystickClose(joystick);
}

// Pluggable
const string& JoyMega::getName() const
{
	return name;
}

string_ref JoyMega::getDescription() const
{
	return desc;
}

void JoyMega::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	plugHelper2();
	status = calcInitialState();
	cycle = 0;
	// when mode button is pressed when joystick is plugged in, then
	// act as a 3-button joypad (otherwise 6-button)
	cycleMask = (status & 0x800) ? 7 : 1;
}

void JoyMega::plugHelper2()
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void JoyMega::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// JoystickDevice
byte JoyMega::read(EmuTime::param time)
{
	// See http://segaretro.org/Control_Pad_(Mega_Drive)
	// and http://frs.badcoffee.info/hardware/joymega-en.html
	// for a detailed description of the MegaDrive joystick.
	checkTime(time);
	switch (cycle) {
	case 0: case 2: case 4:
		// up/down/left/right/b/c
		return (status & 0x00f) | ((status & 0x060) >> 1);
	case 1: case 3:
		// up/down/0/0/a/start
		return (status & 0x013) | ((status & 0x080) >> 2);
	case 5:
		// 0/0/0/0/a/start
		return (status & 0x010) | ((status & 0x080) >> 2);
	case 6:
		// z/y/x/mode/b/c
		return ((status & 0x400) >> 10) | // z
		       ((status & 0xA00) >>  8) | // start+y
		       ((status & 0x100) >>  6) | // x
		       ((status & 0x060) >>  1);  // c+b
	case 7:
		// 1/1/1/1/a/start
		return (status & 0x010) | ((status & 0x080) >> 2) | 0x0f;
	default:
		UNREACHABLE; return 0;
	}
}

void JoyMega::write(byte value, EmuTime::param time)
{
	checkTime(time);
	lastTime = time;
	if (((value >> 2) & 1) != (cycle & 1)) {
		cycle = (cycle + 1) & cycleMask;
	}
	assert(((value >> 2) & 1) == (cycle & 1));
}

void JoyMega::checkTime(EmuTime::param time)
{
	if ((time - lastTime) > EmuDuration::usec(1500)) {
		// longer than 1.5ms since last write -> reset cycle
		cycle = 0;
	}
}

static unsigned encodeButton(unsigned button, byte cycleMask)
{
	unsigned n = (cycleMask == 7) ? 7 : 3; // 6- or 3-button mode
	return 1 << (4 + (button & n));
}

unsigned JoyMega::calcInitialState()
{
	unsigned result = 0xfff;
	int xAxis = SDL_JoystickGetAxis(joystick, 0);
	if (xAxis < -THRESHOLD) {
		result &= ~JOY_LEFT;
	} else if (xAxis > THRESHOLD) {
		result &= ~JOY_RIGHT;
	}

	int yAxis = SDL_JoystickGetAxis(joystick, 1);
	if (yAxis < -THRESHOLD) {
		result &= ~JOY_UP;
	} else if (yAxis > THRESHOLD) {
		result &= ~JOY_DOWN;
	}

	int numButtons = InputEventGenerator::joystickNumButtons(joystick);
	for (int button = 0; button < numButtons; ++button) {
		if (InputEventGenerator::joystickGetButton(joystick, button)) {
			result &= ~encodeButton(button, 7);
		}
	}
	return result;
}

// MSXEventListener
void JoyMega::signalEvent(const shared_ptr<const Event>& event, EmuTime::param time)
{
	auto joyEvent = dynamic_cast<const JoystickEvent*>(event.get());
	if (!joyEvent) return;

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	if (joyEvent->getJoystick() != joyNum) return;

	switch (event->getType()) {
	case OPENMSX_JOY_AXIS_MOTION_EVENT: {
		auto& mev = checked_cast<const JoystickAxisMotionEvent&>(*event);
		short value = mev.getValue();
		switch (mev.getAxis() & 1) {
		case JoystickAxisMotionEvent::X_AXIS: // Horizontal
			if (value < -THRESHOLD) {
				// left, not right
				createEvent(time, JOY_LEFT, JOY_RIGHT);
			} else if (value > THRESHOLD) {
				// not left, right
				createEvent(time, JOY_RIGHT, JOY_LEFT);
			} else {
				// not left, not right
				createEvent(time, 0, JOY_LEFT | JOY_RIGHT);
			}
			break;
		case JoystickAxisMotionEvent::Y_AXIS: // Vertical
			if (value < -THRESHOLD) {
				// up, not down
				createEvent(time, JOY_UP, JOY_DOWN);
			} else if (value > THRESHOLD) {
				// not up, down
				createEvent(time, JOY_DOWN, JOY_UP);
			} else {
				// not up, not down
				createEvent(time, 0, JOY_UP | JOY_DOWN);
			}
			break;
		default:
			// ignore other axis
			break;
		}
		break;
	}
	case OPENMSX_JOY_BUTTON_DOWN_EVENT: {
		auto& butEv = checked_cast<const JoystickButtonEvent&>(*event);
		createEvent(time, encodeButton(butEv.getButton(), cycleMask), 0);
		break;
	}
	case OPENMSX_JOY_BUTTON_UP_EVENT: {
		auto& butEv = checked_cast<const JoystickButtonEvent&>(*event);
		createEvent(time, 0, encodeButton(butEv.getButton(), cycleMask));
		break;
	}
	default:
		UNREACHABLE;
	}
}

void JoyMega::createEvent(EmuTime::param time, unsigned press, unsigned release)
{
	unsigned newStatus = (status & ~press) | release;
	createEvent(time, newStatus);
}

void JoyMega::createEvent(EmuTime::param time, unsigned newStatus)
{
	unsigned diff = status ^ newStatus;
	if (!diff) {
		// event won't actually change the status, so ignore it
		return;
	}
	// make sure we create an event with minimal changes
	unsigned press   =    status & diff;
	unsigned release = newStatus & diff;
	stateChangeDistributor.distributeNew(std::make_shared<JoyMegaState>(
		time, joyNum, press, release));
}

// StateChangeListener
void JoyMega::signalStateChange(const shared_ptr<StateChange>& event)
{
	auto js = dynamic_cast<const JoyMegaState*>(event.get());
	if (!js) return;

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	// TODO an alternative is to log events based on the connector instead
	//      of the joystick. That would make it possible to replay on a
	//      different host without an actual SDL joystick connected.
	if (js->getJoystick() != joyNum) return;

	status = (status & ~js->getPress()) | js->getRelease();
}

void JoyMega::stopReplay(EmuTime::param time)
{
	createEvent(time, calcInitialState());
}

template<typename Archive>
void JoyMega::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("lastTime", lastTime);
	ar.serialize("status", status);
	ar.serialize("cycle", cycle);
	ar.serialize("cycleMask", cycleMask);
	if (ar.isLoader()) {
		if (isPluggedIn()) {
			plugHelper2();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(JoyMega);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, JoyMega, "JoyMega");

#endif // SDL_JOYSTICK_DISABLED

} // namespace openmsx

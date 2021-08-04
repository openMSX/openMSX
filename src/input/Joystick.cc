#include "Joystick.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Event.hh"
#include "InputEventGenerator.hh"
#include "StateChange.hh"
#include "TclObject.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "StringOp.hh"
#include "one_of.hh"
#include "xrange.hh"
#include <memory>

using std::string;

namespace openmsx {

void Joystick::registerAll(MSXEventDistributor& eventDistributor,
                           StateChangeDistributor& stateChangeDistributor,
                           CommandController& commandController,
                           GlobalSettings& globalSettings,
                           PluggingController& controller)
{
#ifdef SDL_JOYSTICK_DISABLED
	(void)eventDistributor;
	(void)stateChangeDistributor;
	(void)commandController;
	(void)globalSettings;
	(void)controller;
#else
	for (auto i : xrange(SDL_NumJoysticks())) {
		if (SDL_Joystick* joystick = SDL_JoystickOpen(i)) {
			// Avoid devices that have axes but no buttons, like accelerometers.
			// SDL 1.2.14 in Linux has an issue where it rejects a device from
			// /dev/input/event* if it has no buttons but does not reject a
			// device from /dev/input/js* if it has no buttons, while
			// accelerometers do end up being symlinked as a joystick in
			// practice.
			if (InputEventGenerator::joystickNumButtons(joystick) != 0) {
				controller.registerPluggable(
					std::make_unique<Joystick>(
						eventDistributor,
						stateChangeDistributor,
						commandController,
						globalSettings,
						joystick));
			}
		}
	}
#endif
}


#ifndef SDL_JOYSTICK_DISABLED
static void checkJoystickConfig(Interpreter& interp, TclObject& newValue)
{
	unsigned n = newValue.getListLength(interp);
	if (n & 1) {
		throw CommandException("Need an even number of elements");
	}
	for (unsigned i = 0; i < n; i += 2) {
		std::string_view key  = newValue.getListIndex(interp, i + 0).getString();
		TclObject value = newValue.getListIndex(interp, i + 1);
		if (key != one_of("A", "B", "LEFT", "RIGHT", "UP", "DOWN")) {
			throw CommandException(
				"Invalid MSX joystick action: must be one of "
				"'A', 'B', 'LEFT', 'RIGHT', 'UP', 'DOWN'.");
		}
		for (auto j : xrange(value.getListLength(interp))) {
			std::string_view host = value.getListIndex(interp, j).getString();
			if (!StringOp::startsWith(host, "button") &&
			    !StringOp::startsWith(host, "+axis") &&
			    !StringOp::startsWith(host, "-axis") &&
			    !StringOp::startsWith(host, "L_hat") &&
			    !StringOp::startsWith(host, "R_hat") &&
			    !StringOp::startsWith(host, "U_hat") &&
			    !StringOp::startsWith(host, "D_hat")) {
				throw CommandException(
					"Invalid host joystick action: must be "
					"one of 'button<N>', '+axis<N>', '-axis<N>', "
					"'L_hat<N>', 'R_hat<N>', 'U_hat<N>', 'D_hat<N>'");
			}
		}
	}
}

[[nodiscard]] static string getJoystickName(unsigned joyNum)
{
	return strCat("joystick", char('1' + joyNum));
}

[[nodiscard]] static TclObject getConfigValue(SDL_Joystick* joystick)
{
	TclObject listA, listB;
	for (auto i : xrange(InputEventGenerator::joystickNumButtons(joystick))) {
		auto button = tmpStrCat("button", i);
		//std::string_view button = tmp;
		if (i & 1) {
			listB.addListElement(button);
		} else {
			listA.addListElement(button);
		}
	}
	TclObject value;
	value.addDictKeyValues("LEFT",  makeTclList("-axis0", "L_hat0"),
	                       "RIGHT", makeTclList("+axis0", "R_hat0"),
	                       "UP",    makeTclList("-axis1", "U_hat0"),
	                       "DOWN",  makeTclList("+axis1", "D_hat0"),
	                       "A",     listA,
	                       "B",     listB);
	return value;
}

// Note: It's OK to open/close the same SDL_Joystick multiple times (we open it
// once per MSX machine). The SDL documentation doesn't state this, but I
// checked the implementation and a SDL_Joystick uses a 'reference count' on
// the open/close calls.
Joystick::Joystick(MSXEventDistributor& eventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   CommandController& commandController,
                   GlobalSettings& globalSettings,
                   SDL_Joystick* joystick_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, joystick(joystick_)
	, joyNum(SDL_JoystickInstanceID(joystick_))
	, deadSetting(globalSettings.getJoyDeadzoneSetting(joyNum))
	, name(getJoystickName(joyNum))
	, desc(SDL_JoystickName(joystick_))
	, configSetting(commandController, tmpStrCat(name, "_config"),
		"joystick configuration", getConfigValue(joystick).getString())
{
	auto& interp = commandController.getInterpreter();
	configSetting.setChecker([&interp](TclObject& newValue) {
		checkJoystickConfig(interp, newValue); });

	pin8 = false; // avoid UMR
}

Joystick::~Joystick()
{
	if (isPluggedIn()) {
		Joystick::unplugHelper(EmuTime::dummy());
	}
	if (joystick) {
		SDL_JoystickClose(joystick);
	}
}

// Pluggable
std::string_view Joystick::getName() const
{
	return name;
}

std::string_view Joystick::getDescription() const
{
	return desc;
}

void Joystick::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	if (!joystick) {
		throw PlugException("Failed to open joystick device");
	}
	plugHelper2();
	status = calcState();
}

void Joystick::plugHelper2()
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void Joystick::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// JoystickDevice
byte Joystick::read(EmuTime::param /*time*/)
{
	return pin8 ? 0x3F : status;
}

void Joystick::write(byte value, EmuTime::param /*time*/)
{
	pin8 = (value & 0x04) != 0;
}

byte Joystick::calcState()
{
	byte result = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	              JOY_BUTTONA | JOY_BUTTONB;
	if (joystick) {
		int threshold = (deadSetting.getInt() * 32768) / 100;
		auto& interp = configSetting.getInterpreter();
		const auto& dict   = configSetting.getValue();
		if (getState(interp, dict, "A"    , threshold)) result &= ~JOY_BUTTONA;
		if (getState(interp, dict, "B"    , threshold)) result &= ~JOY_BUTTONB;
		if (getState(interp, dict, "UP"   , threshold)) result &= ~JOY_UP;
		if (getState(interp, dict, "DOWN" , threshold)) result &= ~JOY_DOWN;
		if (getState(interp, dict, "LEFT" , threshold)) result &= ~JOY_LEFT;
		if (getState(interp, dict, "RIGHT", threshold)) result &= ~JOY_RIGHT;
	}
	return result;
}

bool Joystick::getState(Interpreter& interp, const TclObject& dict,
                        std::string_view key, int threshold)
{
	try {
		const auto& list = dict.getDictValue(interp, key);
		for (auto i : xrange(list.getListLength(interp))) {
			const auto& elem = list.getListIndex(interp, i).getString();
			if (StringOp::startsWith(elem, "button")) {
				if (auto n = StringOp::stringToBase<10, unsigned>(elem.substr(6))) {
					if (InputEventGenerator::joystickGetButton(joystick, *n)) {
						return true;
					}
				}
			} else if (StringOp::startsWith(elem, "+axis")) {
				if (auto n = StringOp::stringToBase<10, unsigned>(elem.substr(5))) {
					if (SDL_JoystickGetAxis(joystick, *n) > threshold) {
						return true;
					}
				}
			} else if (StringOp::startsWith(elem, "-axis")) {
				if (auto n = StringOp::stringToBase<10, unsigned>(elem.substr(5))) {
					if (SDL_JoystickGetAxis(joystick, *n) < -threshold) {
						return true;
					}
				}
			} else if (StringOp::startsWith(elem, "L_hat")) {
				if (auto n = StringOp::stringToBase<10, unsigned>(elem.substr(5))) {
					if (SDL_JoystickGetHat(joystick, *n) & SDL_HAT_LEFT) {
						return true;
					}
				}
			} else if (StringOp::startsWith(elem, "R_hat")) {
				if (auto n = StringOp::stringToBase<10, unsigned>(elem.substr(5))) {
					if (SDL_JoystickGetHat(joystick, *n) & SDL_HAT_RIGHT) {
						return true;
					}
				}
			} else if (StringOp::startsWith(elem, "U_hat")) {
				if (auto n = StringOp::stringToBase<10, unsigned>(elem.substr(5))) {
					if (SDL_JoystickGetHat(joystick, *n) & SDL_HAT_UP) {
						return true;
					}
				}
			} else if (StringOp::startsWith(elem, "D_hat")) {
				if (auto n = StringOp::stringToBase<10, unsigned>(elem.substr(5))) {
					if (SDL_JoystickGetHat(joystick, *n) & SDL_HAT_DOWN) {
						return true;
					}
				}
			}
		}
	} catch (...) {
		// Error, in getListLength() or getListIndex().
		// In either case we can't do anything about it here, so ignore.
	}
	return false;
}

// MSXEventListener
void Joystick::signalMSXEvent(const Event& event,
                              EmuTime::param time) noexcept
{
	const auto* joyEvent = get_if<JoystickEvent>(event);
	if (!joyEvent) return;

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	if (joyEvent->getJoystick() != joyNum) return;

	// TODO: Currently this recalculates the whole joystick state. It might
	// be possible to implement this more efficiently by using the specific
	// event information. Though that's not trivial because e.g. multiple
	// host buttons can map to the same MSX button. Also calcState()
	// involves some string processing. It might be possible to only parse
	// the config once (per setting change). Though this solution is likely
	// good enough.
	createEvent(time, calcState());
}

void Joystick::createEvent(EmuTime::param time, byte newStatus)
{
	byte diff = status ^ newStatus;
	if (!diff) {
		// event won't actually change the status, so ignore it
		return;
	}
	// make sure we create an event with minimal changes
	byte press   =    status & diff;
	byte release = newStatus & diff;
	stateChangeDistributor.distributeNew<JoyState>(
		time, joyNum, press, release);
}

// StateChangeListener
void Joystick::signalStateChange(const StateChange& event)
{
	const auto* js = std::get_if<JoyState>(&event);
	if (!js) return;

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	// TODO an alternative is to log events based on the connector instead
	//      of the joystick. That would make it possible to replay on a
	//      different host without an actual SDL joystick connected.
	if (js->getJoystick() != joyNum) return;

	status = (status & ~js->getPress()) | js->getRelease();
}

void Joystick::stopReplay(EmuTime::param time) noexcept
{
	createEvent(time, calcState());
}

// version 1: Initial version, the variable status was not serialized.
// version 2: Also serialize the above variable, this is required for
//            record/replay, see comment in Keyboard.cc for more details.
template<typename Archive>
void Joystick::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("status", status);
	}
	if constexpr (Archive::IS_LOADER) {
		if (joystick && isPluggedIn()) {
			plugHelper2();
		}
	}
	// no need to serialize 'pin8' it's automatically restored via write()
}
INSTANTIATE_SERIALIZE_METHODS(Joystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Joystick, "Joystick");

#endif // SDL_JOYSTICK_DISABLED

} // namespace openmsx

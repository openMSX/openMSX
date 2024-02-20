#include "KeyJoystick.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Event.hh"
#include "StateChange.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

namespace openmsx {

static std::string_view nameForId(KeyJoystick::ID id)
{
	switch (id) {
		case KeyJoystick::ID1: return "keyjoystick1";
		case KeyJoystick::ID2: return "keyjoystick2";
		default: return "unknown-keyjoystick";
	}
}

class KeyJoyState final : public StateChange
{
public:
	KeyJoyState() = default; // for serialize
	KeyJoyState(EmuTime::param time_, KeyJoystick::ID id_,
	            uint8_t press_, uint8_t release_)
		: StateChange(time_)
		, id(id_), press(press_), release(release_) {}
	[[nodiscard]] auto getId() const { return id; }
	[[nodiscard]] uint8_t getPress()   const { return press; }
	[[nodiscard]] uint8_t getRelease() const { return release; }
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		// for backwards compatibility serialize 'id' as 'name'
		std::string name = Archive::IS_LOADER ? "" : std::string(nameForId(id));
		ar.serialize("name",    name,
		             "press",   press,
		             "release", release);
		if constexpr (Archive::IS_LOADER) {
			id = (name == nameForId(KeyJoystick::ID1)) ? KeyJoystick::ID1
			   : (name == nameForId(KeyJoystick::ID2)) ? KeyJoystick::ID2
			   :                                         KeyJoystick::UNKNOWN;
		}
	}

private:
	KeyJoystick::ID id;
	uint8_t press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, KeyJoyState, "KeyJoyState");

KeyJoystick::KeyJoystick(CommandController& commandController,
                         MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_,
                         ID id_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, up   (commandController, tmpStrCat(nameForId(id_), ".up"),
		"key for direction up",    SDLKey::createDown(SDLK_UP))
	, down (commandController, tmpStrCat(nameForId(id_), ".down"),
		"key for direction down",  SDLKey::createDown(SDLK_DOWN))
	, left (commandController, tmpStrCat(nameForId(id_), ".left"),
		"key for direction left",  SDLKey::createDown(SDLK_LEFT))
	, right(commandController, tmpStrCat(nameForId(id_), ".right"),
		"key for direction right", SDLKey::createDown(SDLK_RIGHT))
	, trigA(commandController, tmpStrCat(nameForId(id_), ".triga"),
		"key for trigger A",       SDLKey::createDown(SDLK_SPACE))
	, trigB(commandController, tmpStrCat(nameForId(id_), ".trigb"),
		"key for trigger B",       SDLKey::createDown(SDLK_m))
	, id(id_)
	, status(JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB)
{
}

KeyJoystick::~KeyJoystick()
{
	if (isPluggedIn()) {
		KeyJoystick::unplugHelper(EmuTime::dummy());
	}
}


// Pluggable
std::string_view KeyJoystick::getName() const
{
	return nameForId(id);
}

std::string_view KeyJoystick::getDescription() const
{
	return "Key-Joystick, use your keyboard to emulate an MSX joystick. "
		"See manual for information on how to configure this.";
}

void KeyJoystick::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void KeyJoystick::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// KeyJoystickDevice
uint8_t KeyJoystick::read(EmuTime::param /*time*/)
{
	return pin8 ? 0x3F : status;
}

void KeyJoystick::write(uint8_t value, EmuTime::param /*time*/)
{
	pin8 = (value & 0x04) != 0;
}


// MSXEventListener
void KeyJoystick::signalMSXEvent(const Event& event,
                                 EmuTime::param time) noexcept
{
	uint8_t press = 0;
	uint8_t release = 0;
	auto getKey = [&](const KeyEvent& e) {
		auto key = e.getKeyCode();
		if      (key == up   .getKey().sym.sym) return JOY_UP;
		else if (key == down .getKey().sym.sym) return JOY_DOWN;
		else if (key == left .getKey().sym.sym) return JOY_LEFT;
		else if (key == right.getKey().sym.sym) return JOY_RIGHT;
		else if (key == trigA.getKey().sym.sym) return JOY_BUTTONA;
		else if (key == trigB.getKey().sym.sym) return JOY_BUTTONB;
		else                            return uint8_t(0);
	};
	visit(overloaded{
		[&](const KeyDownEvent& e) { press   = getKey(e); },
		[&](const KeyUpEvent&   e) { release = getKey(e); },
		[](const EventBase&) { /*ignore*/ }
	}, event);

	if (((status & ~press) | release) != status) {
		stateChangeDistributor.distributeNew<KeyJoyState>(
			time, id, press, release);
	}
}

// StateChangeListener
void KeyJoystick::signalStateChange(const StateChange& event)
{
	const auto* kjs = dynamic_cast<const KeyJoyState*>(&event);
	if (!kjs) return;
	if (kjs->getId() != id) return;

	status = (status & ~kjs->getPress()) | kjs->getRelease();
}

void KeyJoystick::stopReplay(EmuTime::param time) noexcept
{
	// TODO read actual host key state
	uint8_t newStatus = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                    JOY_BUTTONA | JOY_BUTTONB;
	if (newStatus != status) {
		uint8_t release = newStatus & ~status;
		stateChangeDistributor.distributeNew<KeyJoyState>(
			time, id, uint8_t(0), release);
	}
}


// version 1: Initial version, the variable status was not serialized.
// version 2: Also serialize the above variable, this is required for
//            record/replay, see comment in Keyboard.cc for more details.
template<typename Archive>
void KeyJoystick::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("status", status);
	}
	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
	// no need to serialize 'pin8'
}
INSTANTIATE_SERIALIZE_METHODS(KeyJoystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, KeyJoystick, "KeyJoystick");

} // namespace openmsx

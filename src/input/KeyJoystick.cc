#include "KeyJoystick.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Event.hh"
#include "StateChange.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

using std::string;

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
	            byte press_, byte release_)
		: StateChange(time_)
		, id(id_), press(press_), release(release_) {}
	[[nodiscard]] auto getId() const { return id; }
	[[nodiscard]] byte getPress()   const { return press; }
	[[nodiscard]] byte getRelease() const { return release; }
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		// for backwards compatibility serialize 'id' as 'name'
		std::string name = ar.IS_LOADER ? "" : std::string(nameForId(id));
		ar.serialize("name",    name,
		             "press",   press,
		             "release", release);
		if constexpr (ar.IS_LOADER) {
			id = (name == nameForId(KeyJoystick::ID1)) ? KeyJoystick::ID1
			   : (name == nameForId(KeyJoystick::ID2)) ? KeyJoystick::ID2
			   :                                         KeyJoystick::UNKNOWN;
		}
	}

private:
	KeyJoystick::ID id;
	byte press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, KeyJoyState, "KeyJoyState");

KeyJoystick::KeyJoystick(CommandController& commandController,
                         MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_,
                         ID id_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, up   (commandController, tmpStrCat(nameForId(id_), ".up"),
		"key for direction up",    Keys::K_UP)
	, down (commandController, tmpStrCat(nameForId(id_), ".down"),
		"key for direction down",  Keys::K_DOWN)
	, left (commandController, tmpStrCat(nameForId(id_), ".left"),
		"key for direction left",  Keys::K_LEFT)
	, right(commandController, tmpStrCat(nameForId(id_), ".right"),
		"key for direction right", Keys::K_RIGHT)
	, trigA(commandController, tmpStrCat(nameForId(id_), ".triga"),
		"key for trigger A",       Keys::K_SPACE)
	, trigB(commandController, tmpStrCat(nameForId(id_), ".trigb"),
		"key for trigger B",       Keys::K_M)
	, id(id_)
{
	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
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
byte KeyJoystick::read(EmuTime::param /*time*/)
{
	return pin8 ? 0x3F : status;
}

void KeyJoystick::write(byte value, EmuTime::param /*time*/)
{
	pin8 = (value & 0x04) != 0;
}


// MSXEventListener
void KeyJoystick::signalMSXEvent(const Event& event,
                                 EmuTime::param time) noexcept
{
	byte press = 0;
	byte release = 0;
	auto getKey = [&](const KeyEvent& e) {
		auto key = static_cast<Keys::KeyCode>(
			int(e.getKeyCode()) & int(Keys::K_MASK));
		if      (key == up   .getKey()) return JOY_UP;
		else if (key == down .getKey()) return JOY_DOWN;
		else if (key == left .getKey()) return JOY_LEFT;
		else if (key == right.getKey()) return JOY_RIGHT;
		else if (key == trigA.getKey()) return JOY_BUTTONA;
		else if (key == trigB.getKey()) return JOY_BUTTONB;
		else                            return 0;
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
	byte newStatus = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                 JOY_BUTTONA | JOY_BUTTONB;
	if (newStatus != status) {
		byte release = newStatus & ~status;
		stateChangeDistributor.distributeNew<KeyJoyState>(
			time, id, 0, release);
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

#include "KeyJoystick.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "Event.hh"
#include "StateChange.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

using std::string;
using std::shared_ptr;

namespace openmsx {

class KeyJoyState final : public StateChange
{
public:
	KeyJoyState() = default; // for serialize
	KeyJoyState(EmuTime::param time_, string name_,
	            byte press_, byte release_)
		: StateChange(time_)
		, name(std::move(name_)), press(press_), release(release_) {}
	[[nodiscard]] const string& getName() const { return name; }
	[[nodiscard]] byte getPress()   const { return press; }
	[[nodiscard]] byte getRelease() const { return release; }
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("name",    name,
		             "press",   press,
		             "release", release);
	}

private:
	string name;
	byte press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, KeyJoyState, "KeyJoyState");

KeyJoystick::KeyJoystick(CommandController& commandController,
                         MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_,
                         std::string name_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, name(std::move(name_))
	, up   (commandController, tmpStrCat(name, ".up"),
		"key for direction up",    Keys::K_UP)
	, down (commandController, tmpStrCat(name, ".down"),
		"key for direction down",  Keys::K_DOWN)
	, left (commandController, tmpStrCat(name, ".left"),
		"key for direction left",  Keys::K_LEFT)
	, right(commandController, tmpStrCat(name, ".right"),
		"key for direction right", Keys::K_RIGHT)
	, trigA(commandController, tmpStrCat(name, ".triga"),
		"key for trigger A",       Keys::K_SPACE)
	, trigB(commandController, tmpStrCat(name, ".trigb"),
		"key for trigger B",       Keys::K_M)
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
	return name;
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
		stateChangeDistributor.distributeNew(std::make_shared<KeyJoyState>(
			time, name, press, release));
	}
}

// StateChangeListener
void KeyJoystick::signalStateChange(const shared_ptr<StateChange>& event)
{
	const auto* kjs = dynamic_cast<const KeyJoyState*>(event.get());
	if (!kjs) return;
	if (kjs->getName() != name) return;

	status = (status & ~kjs->getPress()) | kjs->getRelease();
}

void KeyJoystick::stopReplay(EmuTime::param time) noexcept
{
	// TODO read actual host key state
	byte newStatus = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                 JOY_BUTTONA | JOY_BUTTONB;
	if (newStatus != status) {
		byte release = newStatus & ~status;
		stateChangeDistributor.distributeNew(std::make_shared<KeyJoyState>(
			time, name, 0, release));
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

#include "MSXJoystick.hh"

#include "CommandController.hh"
#include "Event.hh"
#include "IntegerSetting.hh"
#include "JoystickManager.hh"
#include "MSXEventDistributor.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

#include "join.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <algorithm>

namespace openmsx {

class MSXJoyState final : public StateChange
{
public:
	MSXJoyState() = default; // for serialize
	MSXJoyState(EmuTime::param time_, uint8_t id_,
	            uint8_t press_, uint8_t release_)
		: StateChange(time_), id(id_)
		, press(press_), release(release_) {}

	[[nodiscard]] auto getId()      const { return id; }
	[[nodiscard]] auto getPress()   const { return press; }
	[[nodiscard]] auto getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/) {
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("id",      id,
		             "press",   press,
		             "release", release);
	}

private:
	uint8_t id, press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, MSXJoyState, "MSXJoyState");

TclObject MSXJoystick::getDefaultConfig(JoystickId joyId, const JoystickManager& joystickManager)
{
	auto buttons = joystickManager.getNumButtons(joyId);
	if (!buttons) return {};

	TclObject listA, listB;
	auto joy = joyId.str();
	for (auto b : xrange(*buttons)) {
		((b & 1) ? listB : listA).addListElement(tmpStrCat(joy, " button", b));
	}
	return TclObject(TclObject::MakeDictTag{},
		"UP",    makeTclList(tmpStrCat(joy, " -axis1"), tmpStrCat(joy, " hat0 up")),
		"DOWN",  makeTclList(tmpStrCat(joy, " +axis1"), tmpStrCat(joy, " hat0 down")),
		"LEFT",  makeTclList(tmpStrCat(joy, " -axis0"), tmpStrCat(joy, " hat0 left")),
		"RIGHT", makeTclList(tmpStrCat(joy, " +axis0"), tmpStrCat(joy, " hat0 right")),
		"A",     listA,
		"B",     listB);
}

MSXJoystick::MSXJoystick(CommandController& commandController_,
                         MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_,
                         JoystickManager& joystickManager_,
                         uint8_t id_)
	: commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, joystickManager(joystickManager_)
	, configSetting(commandController, tmpStrCat("msxjoystick", id_, "_config"),
		"msxjoystick mapping configuration", getDefaultConfig(JoystickId(id_ - 1), joystickManager).getString())
	, description(strCat("MSX joystick ", id_, ". Mapping is fully configurable."))
	, id(id_)
{
	configSetting.setChecker([this](const TclObject& newValue) {
		this->checkJoystickConfig(newValue); });
	// fill in 'bindings'
	checkJoystickConfig(configSetting.getValue());
}

MSXJoystick::~MSXJoystick()
{
	if (isPluggedIn()) {
		MSXJoystick::unplugHelper(EmuTime::dummy());
	}
}

void MSXJoystick::checkJoystickConfig(const TclObject& newValue)
{
	std::array<std::vector<BooleanInput>, 6> newBindings;

	auto& interp = commandController.getInterpreter();
	auto n = newValue.getListLength(interp);
	if (n & 1) {
		throw CommandException("Need an even number of elements");
	}
	for (decltype(n) i = 0; i < n; i += 2) {
		static constexpr std::array<std::string_view, 6> keys = {
			// order is important!
			"UP", "DOWN", "LEFT", "RIGHT", "A", "B"
		};
		std::string_view key  = newValue.getListIndex(interp, i + 0).getString();
		auto it = std::ranges::find(keys, key);
		if (it == keys.end()) {
			throw CommandException(
				"Invalid key: must be one of ", join(keys, ", "));
		}
		auto idx = std::distance(keys.begin(), it);

		TclObject value = newValue.getListIndex(interp, i + 1);
		for (auto j : xrange(value.getListLength(interp))) {
			std::string_view val = value.getListIndex(interp, j).getString();
			auto bind = parseBooleanInput(val);
			if (!bind) {
				throw CommandException("Invalid binding: ", val);
			}
			newBindings[idx].push_back(*bind);
		}
	}

	// only change current bindings when parsing was fully successful
	copy_to_range(newBindings, bindings);
}

// Pluggable
std::string_view MSXJoystick::getName() const
{
	switch (id) {
		case 1: return "msxjoystick1";
		case 2: return "msxjoystick2";
		default: UNREACHABLE;
	}
}

std::string_view MSXJoystick::getDescription() const
{
	return description;
}

void MSXJoystick::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void MSXJoystick::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// MSXJoystickDevice
uint8_t MSXJoystick::read(EmuTime::param /*time*/)
{
	return pin8 ? 0x3F : status;
}

void MSXJoystick::write(uint8_t value, EmuTime::param /*time*/)
{
	pin8 = (value & 0x04) != 0;
}


// MSXEventListener
void MSXJoystick::signalMSXEvent(const Event& event,
                                 EmuTime::param time) noexcept
{
	uint8_t press = 0;
	uint8_t release = 0;

	auto getJoyDeadZone = [&](JoystickId joyId) {
		const auto* setting = joystickManager.getJoyDeadZoneSetting(joyId);
		return setting ? setting->getInt() : 0;
	};
	for (int i : xrange(6)) {
		for (const auto& binding : bindings[i]) {
			if (auto onOff = match(binding, event, getJoyDeadZone)) {
				(*onOff ? press : release) |= 1 << i;
			}
		}
	}

	if (((status & ~press) | release) != status) {
		stateChangeDistributor.distributeNew<MSXJoyState>(
			time, id, press, release);
	}
}

// StateChangeListener
void MSXJoystick::signalStateChange(const StateChange& event)
{
	const auto* kjs = dynamic_cast<const MSXJoyState*>(&event);
	if (!kjs) return;
	if (kjs->getId() != id) return;

	status = (status & ~kjs->getPress()) | kjs->getRelease();
}

void MSXJoystick::stopReplay(EmuTime::param time) noexcept
{
	uint8_t newStatus = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                    JOY_BUTTONA | JOY_BUTTONB;
	if (newStatus != status) {
		uint8_t release = newStatus & ~status;
		stateChangeDistributor.distributeNew<MSXJoyState>(
			time, id, uint8_t(0), release);
	}
}


template<typename Archive>
void MSXJoystick::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("status", status);
	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
	// no need to serialize 'pin8'
}
INSTANTIATE_SERIALIZE_METHODS(MSXJoystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MSXJoystick, "MSXJoystick");

} // namespace openmsx

#include "MSXJoystick.hh"

#include "CommandController.hh"
#include "Event.hh"
#include "GlobalSettings.hh"
#include "MSXEventDistributor.hh"
#include "ranges.hh"
#include "StateChangeDistributor.hh"
#include "StateChange.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

#include "join.hh"
#include "unreachable.hh"
#include "xrange.hh"

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

[[nodiscard]] static TclObject getDefaultConfig(uint8_t /*id*/)
{
	TclObject result;
	result.addDictKeyValues("UP",    makeTclList("keyb Up"),
	                        "DOWN",  makeTclList("keyb Down"),
	                        "LEFT",  makeTclList("keyb Left"),
	                        "RIGHT", makeTclList("keyb Right"),
	                        "A",     makeTclList("keyb Space"),
	                        "B",     makeTclList("keyb M"));
	return result;
}
MSXJoystick::MSXJoystick(CommandController& commandController_,
                         MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_,
                         GlobalSettings& globalSettings_,
                         uint8_t id_)
	: commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, globalSettings(globalSettings_)
	, configSetting(commandController, tmpStrCat("msxjoystick", id_, "_config"),
		"msxjoystick mapping configuration", getDefaultConfig(id_).getString())
	, id(id_)
	, status(JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB)
{
	configSetting.setChecker([this](TclObject& newValue) {
		this->checkJoystickConfig(newValue); });
	// fill in 'bindings'
	checkJoystickConfig(const_cast<TclObject&>(configSetting.getValue()));
}

MSXJoystick::~MSXJoystick()
{
	if (isPluggedIn()) {
		MSXJoystick::unplugHelper(EmuTime::dummy());
	}
}

void MSXJoystick::checkJoystickConfig(TclObject& newValue)
{
	std::array<std::vector<BooleanInput>, 6> newBindings;

	auto& interp = commandController.getInterpreter();
	unsigned n = newValue.getListLength(interp);
	if (n & 1) {
		throw CommandException("Need an even number of elements");
	}
	for (unsigned i = 0; i < n; i += 2) {
		static constexpr std::array<std::string_view, 6> keys = {
			// order is important!
			"UP", "DOWN", "LEFT", "RIGHT", "A", "B"
		};
		std::string_view key  = newValue.getListIndex(interp, i + 0).getString();
		auto it = ranges::find(keys, key);
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
	ranges::copy(newBindings, bindings);
}

// Pluggable
std::string_view MSXJoystick::getName() const
{
	switch (id) {
		case 1: return "msxjoystick1";
		case 2: return "msxjoystick2";
		default: UNREACHABLE; return "";
	}
}

std::string_view MSXJoystick::getDescription() const
{
	return "MSX-Joystick. See manual for information on how to configure this.";
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

	auto getJoyDeadZone = [&](int joystick) {
		return globalSettings.getJoyDeadZoneSetting(joystick).getInt();
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

#include "ColecoJoystick.hh"

#include "CommandController.hh"
#include "Event.hh"
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

TclObject ColecoJoystick::getDefaultConfig(JoystickId joyId, const JoystickManager& joystickManager)
{
	auto buttons = joystickManager.getNumButtons(joyId);
	if (!buttons) return {};

	auto joy = joyId.str();
	TclObject result(TclObject::MakeDictTag{},
		"UP",    makeTclList(tmpStrCat(joy, " -axis1"), tmpStrCat(joy, " hat0 up")),
		"DOWN",  makeTclList(tmpStrCat(joy, " +axis1"), tmpStrCat(joy, " hat0 down")),
		"LEFT",  makeTclList(tmpStrCat(joy, " -axis0"), tmpStrCat(joy, " hat0 left")),
		"RIGHT", makeTclList(tmpStrCat(joy, " +axis0"), tmpStrCat(joy, " hat0 right")));
	// Fire buttons first, then the keypad keys most games use: '1'/'2' to
	// select the game variant, '*'/'#' to restart or pause. The other keys
	// get an explicitly empty list: they exist, but have no default binding.
	static constexpr std::array<std::string_view, 6> buttonKeys = {
		"L", "R", "1", "2", "*", "#"
	};
	for (auto key : subspan(keyNames, 4)) { // skip the directions
		auto it = std::ranges::find(buttonKeys, key);
		auto b = std::distance(buttonKeys.begin(), it);
		result.addDictKeyValue(key, (it != buttonKeys.end() && size_t(b) < *buttons)
			? makeTclList(tmpStrCat(joy, " button", b))
			: makeTclList());
	}
	return result;
}

ColecoJoystick::ColecoJoystick(CommandController& commandController_,
                               MSXEventDistributor& eventDistributor_,
                               StateChangeDistributor& stateChangeDistributor_,
                               JoystickManager& joystickManager_,
                               uint8_t id_)
	: commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, joystickManager(joystickManager_)
	, configSetting(commandController, tmpStrCat("colecojoystick", id_, "_config"),
		"colecojoystick mapping configuration", getDefaultConfig(JoystickId(id_ - 1), joystickManager).getString())
	, description(strCat("ColecoVision controller ", id_, ". Mapping is fully configurable."))
	, id(id_)
{
	configSetting.setChecker([this](const TclObject& newValue) {
		this->checkJoystickConfig(newValue); });
	try {
		// fill in 'bindings'
		checkJoystickConfig(configSetting.getValue());
	} catch (MSXException& /*e*/) {
		configSetting.setValue(getDefaultConfig(JoystickId(id - 1), joystickManager));
	}
}

ColecoJoystick::~ColecoJoystick()
{
	if (isPluggedIn()) {
		ColecoJoystick::unplugHelper(EmuTime::dummy());
	}
}

void ColecoJoystick::checkJoystickConfig(const TclObject& newValue)
{
	std::array<std::vector<BooleanInput>, NUM_BUTTONS> newBindings;

	auto& interp = commandController.getInterpreter();
	auto n = newValue.getListLength(interp);
	if (n & 1) {
		throw CommandException("Need an even number of elements");
	}
	for (decltype(n) i = 0; i < n; i += 2) {
		std::string_view key  = newValue.getListIndex(interp, i + 0).getString();
		auto it = std::ranges::find(keyNames, key);
		if (it == keyNames.end()) {
			throw CommandException(
				"Invalid key: must be one of ", join(keyNames, ", "));
		}
		auto idx = std::distance(keyNames.begin(), it);

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
zstring_view ColecoJoystick::getName() const
{
	switch (id) {
		case 1: return "colecojoystick1";
		case 2: return "colecojoystick2";
		default: UNREACHABLE;
	}
}

zstring_view ColecoJoystick::getDescription() const
{
	return description;
}

void ColecoJoystick::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void ColecoJoystick::unplugHelper(EmuTime /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// JoystickDevice
uint8_t ColecoJoystick::read(EmuTime /*time*/)
{
	return pin8 ? 0x3F : (status & 0x3F);
}

void ColecoJoystick::write(uint8_t value, EmuTime /*time*/)
{
	pin8 = (value & 0x04) != 0;
}


// MSXEventListener
void ColecoJoystick::signalMSXEvent(const Event& event,
                                    EmuTime time) noexcept
{
	unsigned press = 0;
	unsigned release = 0;

	auto getJoyDeadThreshold = [&](JoystickId joyId) {
		return joystickManager.getJoyDeadThreshold(joyId);
	};
	for (auto i : xrange(NUM_BUTTONS)) {
		for (const auto& binding : bindings[i]) {
			if (auto onOff = match(binding, event, getJoyDeadThreshold)) {
				(*onOff ? press : release) |= 1u << i;
			}
		}
	}

	if (((status & ~press) | release) != status) {
		stateChangeDistributor.distributeNew<ColecoJoyState>(
			time, id, press, release);
	}
}

// StateChangeListener
void ColecoJoystick::signalStateChange(const StateChange& event)
{
	const auto* cjs = std::get_if<ColecoJoyState>(&event);
	if (!cjs) return;
	if (cjs->getId() != id) return;

	status = (status & ~cjs->getPress()) | cjs->getRelease();
}

void ColecoJoystick::stopReplay(EmuTime time) noexcept
{
	if (status != ALL_RELEASED) {
		unsigned release = ALL_RELEASED & ~status;
		stateChangeDistributor.distributeNew<ColecoJoyState>(
			time, id, 0u, release);
	}
}


template<typename Archive>
void ColecoJoystick::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("status", status);
	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
	// no need to serialize 'pin8'
}
INSTANTIATE_SERIALIZE_METHODS(ColecoJoystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, ColecoJoystick, "ColecoController");

} // namespace openmsx

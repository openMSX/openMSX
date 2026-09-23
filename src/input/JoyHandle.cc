#include "JoyHandle.hh"

#include "Clock.hh"
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

#include <cmath>

namespace openmsx {

using namespace std::literals;

TclObject JoyHandle::getDefaultConfig(JoystickId joyId, const JoystickManager& joystickManager)
{
	auto buttons = joystickManager.getNumButtons(joyId);
	if (!buttons) return {};

	TclObject listA, listB;
	auto joy = joyId.str();
	for (auto b : xrange(*buttons)) {
		((b & 1) ? listB : listA).addListElement(tmpStrCat(joy, " button", b));
	}
	return TclObject(TclObject::MakeDictTag{},
		"UP",     makeTclList(tmpStrCat(joy, " hat0 up")),
		"DOWN",   makeTclList(tmpStrCat(joy, " hat0 down")),
		"LEFT",   makeTclList(tmpStrCat(joy, " hat0 left")),
		"RIGHT",  makeTclList(tmpStrCat(joy, " hat0 right")),
		"A",      listA,
		"B",      listB,
		"WHEEL",  makeTclList(tmpStrCat(joy, " axis0")));
}

JoyHandle::JoyHandle(CommandController& commandController_,
                         MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_,
                         JoystickManager& joystickManager_,
                         uint8_t id_)
	: commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, joystickManager(joystickManager_)
	, configSetting(commandController, tmpStrCat("joyhandle", id_, "_config"),
		"joyhandle mapping configuration", getDefaultConfig(JoystickId(id_ - 1), joystickManager).getString())
	, description(strCat("Panasonic FS-JH1 Joy Handle ", id_, ". Mapping is fully configurable."))
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

JoyHandle::~JoyHandle()
{
	if (isPluggedIn()) {
		JoyHandle::unplugHelper(EmuTime::dummy());
	}
}

void JoyHandle::checkJoystickConfig(const TclObject& newValue)
{
	std::array<std::vector<BooleanInput>, 6> newBindings;
	std::vector<AnalogStatus> newWheelBindings;

	auto& interp = commandController.getInterpreter();
	unsigned n = newValue.getListLength(interp);
	if (n & 1) {
		throw CommandException("Need an even number of elements");
	}
	for (unsigned i = 0; i < n; i += 2) {
		static constexpr std::array keys = {
			// order is important!
			"UP"sv, "DOWN"sv, "LEFT"sv, "RIGHT"sv, "A"sv, "B"sv, "WHEEL"sv
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
			if (key == "WHEEL") {
				auto bind = parseAnalogInput(val);
				if (!bind) {
					throw CommandException("Invalid binding: ", val);
				}
				newWheelBindings.emplace_back(*bind, 0.0f);
			} else {
				auto bind = parseBooleanInput(val);
				if (!bind) {
					throw CommandException("Invalid binding: ", val);
				}
				newBindings[idx].push_back(*bind);
			}
		}
	}

	// only change current bindings when parsing was fully successful
	copy_to_range(newBindings, bindings);
	wheelBindings = std::move(newWheelBindings);
}

// Pluggable
zstring_view JoyHandle::getName() const
{
	switch (id) {
		case 1: return "joyhandle1";
		case 2: return "joyhandle2";
		default: UNREACHABLE;
	}
}

zstring_view JoyHandle::getDescription() const
{
	return description;
}

void JoyHandle::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);

	analogValue = 0.0f;
	resetWheelCache();
}

void JoyHandle::unplugHelper(EmuTime /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

void JoyHandle::resetWheelCache()
{
	for (auto& entry : wheelBindings) {
		entry.value = 0.0f;
	}
}

// MSXJoystickDevice
uint8_t JoyHandle::read(EmuTime time)
{
	Clock<2> clock(EmuTime::zero()); // ticks at 2Hz
	uint8_t cycle = clock.getTicksTill(time) & 1;

	const uint8_t wheelStatus =
	    ((analogValue < 0.0f) && ((analogValue < -0.5f) || (cycle == 1))) ? JOY_LEFT
	  : ((analogValue > 0.0f) && ((analogValue >  0.5f) || (cycle == 1))) ? JOY_RIGHT
	  : 0;
	return status & ~wheelStatus;
}

void JoyHandle::write(uint8_t /*value*/, EmuTime /*time*/)
{
}

[[nodiscard]] static float quantizeWheel(float a)
{
	if (a == 0.0f) return 0.0f;
	return std::copysign(std::abs(a) <= 0.5f ? 0.5f : 1.0f, a);
}

// MSXEventListener
void JoyHandle::signalMSXEvent(const Event& event,
                               EmuTime time) noexcept
{
	auto getJoyDeadThreshold = [&](JoystickId joyId) {
		return joystickManager.getJoyDeadThreshold(joyId);
	};
	auto getJoyValue = [&](JoystickId joyId, const JoystickAxisMotionEvent& e) {
		return joystickManager.getJoyValue(joyId, e);
	};

	uint8_t press = 0;
	uint8_t release = 0;
	for (int i : xrange(6)) {
		for (const auto& binding : bindings[i]) {
			if (auto onOff = match(binding, event, getJoyDeadThreshold)) {
				(*onOff ? press : release) |= 1 << i;
			}
		}
	}

	float newAnalog = 0;
	for (auto& [binding, value] : wheelBindings) {
		if (auto v = match(binding, event, getJoyValue)) {
			value = *v;
		}
		if (std::abs(value) > std::abs(newAnalog)) {
			newAnalog = value;
		}
	}
	newAnalog = quantizeWheel(newAnalog);

	if ((((status & ~press) | release) != status) || (newAnalog != analogValue)) {
		stateChangeDistributor.distributeNew<JoyHandleState>(
			time, id, press, release, newAnalog);
	}
}

// StateChangeListener
void JoyHandle::signalStateChange(const StateChange& event)
{
	const auto* jhs = std::get_if<JoyHandleState>(&event);
	if (!jhs) return;
	if (jhs->getId() != id) return;

	status = (status & ~jhs->getPress()) | jhs->getRelease();
	analogValue = jhs->getAnalog();
}

void JoyHandle::stopReplay(EmuTime time) noexcept
{
	uint8_t newStatus = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                    JOY_BUTTONA | JOY_BUTTONB;
	if (newStatus != status || (analogValue != 0.0f)) {
		resetWheelCache();
		uint8_t release = newStatus & ~status;
		stateChangeDistributor.distributeNew<JoyHandleState>(
			time, id, uint8_t(0), release, 0.0f);
	}
}


template<typename Archive>
void JoyHandle::serialize(Archive& ar, unsigned /*version*/)
{
	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
	ar.serialize("status",      status,
	             "analogValue", analogValue);
}
INSTANTIATE_SERIALIZE_METHODS(JoyHandle);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, JoyHandle, "JoyHandle");

} // namespace openmsx

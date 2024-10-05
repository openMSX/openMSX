#include "JoyHandle.hh"

#include "CommandController.hh"
#include "Event.hh"
#include "IntegerSetting.hh"
#include "JoystickManager.hh"
#include "MSXEventDistributor.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "build-info.hh"

#include "join.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"

namespace openmsx {

class JoyHandleState final : public StateChange
{
public:
	JoyHandleState() = default; // for serialize
	JoyHandleState(EmuTime::param time_, uint8_t id_,
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
REGISTER_POLYMORPHIC_CLASS(StateChange, JoyHandleState, "JoyHandleState");

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
		"UP",          makeTclList(tmpStrCat(joy, " hat0 up")),
		"DOWN",        makeTclList(tmpStrCat(joy, " hat0 down")),
		"LEFT",        makeTclList(tmpStrCat(joy, " hat0 left")),
		"RIGHT",       makeTclList(tmpStrCat(joy, " hat0 right")),
		"A",           listA,
		"B",           listB,
		"WHEEL_LEFT",  makeTclList(tmpStrCat(joy, " -axis0")),
		"WHEEL_RIGHT", makeTclList(tmpStrCat(joy, " +axis0")));
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
	// fill in 'bindings'
	checkJoystickConfig(configSetting.getValue());
}

JoyHandle::~JoyHandle()
{
	if (isPluggedIn()) {
		JoyHandle::unplugHelper(EmuTime::dummy());
	}
}

void JoyHandle::checkJoystickConfig(const TclObject& newValue)
{
	std::array<std::vector<BooleanInput>, 8> newBindings;

	auto& interp = commandController.getInterpreter();
	unsigned n = newValue.getListLength(interp);
	if (n & 1) {
		throw CommandException("Need an even number of elements");
	}
	for (unsigned i = 0; i < n; i += 2) {
		static constexpr std::array<std::string_view, 8> keys = {
			// order is important!
			"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "WHEEL_LEFT", "WHEEL_RIGHT"
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
std::string_view JoyHandle::getName() const
{
	switch (id) {
		case 1: return "joyhandle1";
		case 2: return "joyhandle2";
		default: UNREACHABLE;
	}
}

std::string_view JoyHandle::getDescription() const
{
	return description;
}

void JoyHandle::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);

	lastTime = EmuTime::zero();
	cycle = 0;
	analogValue = 0;
}

void JoyHandle::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// MSXJoystickDevice
uint8_t JoyHandle::read(EmuTime::param time)
{
	checkTime(time);
	const uint8_t wheelStatus =
			((analogValue < 0) && ((analogValue == -100) || (cycle == 1))) ? JOY_LEFT
		  : ((analogValue > 0) && ((analogValue ==  100) || (cycle == 1))) ? JOY_RIGHT
		  : 0;
	return status | wheelStatus;
}

void JoyHandle::write(uint8_t /*value*/, EmuTime::param /*time*/)
{
}

void JoyHandle::checkTime(EmuTime::param time)
{
	if ((time - lastTime) > EmuDuration::msec(500)) {
		// longer than 500ms since last read -> change cycle
		lastTime = time;
		cycle = 1 - cycle;
	}
}


// MSXEventListener
void JoyHandle::signalMSXEvent(const Event& event,
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

	for (int i = 6; i < 8; i++) {
		for (const auto& binding : bindings[i]) {
			std::visit(overloaded{
				[&](const BooleanJoystickAxis& bind, const JoystickAxisMotionEvent& e) {
					if (bind.getJoystick() != e.getJoystick()) return;
					if (bind.getAxis() != e.getAxis()) return;
					int deadZone = getJoyDeadZone(bind.getJoystick()); // percentage 0..100
					int threshold = (deadZone * 32768) / 100; // 0..32768
					int halfwayZone = (deadZone + 100) / 2; // percentage 0..100 halfway between deadZone and 100
					int halfwayThreshold = (halfwayZone * 32768) / 100; // 0..32768
					if (bind.getDirection() == BooleanJoystickAxis::Direction::POS) {
						analogValue = e.getValue() > halfwayThreshold ? 100
									: e.getValue() > threshold        ?  50
																	  :   0;
					} else {
						analogValue = e.getValue() < -halfwayThreshold ? -100
									: e.getValue() < -threshold        ?  -50
																	   :    0;
					}
				},
				[](const auto&, const auto&) { /*ignore*/ }
			}, binding, event);
		}
	}

	// TODO send analogValue to JoyHandleState

	if (((status & ~press) | release) != status) {
		stateChangeDistributor.distributeNew<JoyHandleState>(
			time, id, press, release);
	}
}

// StateChangeListener
void JoyHandle::signalStateChange(const StateChange& event)
{
	const auto* kjs = dynamic_cast<const JoyHandleState*>(&event);
	if (!kjs) return;
	if (kjs->getId() != id) return;

	status = (status & ~kjs->getPress()) | kjs->getRelease();

	// TODO receive analogValue from JoyHandleState
}

void JoyHandle::stopReplay(EmuTime::param time) noexcept
{
	uint8_t newStatus = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                    JOY_BUTTONA | JOY_BUTTONB;
	if (newStatus != status) {
		uint8_t release = newStatus & ~status;
		stateChangeDistributor.distributeNew<JoyHandleState>(
			time, id, uint8_t(0), release);
	}
}


template<typename Archive>
void JoyHandle::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("status",      status,
				 "lastTime",    lastTime,
				 "cycle",       cycle,
				 "analogValue", analogValue);
	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(JoyHandle);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, JoyHandle, "JoyHandle");

} // namespace openmsx

#include "JoyMega.hh"

#include "CommandController.hh"
#include "Event.hh"
#include "GlobalSettings.hh"
#include "MSXEventDistributor.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

#include "join.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "xrange.hh"

#include <memory>

namespace openmsx {

class JoyMegaState final : public StateChange
{
public:
	JoyMegaState() = default; // for serialize
	JoyMegaState(EmuTime::param time_, uint8_t id_,
	             unsigned press_, unsigned release_)
		: StateChange(time_)
		, press(press_), release(release_), id(id_) {}

	[[nodiscard]] auto getId()      const { return id; }
	[[nodiscard]] auto getPress()   const { return press; }
	[[nodiscard]] auto getRelease() const { return release; }

	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("id",      id,
		             "press",   press,
		             "release", release);
	}
private:
	unsigned press, release;
	uint8_t id;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, JoyMegaState, "JoyMegaState");

TclObject JoyMega::getDefaultConfig(uint8_t id)
{
	if (auto* sdl_joystick = SDL_JoystickOpen(id - 1)) {
		std::array<TclObject, 8> lists;
		auto joy = strCat("joy", id);
		for (auto b : xrange(SDL_JoystickNumButtons(sdl_joystick))) {
			lists[b % 8].addListElement(tmpStrCat(joy, " button", b));
		}
		TclObject result(TclObject::MakeDictTag{},
			"UP",    makeTclList(tmpStrCat(joy, " -axis1"), tmpStrCat(joy, " hat0 up")),
			"DOWN",  makeTclList(tmpStrCat(joy, " +axis1"), tmpStrCat(joy, " hat0 down")),
			"LEFT",  makeTclList(tmpStrCat(joy, " -axis0"), tmpStrCat(joy, " hat0 left")),
			"RIGHT", makeTclList(tmpStrCat(joy, " +axis0"), tmpStrCat(joy, " hat0 right")),
			"A",     lists[0],
			"B",     lists[1],
			"C",     lists[2],
			"X",     lists[3],
			"Y",     lists[4],
			"Z",     lists[5],
			"SELECT",lists[6],
			"START", lists[7]);
		SDL_JoystickClose(sdl_joystick);
		return result;
	}
	return TclObject();
}

JoyMega::JoyMega(CommandController& commandController_,
                 MSXEventDistributor& eventDistributor_,
                 StateChangeDistributor& stateChangeDistributor_,
                 GlobalSettings& globalSettings_,
                 uint8_t id_)
	: commandController(commandController_)
	, eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, globalSettings(globalSettings_)
	, configSetting(commandController, tmpStrCat("joymega", id_, "_config"),
		"joymega mapping configuration", getDefaultConfig(id_).getString())
	, description(strCat("Mega drive joystick ", id_, ". Mapping is fully configurable."))
	, id(id_)
{
	configSetting.setChecker([this](TclObject& newValue) {
		this->checkJoystickConfig(newValue); });
	// fill in 'bindings'
	checkJoystickConfig(const_cast<TclObject&>(configSetting.getValue()));
}

JoyMega::~JoyMega()
{
	if (isPluggedIn()) {
		JoyMega::unplugHelper(EmuTime::dummy());
	}
}

void JoyMega::checkJoystickConfig(TclObject& newValue)
{
	std::array<std::vector<BooleanInput>, 12> newBindings;

	auto& interp = commandController.getInterpreter();
	unsigned n = newValue.getListLength(interp);
	if (n & 1) {
		throw CommandException("Need an even number of elements");
	}
	for (unsigned i = 0; i < n; i += 2) {
		static constexpr std::array<std::string_view, 12> keys = {
			// order is important!
			"UP", "DOWN", "LEFT", "RIGHT",
			"A", "B", "C", "START",
			"X", "Y", "Z", "SELECT",
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
std::string_view JoyMega::getName() const
{
	switch (id) {
		case 1: return "joymega1";
		case 2: return "joymega2";
		default: UNREACHABLE; return "";
	}
}

std::string_view JoyMega::getDescription() const
{
	return description;
}

void JoyMega::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	plugHelper2();
	status = 0xfff;
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
uint8_t JoyMega::read(EmuTime::param time)
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

void JoyMega::write(uint8_t value, EmuTime::param time)
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

static constexpr unsigned encodeButton(unsigned button, uint8_t cycleMask)
{
	unsigned n = (cycleMask == 7) ? 7 : 3; // 6- or 3-button mode
	return 1 << (4 + (button & n));
}

// MSXEventListener
void JoyMega::signalMSXEvent(const Event& event, EmuTime::param time) noexcept
{
	uint8_t press = 0;
	uint8_t release = 0;

	auto getJoyDeadZone = [&](int joystick) {
		return globalSettings.getJoyDeadZoneSetting(joystick).getInt();
	};
	for (int i : xrange(12)) {
		for (const auto& binding : bindings[i]) {
			if (auto onOff = match(binding, event, getJoyDeadZone)) {
				(*onOff ? press : release) |= 1 << i;
			}
		}
	}

	if (((status & ~press) | release) != status) {
		stateChangeDistributor.distributeNew<JoyMegaState>(
			time, id, press, release);
	}
}

// StateChangeListener
void JoyMega::signalStateChange(const StateChange& event)
{
	const auto* js = dynamic_cast<const JoyMegaState*>(&event);
	if (!js) return;
	if (js->getId() != id) return;

	status = (status & ~js->getPress()) | js->getRelease();
}

void JoyMega::stopReplay(EmuTime::param time) noexcept
{
	unsigned newStatus = 0xfff;
	if (newStatus != status) {
		auto release = newStatus & ~status;
		stateChangeDistributor.distributeNew<JoyMegaState>(
			time, id, 0, release);
	}
}

template<typename Archive>
void JoyMega::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("lastTime",  lastTime,
	             "status",    status,
	             "cycle",     cycle,
	             "cycleMask", cycleMask);
	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper2();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(JoyMega);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, JoyMega, "JoyMega");

} // namespace openmsx

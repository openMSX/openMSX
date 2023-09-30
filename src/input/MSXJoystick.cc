#include "MSXJoystick.hh"

#include "Event.hh"
#include "GlobalSettings.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "StateChange.hh"
#include "serialize.hh"
#include "serialize_meta.hh"

#include "unreachable.hh"

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

MSXJoystick::MSXJoystick(//CommandController& commandController,
                         MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_,
                         GlobalSettings& globalSettings_,
                         uint8_t id_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, globalSettings(globalSettings_)
	, id(id_)
	, status(JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB)
{
	// TODO hardcoded for now
	up   .emplace_back(BooleanKeyboard(SDLK_UP));
	down .emplace_back(BooleanKeyboard(SDLK_DOWN));
	left .emplace_back(BooleanKeyboard(SDLK_LEFT));
	right.emplace_back(BooleanKeyboard(SDLK_RIGHT));
	trigA.emplace_back(BooleanKeyboard(SDLK_SPACE));
	trigA.emplace_back(BooleanMouseButton(1));
	trigB.emplace_back(BooleanKeyboard(SDLK_m));
	trigB.emplace_back(BooleanMouseButton(3));
}

MSXJoystick::~MSXJoystick()
{
	if (isPluggedIn()) {
		MSXJoystick::unplugHelper(EmuTime::dummy());
	}
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
	auto process = [&](std::span<const BooleanInput> bindings, uint8_t bit) {
		for (const auto& binding : bindings) {
			if (auto onOff = match(binding, event, getJoyDeadZone)) {
				(*onOff ? press : release) |= bit;
			}
		}
	};
	process(up,    JOY_UP);
	process(down,  JOY_DOWN);
	process(left,  JOY_LEFT);
	process(right, JOY_RIGHT);
	process(trigA, JOY_BUTTONA);
	process(trigB, JOY_BUTTONB);

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

// MHT Gunstick light gun.
//
// Games blank the screen, draw the targets in white and then poll the light
// sensor (e.g. Target Plus counts how many reads see light). The sensor is
// emulated by looking at the frame the VDP is drawing at the moment the MSX
// reads the port, see GunstickSensor.hh. So it needs an active renderer, and
// it asks the Display to also render the frames that frameskip doesn't show.
// With renderer "none" the gun never sees light.
// That also makes the sensor output non-reproducible, so it's recorded as a
// state change, and a replay uses the recorded value.

#include "Gunstick.hh"

#include "MSXEventDistributor.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"

#include "Display.hh"
#include "Event.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "stl.hh"

#include <SDL.h>

#include <cstdint>
#include <variant>

namespace openmsx {

Gunstick::Gunstick(MSXMotherBoard& motherBoard_,
                   MSXEventDistributor& eventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   Display& display_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, display(display_)
	, sensor(motherBoard_, display_)
{
}

Gunstick::~Gunstick()
{
	if (isPluggedIn()) {
		Gunstick::unplugHelper(EmuTime::dummy());
	}
}

// Pluggable
zstring_view Gunstick::getName() const
{
	return "gunstick";
}

zstring_view Gunstick::getDescription() const
{
	return "MHT Gunstick light gun, aimed with the mouse";
}

void Gunstick::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
	display.requestCrosshairCursor();
	display.requestRenderAllFrames();
}

void Gunstick::unplugHelper(EmuTime /*time*/)
{
	display.releaseRenderAllFrames();
	display.releaseCrosshairCursor();
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
static constexpr uint8_t LIGHT   = JoystickDevice::RD_PIN2;
static constexpr uint8_t TRIGGER = JoystickDevice::RD_PIN6;

[[nodiscard]] static constexpr uint8_t setActive(uint8_t status, uint8_t bit, bool active)
{
	return active ? (status & ~bit) : (status | bit); // 0-bit means active
}

uint8_t Gunstick::read(EmuTime time)
{
	if (!stateChangeDistributor.isReplaying()) {
		// The CPU syncs the scheduler before a port read, so on replay
		// this event is delivered before the read at the same time.
		if (auto newStatus = setActive(status, LIGHT, sensor.senseLight(time));
		    newStatus != status) {
			stateChangeDistributor.distributeNew<GunstickState>(time, newStatus);
		}
	}
	return status;
}

void Gunstick::write(uint8_t /*value*/, EmuTime /*time*/)
{
	// pin 8 is not used
}

// MSXEventListener
void Gunstick::signalMSXEvent(const Event& event,
                              EmuTime time) noexcept
{
	bool newTrigger = hostTrigger;
	std::visit(overloaded{
		[&](const MouseButtonDownEvent& e) {
			if (e.getButton() == SDL_BUTTON_LEFT) newTrigger = true;
		},
		[&](const MouseButtonUpEvent& e) {
			if (e.getButton() == SDL_BUTTON_LEFT) newTrigger = false;
		},
		[](const EventBase&) { /*ignore*/ }
	}, event);

	if (newTrigger != hostTrigger) {
		hostTrigger = newTrigger;
		stateChangeDistributor.distributeNew<GunstickState>(
			time, setActive(status, TRIGGER, hostTrigger));
	}
}

// StateChangeListener
void Gunstick::signalStateChange(const StateChange& event)
{
	if (const auto* gs = std::get_if<GunstickState>(&event)) {
		status = gs->getStatus();
	}
}

void Gunstick::stopReplay(EmuTime time) noexcept
{
	if (auto newStatus = setActive(status, TRIGGER, hostTrigger);
	    newStatus != status) {
		stateChangeDistributor.distributeNew<GunstickState>(time, newStatus);
	}
}


template<typename Archive>
void Gunstick::serialize(Archive& ar, unsigned /*version*/)
{
	// no need to serialize host state
	ar.serialize("status", status);

	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Gunstick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Gunstick, "Gunstick");

} // namespace openmsx

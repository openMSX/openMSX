// Light guns, emulated with the host mouse.
//
// Games blank the screen, draw the targets in white and then poll the light
// sensor (e.g. Target Plus counts how many reads see light). The sensor is
// emulated by looking at the frame the VDP is drawing at the moment the MSX
// reads the port, see LightGunSensor and GunstickSensor.hh. So it needs an
// active renderer, and it asks the Display to also render the frames that
// frameskip doesn't show. With renderer "none" the gun never sees light.
// That also makes the sensor output non-reproducible, so it's recorded as a
// state change, and a replay uses the recorded value.

#include "LightGun.hh"

#include "MSXEventDistributor.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"

#include "Display.hh"
#include "Event.hh"
#include "serialize.hh"
#include "stl.hh"

#include <SDL.h>

#include <cstdint>
#include <utility>
#include <variant>

namespace openmsx {

[[nodiscard]] static constexpr uint8_t setBit(uint8_t s, uint8_t bit, bool one)
{
	return one ? (s | bit) : (s & ~bit);
}

LightGun::LightGun(MSXMotherBoard& motherBoard_,
                   MSXEventDistributor& eventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   Display& display_, ID id_, Pins pins_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, display(display_)
	, sensor(motherBoard_, display_)
	, id(id_)
	, pins(pins_)
	, status(withTrigger(withLight(0x3F, false), false))
{
}

LightGun::~LightGun()
{
	if (isPluggedIn()) {
		LightGun::unplugHelper(EmuTime::dummy());
	}
}

uint8_t LightGun::withLight(uint8_t s, bool light) const
{
	return setBit(s, pins.light, light == pins.lightHigh);
}

uint8_t LightGun::withTrigger(uint8_t s, bool pulled) const
{
	return setBit(s, pins.trigger, !pulled);
}

// Pluggable
void LightGun::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
	display.requestCrosshairCursor();
	display.requestRenderAllFrames();
}

void LightGun::unplugHelper(EmuTime /*time*/)
{
	display.releaseRenderAllFrames();
	display.releaseCrosshairCursor();
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
uint8_t LightGun::read(EmuTime time)
{
	if (!stateChangeDistributor.isReplaying()) {
		// The CPU syncs the scheduler before a port read, so on replay
		// this event is delivered before the read at the same time.
		if (auto newStatus = withLight(status, sensor.senseLight(time));
		    newStatus != status) {
			stateChangeDistributor.distributeNew<LightGunState>(
				time, std::to_underlying(id), newStatus);
		}
	}
	return status;
}

void LightGun::write(uint8_t /*value*/, EmuTime /*time*/)
{
	// pin 8 is not used
}

// MSXEventListener
void LightGun::signalMSXEvent(const Event& event,
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
		stateChangeDistributor.distributeNew<LightGunState>(
			time, std::to_underlying(id), withTrigger(status, hostTrigger));
	}
}

// StateChangeListener
void LightGun::signalStateChange(const StateChange& event)
{
	if (const auto* s = std::get_if<LightGunState>(&event);
	    s && (s->getId() == std::to_underlying(id))) {
		status = s->getStatus();
	}
}

void LightGun::stopReplay(EmuTime time) noexcept
{
	if (auto newStatus = withTrigger(status, hostTrigger);
	    newStatus != status) {
		stateChangeDistributor.distributeNew<LightGunState>(
			time, std::to_underlying(id), newStatus);
	}
}


template<typename Archive>
void LightGun::serialize(Archive& ar, unsigned /*version*/)
{
	// no need to serialize host state
	ar.serialize("status", status);

	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}

INSTANTIATE_SERIALIZE_METHODS(LightGun);

} // namespace openmsx

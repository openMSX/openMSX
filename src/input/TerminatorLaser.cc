// ASCII Plus-X Terminator Laser light gun.
//
// It works like the Gunstick (see Gunstick.cc and GunstickSensor.hh, the
// sensor is shared via LightGunSensor), but on other pins and with the light
// polarity inverted. E.g. Dungeon Hunter blanks the screen, then shows one
// target per frame as a white block and polls the sensor during that frame:
// any read with light hits that target.
// The sensor output is recorded as a state change, and a replay uses the
// recorded value.

#include "TerminatorLaser.hh"

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

TerminatorLaser::TerminatorLaser(MSXMotherBoard& motherBoard_,
                                 MSXEventDistributor& eventDistributor_,
                                 StateChangeDistributor& stateChangeDistributor_,
                                 Display& display_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, display(display_)
	, sensor(motherBoard_, display_)
{
}

TerminatorLaser::~TerminatorLaser()
{
	if (isPluggedIn()) {
		TerminatorLaser::unplugHelper(EmuTime::dummy());
	}
}

// Pluggable
zstring_view TerminatorLaser::getName() const
{
	return "terminatorlaser";
}

zstring_view TerminatorLaser::getDescription() const
{
	return "ASCII Plus-X Terminator Laser light gun, aimed with the mouse";
}

void TerminatorLaser::plugHelper(Connector& /*connector*/, EmuTime /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
	display.requestCrosshairCursor();
	display.requestRenderAllFrames();
}

void TerminatorLaser::unplugHelper(EmuTime /*time*/)
{
	display.releaseRenderAllFrames();
	display.releaseCrosshairCursor();
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
static constexpr uint8_t LIGHT   = JoystickDevice::RD_PIN6; // 1 = light
static constexpr uint8_t TRIGGER = JoystickDevice::RD_PIN7; // 0 = pulled

[[nodiscard]] static constexpr uint8_t setBit(uint8_t status, uint8_t bit, bool one)
{
	return one ? (status | bit) : (status & ~bit);
}

uint8_t TerminatorLaser::read(EmuTime time)
{
	if (!stateChangeDistributor.isReplaying()) {
		// The CPU syncs the scheduler before a port read, so on replay
		// this event is delivered before the read at the same time.
		if (auto newStatus = setBit(status, LIGHT, sensor.senseLight(time));
		    newStatus != status) {
			stateChangeDistributor.distributeNew<TerminatorLaserState>(time, newStatus);
		}
	}
	return status;
}

void TerminatorLaser::write(uint8_t /*value*/, EmuTime /*time*/)
{
	// pin 8 is not used
}

// MSXEventListener
void TerminatorLaser::signalMSXEvent(const Event& event,
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
		stateChangeDistributor.distributeNew<TerminatorLaserState>(
			time, setBit(status, TRIGGER, !hostTrigger));
	}
}

// StateChangeListener
void TerminatorLaser::signalStateChange(const StateChange& event)
{
	if (const auto* ts = std::get_if<TerminatorLaserState>(&event)) {
		status = ts->getStatus();
	}
}

void TerminatorLaser::stopReplay(EmuTime time) noexcept
{
	if (auto newStatus = setBit(status, TRIGGER, !hostTrigger);
	    newStatus != status) {
		stateChangeDistributor.distributeNew<TerminatorLaserState>(time, newStatus);
	}
}


template<typename Archive>
void TerminatorLaser::serialize(Archive& ar, unsigned /*version*/)
{
	// no need to serialize host state
	ar.serialize("status", status);

	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(TerminatorLaser);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, TerminatorLaser, "TerminatorLaser");

} // namespace openmsx

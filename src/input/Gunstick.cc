// MHT Gunstick light gun.
//
// Games blank the screen, draw the targets in white and then poll the light
// sensor (e.g. Target Plus counts how many reads see light). The sensor is
// emulated by looking at the frame the VDP is drawing at the moment the MSX
// reads the port, see GunstickSensor.hh. So it needs an active renderer: on
// skipped frames the working frame still holds an older picture, and with
// renderer "none" the gun never sees light.
// That also makes the sensor output non-reproducible, so it's recorded as a
// state change, and a replay uses the recorded value.

#include "Gunstick.hh"
#include "GunstickSensor.hh"

#include "MSXEventDistributor.hh"
#include "StateChange.hh"
#include "StateChangeDistributor.hh"

#include "Display.hh"
#include "Event.hh"
#include "MSXMotherBoard.hh"
#include "OutputSurface.hh"
#include "RawFrame.hh"
#include "RenderSettings.hh"
#include "SDLRasterizer.hh"
#include "VDP.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "stl.hh"

#include <SDL.h>

#include <array>
#include <cstdint>
#include <optional>
#include <variant>

using namespace gl;

namespace openmsx {

static_assert(gunstick::TICKS_PER_LINE == VDP::TICKS_PER_LINE);

Gunstick::Gunstick(MSXMotherBoard& motherBoard_,
                   MSXEventDistributor& eventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   Display& display_)
	: motherBoard(motherBoard_)
	, eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, display(display_)
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
}

void Gunstick::unplugHelper(EmuTime /*time*/)
{
	display.releaseCrosshairCursor();
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}

// JoystickDevice
static constexpr uint8_t LIGHT   = JoystickDevice::RD_PIN2;
static constexpr uint8_t TRIGGER = JoystickDevice::RD_PIN6;

uint8_t Gunstick::read(EmuTime time)
{
	if (!stateChangeDistributor.isReplaying()) {
		// The CPU syncs the scheduler before a port read, so on replay
		// this event is delivered before the read at the same time.
		if (bool newLight = senseLight(time); newLight != light) {
			stateChangeDistributor.distributeNew<GunstickLightState>(time, newLight);
		}
	}
	uint8_t result = 0x3F; // 1-bit means not active
	if (trigger) result &= ~TRIGGER;
	if (light)   result &= ~LIGHT;
	return result;
}

void Gunstick::write(uint8_t /*value*/, EmuTime /*time*/)
{
	// pin 8 is not used
}

bool Gunstick::senseLight(EmuTime time)
{
	if (!aim) return false;
	if (!vdp) {
		vdp = dynamic_cast<VDP*>(motherBoard.findDevice("VDP")); // TODO name based OK?
		if (!vdp) return false; // not expected: MSX machines have a VDP
	}

	int firstLineTicks = SDLRasterizer::getLineRenderTop(vdp->isPalTiming()) * VDP::TICKS_PER_LINE;
	int beamPos = gunstick::beamPosition(vdp->getTicksThisFrame(time) - firstLineTicks);
	// Bail out early: getWorkingFrame() syncs the renderer.
	if (!gunstick::beamNearAim(beamPos, *aim)) return false;

	const RawFrame* frame = vdp->getWorkingFrame(time);
	if (!frame) return false; // renderer "none"

	std::array<FrameSource::Pixel, gunstick::FRAME_WIDTH> buf;
	const FrameSource::Pixel* line = nullptr;
	int lineY = -1;
	return gunstick::seesLight(beamPos, *aim, [&](int x, int y) {
		if (y != lineY) {
			line = frame->getLinePtr320_240(y, buf).data();
			lineY = y;
		}
		return line[x];
	});
}

std::optional<ivec2> Gunstick::mouseToFrame(ivec2 mouse) const
{
	const auto* output = display.getOutputSurface();
	if (!output) return {};
	auto pixelSize = display.getMsxPixelSize();
	if (!pixelSize) return {};
	auto& renderSettings = display.getRenderSettings();
	return gunstick::mouseToFrame(
		vec2(mouse), *pixelSize,
		vec2(output->getViewOffset()), vec2(output->getViewSize()),
		renderSettings.getHorizontalStretch(), renderSettings.getFullStretch());
}

// MSXEventListener
void Gunstick::signalMSXEvent(const Event& event,
                              EmuTime time) noexcept
{
	auto newAim = hostAim;
	bool newTrigger = hostTrigger;

	std::visit(overloaded{
		[&](const MouseMotionEvent& e) {
			newAim = mouseToFrame(ivec2(e.getAbsX(), e.getAbsY()));
		},
		[&](const MouseButtonDownEvent& e) {
			if (e.getButton() == SDL_BUTTON_LEFT) newTrigger = true;
		},
		[&](const MouseButtonUpEvent& e) {
			if (e.getButton() == SDL_BUTTON_LEFT) newTrigger = false;
		},
		[](const EventBase&) { /*ignore*/ }
	}, event);

	if ((newAim != hostAim) || (newTrigger != hostTrigger)) {
		hostAim = newAim;
		hostTrigger = newTrigger;
		stateChangeDistributor.distributeNew<GunstickState>(time, hostAim, hostTrigger);
	}
}

// StateChangeListener
void Gunstick::signalStateChange(const StateChange& event)
{
	if (const auto* gs = std::get_if<GunstickState>(&event)) {
		aim     = gs->getAim();
		trigger = gs->getTrigger();
	} else if (const auto* ls = std::get_if<GunstickLightState>(&event)) {
		light = ls->getLight();
	}
}

void Gunstick::stopReplay(EmuTime time) noexcept
{
	if ((aim != hostAim) || (trigger != hostTrigger)) {
		stateChangeDistributor.distributeNew<GunstickState>(time, hostAim, hostTrigger);
	}
}


template<typename Archive>
void Gunstick::serialize(Archive& ar, unsigned /*version*/)
{
	// no need to serialize host state
	GunstickState::serializeAim(ar, aim);
	ar.serialize("trigger", trigger,
	             "light",   light);

	if constexpr (Archive::IS_LOADER) {
		if (isPluggedIn()) {
			plugHelper(*getConnector(), EmuTime::dummy());
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Gunstick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Gunstick, "Gunstick");

} // namespace openmsx

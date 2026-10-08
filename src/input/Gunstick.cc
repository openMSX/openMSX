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
#include "VideoSystem.hh"
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
		if (auto newStatus = setActive(status, LIGHT, senseLight(time));
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

bool Gunstick::senseLight(EmuTime time)
{
	auto aim = getAim();
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

	// cache the last fetched line
	std::array<FrameSource::Pixel, gunstick::FRAME_WIDTH> buf;
	const FrameSource::Pixel* lineCache = nullptr;
	int lineCacheY = -1;
	return gunstick::seesLight(beamPos, *aim, [&](int x, int y) {
		if (y != lineCacheY) {
			lineCache = frame->getLinePtr320_240(y, buf).data();
			lineCacheY = y;
		}
		return lineCache[x];
	});
}

// The RawFrame pixel under the host mouse pointer.
std::optional<ivec2> Gunstick::getAim() const
{
	const auto* output = display.getOutputSurface();
	if (!output) return {};
	auto mouse = display.getVideoSystem().getMouseCoord();
	if (!mouse) return {}; // not over the openMSX window
	auto pixelSize = display.getMsxPixelSize();
	if (!pixelSize) return {};
	auto& renderSettings = display.getRenderSettings();
	float hStretch = renderSettings.getHorizontalStretch();
	auto view = gunstick::mouseToView(
		vec2(*mouse), *pixelSize,
		vec2(output->getViewOffset()), vec2(output->getViewSize()),
		hStretch, renderSettings.getFullStretch());
	return (renderSettings.getDisplayDeform() == RenderSettings::DisplayDeform::_3D)
	     ? gunstick::viewToFrame3D(view, hStretch)
	     : gunstick::viewToFrame(view, hStretch);
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

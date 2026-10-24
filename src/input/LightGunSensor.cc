#include "LightGunSensor.hh"
#include "GunstickSensor.hh"

#include "Display.hh"
#include "MSXMotherBoard.hh"
#include "OutputSurface.hh"
#include "RawFrame.hh"
#include "RenderSettings.hh"
#include "SDLRasterizer.hh"
#include "VDP.hh"
#include "VideoSystem.hh"

#include <array>

using namespace gl;

namespace openmsx {

static_assert(gunstick::TICKS_PER_LINE == VDP::TICKS_PER_LINE);

LightGunSensor::LightGunSensor(MSXMotherBoard& motherBoard_, Display& display_)
	: motherBoard(motherBoard_)
	, display(display_)
{
}

bool LightGunSensor::senseLight(EmuTime time)
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
std::optional<ivec2> LightGunSensor::getAim() const
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

} // namespace openmsx

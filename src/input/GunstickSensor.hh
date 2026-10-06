#ifndef GUNSTICKSENSOR_HH
#define GUNSTICKSENSOR_HH

// Pure helpers for the Gunstick light gun emulation. They don't depend on
// the VDP or the Display, so they can be unit tested.

#include "PixelOperations.hh"
#include "gl_vec.hh"

#include <algorithm>
#include <cstdint>
#include <optional>

namespace openmsx::gunstick {

// RawFrame geometry, see SDLRasterizer.
inline constexpr int FRAME_WIDTH = 320;
inline constexpr int FRAME_HEIGHT = 240;
inline constexpr int TICKS_PER_LINE = 1368; // VDP::TICKS_PER_LINE
inline constexpr int PIXELS_PER_LINE = TICKS_PER_LINE / 4; // in 320-wide pixels
inline constexpr int TICKS_ORIGIN = 770; // VDP tick of RawFrame column 160

// Sensor model, in RawFrame pixels. Based on NYYRIKKI's measurements of a
// real gun (openMSX issue #139): the sensor sees a circle of about 5 pixels
// radius, reacts about one line after the light and then holds the signal
// for 24-28 lines.
// The delay also matters for the emulation: the renderer has not yet drawn
// the pixel under the beam, so that pixel (and the rest of the line) still
// holds an older frame.
inline constexpr int SENSOR_RADIUS = 5;
inline constexpr int SENSOR_DELAY = 1 * PIXELS_PER_LINE;
inline constexpr int PERSISTENCE = 26 * PIXELS_PER_LINE;
inline constexpr int BRIGHT_THRESHOLD = 128; // luminance, 0..255

/** Is this RawFrame pixel bright enough to be seen by the sensor? */
[[nodiscard]] inline bool isBright(Pixel p)
{
	PixelOperations pixelOps;
	unsigned luminance = (pixelOps.red(p) * 77 + pixelOps.green(p) * 150 + pixelOps.blue(p) * 29) >> 8;
	return luminance >= BRIGHT_THRESHOLD;
}

/** Position of the VDP beam expressed as the RawFrame pixel (y * PIXELS_PER_LINE
  * + x) it is drawing. Differences between positions are durations.
  * @param ticks VDP ticks since the start of RawFrame line 0 (see
  *              SDLRasterizer::getLineRenderTop()), may be negative.
  */
[[nodiscard]] constexpr int beamPosition(int ticks)
{
	int y = (ticks >= 0) ? (ticks / TICKS_PER_LINE)
	                     : -((TICKS_PER_LINE - 1 - ticks) / TICKS_PER_LINE);
	int x = (((ticks - y * TICKS_PER_LINE) - TICKS_ORIGIN) >> 2) + FRAME_WIDTH / 2;
	return y * PIXELS_PER_LINE + x;
}

/** Could the sensor see light at all? (Cheap test, done before fetching
  * the frame.) */
[[nodiscard]] constexpr bool beamNearAim(int beamPos, gl::ivec2 aim)
{
	int first = (aim.y - SENSOR_RADIUS) * PIXELS_PER_LINE + (aim.x - SENSOR_RADIUS);
	int last  = (aim.y + SENSOR_RADIUS) * PIXELS_PER_LINE + (aim.x + SENSOR_RADIUS);
	return (first + SENSOR_DELAY <= beamPos) &&
	       (beamPos <= last + SENSOR_DELAY + PERSISTENCE);
}

/** Does the sensor see light? That is: did the beam draw a bright pixel
  * within SENSOR_RADIUS of the aim point, between SENSOR_DELAY and
  * SENSOR_DELAY + PERSISTENCE pixels ago?
  * @param getPixel Callable (int x, int y) -> Pixel, a RawFrame pixel.
  */
template<typename GetPixel>
[[nodiscard]] bool seesLight(int beamPos, gl::ivec2 aim, GetPixel getPixel)
{
	if (!beamNearAim(beamPos, aim)) return false; // also checked by callers, cheap

	int y0 = std::max(aim.y - SENSOR_RADIUS, 0);
	int y1 = std::min(aim.y + SENSOR_RADIUS, FRAME_HEIGHT - 1);
	int x0 = std::max(aim.x - SENSOR_RADIUS, 0);
	int x1 = std::min(aim.x + SENSOR_RADIUS, FRAME_WIDTH - 1);
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			int age = beamPos - (y * PIXELS_PER_LINE + x);
			if ((age < SENSOR_DELAY) || (age > SENSOR_DELAY + PERSISTENCE)) continue;
			if (isBright(getPixel(x, y))) return true;
		}
	}
	return false;
}

/** Host mouse position -> RawFrame position, nullopt when outside the MSX
  * image. Mirrors PostProcessor::paint(): the image shows the middle
  * 'hStretch' columns and all 240 lines of the RawFrame.
  * @param mouse Mouse position, in window coordinates ('points').
  * @param msxPixelSize Size of one RawFrame pixel in points, see
  *                     VisibleSurface::getMsxPixelSize().
  * @param viewOffset, viewSize The OutputSurface view port, in physical
  *                    pixels (differ from points on high-DPI displays).
  */
[[nodiscard]] inline std::optional<gl::ivec2> mouseToFrame(
	gl::vec2 mouse, gl::vec2 msxPixelSize,
	gl::vec2 viewOffset, gl::vec2 viewSize,
	float hStretch, bool fullStretch)
{
	// With full_stretch the image covers the whole window. Otherwise convert
	// the view offset from physical pixels to points.
	gl::vec2 offset = fullStretch
	                ? gl::vec2()
	                : viewOffset * (msxPixelSize * gl::vec2(hStretch, float(FRAME_HEIGHT)) / viewSize);
	gl::vec2 pos = (mouse - offset) / msxPixelSize;
	float fx = (float(FRAME_WIDTH) - hStretch) * 0.5f + pos.x;
	float fy = pos.y;
	if ((fx < 0.0f) || (fx >= float(FRAME_WIDTH)) ||
	    (fy < 0.0f) || (fy >= float(FRAME_HEIGHT))) {
		return {};
	}
	return gl::ivec2(int(fx), int(fy));
}

} // namespace openmsx::gunstick

#endif

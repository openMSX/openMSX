#ifndef V9990RENDERER_HH
#define V9990RENDERER_HH

#include "V9990ModeEnum.hh"

#include "EmuTime.hh"

#include <cstdint>

namespace openmsx {

class PostProcessor;

/** Abstract base class for V9990 renderers.
  * A V9990Renderer is a class that converts the V9990 state into
  * visual information (e.g. pixels on a screen).
  *
  * @see Renderer.hh
  */
class V9990Renderer
{
public:
	virtual ~V9990Renderer() = default;

	/** See V9990::getPostProcessor. */
	[[nodiscard]] virtual PostProcessor* getPostProcessor() const = 0;

	/** Re-initialise the V9990Renderer's state.
	  * @param time The moment in emulated time this reset occurs.
	  */
	virtual void reset(EmuTime time) = 0;

	/** Signal the start of a new frame.
	  * The V9990Renderer can use this to get fixed-per-frame
	  * settings from the V9990 VDP. Typical settings include:
	  * - PAL/NTSC timing
	  * - MCLK/XTAL selection
	  * @param time The moment in emulated time the frame starts.
	  */
	virtual void frameStart(EmuTime time) = 0;

	/** Signal the end of the current frame.
	  * @param time The moment in emulated time the frame ends.
	  */
	virtual void frameEnd(EmuTime time) = 0;

	/** Render until the given point in emulated time
	  * @param time The moment in emulated time the frame ends.
	  */
	virtual void renderUntil(EmuTime time) = 0;

	/** Informs the renderer of a VDP display enabled change.
	 *  Both the regular border start/end and forced blanking by clearing
	 *  the display enable bit are considered display enabled changes.
	 *  @param enabled The new display enabled state.
	 *  @param time The moment in emulated time this change occurs.
	 */
	virtual void updateDisplayEnabled(bool enabled, EmuTime time) = 0;

	/** Set screen mode. */
	virtual void setDisplayMode(V9990DisplayMode mode,
	                            EmuTime time) = 0;

	/** Set color mode. */
	virtual void setColorMode(V9990ColorMode mode,
	                          EmuTime time) = 0;

	/** Set a palette entry. */
	virtual void updatePalette(int index, uint8_t r, uint8_t g, uint8_t b, bool ys,
	                        EmuTime time) = 0;

	/** The CPU read from or wrote to the palette data port (P#1).
	  * On a real V9990 such an access causes "palette snow": for a short
	  * moment every palette lookup made by the video output returns the
	  * accessed palette entry.
	  * For a write this must be called after updatePalette().
	  * @param index  The accessed palette entry.
	  * @param time   The moment in emulated time of the access.
	  */
	virtual void paletteSnow(int index, EmuTime time) = 0;

	/** Does an access to the palette data port at this moment cause
	  * palette snow: does at least one of the pixels drawn during the
	  * access get its color from the palette? That's not the case when
	  * the beam is in the blanking or when the pixels there have a
	  * direct color. The colors themselves don't matter: snow that has
	  * the color the pixel already had also counts.
	  * Unlike paletteSnow() this doesn't draw anything, and it doesn't
	  * depend on frame skip or on the 'v9990_palette_snow' setting.
	  * @param time  The moment in emulated time of the access.
	  */
	[[nodiscard]] virtual bool isPaletteInUse(EmuTime time) = 0;

	/** Change superimpose status. */
	virtual void updateSuperimposing(bool enabled, EmuTime time) = 0;

	/** Set background color. */
	virtual void updateBackgroundColor(int index, EmuTime time) = 0;

	/** Set scroll register. */
	virtual void updateScrollAX(EmuTime time) = 0;
	virtual void updateScrollBX(EmuTime time) = 0;
	virtual void updateScrollAYLow(EmuTime time) = 0;
	virtual void updateScrollBYLow(EmuTime time) = 0;

	/** Set layer disable bits (SDA/SDB in R#22). */
	virtual void updateLayerDisable(EmuTime time) = 0;

protected:
	V9990Renderer() = default;
};

} // namespace openmsx

#endif

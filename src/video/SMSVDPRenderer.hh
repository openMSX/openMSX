#ifndef SMSVDPRENDERER_HH
#define SMSVDPRENDERER_HH

#include "EmuTime.hh"

#include <cstdint>
#include <span>

namespace openmsx {

class PostProcessor;

/** Abstract base class for SMSVDP renderers.
  * A SMSVDPRenderer converts the output of the Franky's Sega VDP core into
  * visual information (e.g. pixels on a screen).
  *
  * @see Renderer.hh
  */
class SMSVDPRenderer
{
public:
	virtual ~SMSVDPRenderer() = default;

	/** See SMSVDP::getPostProcessor. */
	[[nodiscard]] virtual PostProcessor* getPostProcessor() const = 0;

	/** Is this renderer currently producing visible output?
	  * When false, the VDP core can skip the scanline rendering work.
	  */
	[[nodiscard]] virtual bool isActive() const = 0;

	/** Re-initialise the renderer's state. */
	virtual void reset(EmuTime time) = 0;

	/** Signal the start of a new frame. */
	virtual void frameStart(EmuTime time) = 0;

	/** Signal the end of the current frame. */
	virtual void frameEnd(EmuTime time) = 0;

	/** Draw one output scanline (320 pixels in 0x00RRGGBB format). */
	virtual void drawLine(unsigned y, std::span<const uint32_t, 320> pixels) = 0;

protected:
	SMSVDPRenderer() = default;
};

} // namespace openmsx

#endif

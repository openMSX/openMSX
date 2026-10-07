#ifndef SMSVDPRASTERIZER_HH
#define SMSVDPRASTERIZER_HH

#include "EmuTime.hh"

#include <cstdint>
#include <span>

namespace openmsx {

class PostProcessor;

/** Abstract base class for SMSVDP rasterizers.
  * A rasterizer receives complete scanlines from the Sega VDP core and
  * transfers them to the video system (typically via a PostProcessor).
  */
class SMSVDPRasterizer
{
public:
	virtual ~SMSVDPRasterizer() = default;

	/** See SMSVDP::getPostProcessor. */
	[[nodiscard]] virtual PostProcessor* getPostProcessor() const = 0;

	virtual void reset() = 0;
	virtual void frameStart() = 0;
	virtual void frameEnd(EmuTime time) = 0;

	/** Draw one output scanline (320 pixels in 0x00RRGGBB format). */
	virtual void drawLine(unsigned y, std::span<const uint32_t, 320> pixels) = 0;

protected:
	SMSVDPRasterizer() = default;
};

} // namespace openmsx

#endif

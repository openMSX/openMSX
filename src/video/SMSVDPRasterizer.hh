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

	virtual void frameEnd(EmuTime time) = 0;

	/** Draw one output scanline (320 palette indices, values 0..79). */
	virtual void drawLine(unsigned y, std::span<const uint8_t, 320> paletteIndices) = 0;

protected:
	SMSVDPRasterizer() = default;
};

} // namespace openmsx

#endif

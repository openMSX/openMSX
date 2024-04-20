#ifndef SUPERIMPOSEDFRAME_HH
#define SUPERIMPOSEDFRAME_HH

#include "FrameSource.hh"

namespace openmsx {

/** This class represents a frame that is the (per-pixel) alpha-blend of
  * two other frames. When the two input frames have a different resolution.
  * The result will have the highest resolution of the two inputs (in other
  * words, the lower resolution frame gets upscaled to the higher resolution).
  */
class SuperImposedFrame final : public FrameSource
{
public:
	void init(const FrameSource* top, const FrameSource* bottom);

	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;
	[[nodiscard]] std::span<const Pixel> getUnscaledLine(
		unsigned line, std::span<Pixel> helpBuf) const override;

private:
	const FrameSource* top;
	const FrameSource* bottom;
};

} // namespace openmsx

#endif

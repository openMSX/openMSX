#ifndef SUPERIMPOSEDFRAME_HH
#define SUPERIMPOSEDFRAME_HH

#include "FrameSource.hh"
#include <memory>

namespace openmsx {

/** This class represents a frame that is the (per-pixel) alpha-blend of
  * two other frames. When the two input frames have a different resolution.
  * The result will have the highest resolution of the two inputs (in other
  * words, the lower resolution frame gets upscaled to the higher resolution).
  */
class SuperImposedFrame : public FrameSource
{
public:
	[[nodiscard]] static std::unique_ptr<SuperImposedFrame> create(
		const PixelFormat& format);
	void init(const FrameSource* top, const FrameSource* bottom);
	virtual ~SuperImposedFrame() = default;

protected:
	explicit SuperImposedFrame(const PixelFormat& format);

protected:
	const FrameSource* top;
	const FrameSource* bottom;
};

} // namespace openmsx

#endif

#ifndef SUPERIMPOSEDFRAME_HH
#define SUPERIMPOSEDFRAME_HH

#include "FrameSource.hh"
#include "PixelOperations.hh"

namespace openmsx {

/** This class represents a frame that is the (per-pixel) alpha-blend of
  * two other frames. When the two input frames have a different resolution.
  * The result will have the highest resolution of the two inputs (in other
  * words, the lower resolution frame gets upscaled to the higher resolution).
  */
class SuperImposedFrame final : public FrameSource
{
public:
	explicit SuperImposedFrame(const PixelFormat& format);
	virtual ~SuperImposedFrame() = default;
	void init(const FrameSource* top, const FrameSource* bottom);

	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;
	[[nodiscard]] const void* getLineInfo(
		unsigned line, unsigned& width,
		void* buf, unsigned bufWidth) const override;

private:
	PixelOperations pixelOps;
	const FrameSource* top;
	const FrameSource* bottom;
};

} // namespace openmsx

#endif

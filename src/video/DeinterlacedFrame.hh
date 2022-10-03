#ifndef DEINTERLACEDFRAME_HH
#define DEINTERLACEDFRAME_HH

#include "FrameSource.hh"
#include <array>

namespace openmsx {

/** Produces a deinterlaced video frame based on two other FrameSources
  * (typically two RawFrames) containing the even and odd field.
  * This class does not copy the data from the input FrameSources.
  */
class DeinterlacedFrame final : public FrameSource
{
public:
	explicit DeinterlacedFrame(const PixelFormat& format);
	void init(FrameSource* evenField, FrameSource* oddField);

private:
	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;
	[[nodiscard]] const void* getLineInfo(
		unsigned line, unsigned& width,
		void* buf, unsigned bufWidth) const override;

private:
	/** The original frames whose data will be deinterlaced.
	  * The even frame is at index 0, the odd frame at index 1.
	  */
	std::array<FrameSource*, 2> fields;
};

} // namespace openmsx

#endif // DEINTERLACEDFRAME_HH

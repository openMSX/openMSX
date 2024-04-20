#ifndef DOUBLEDFRAME_HH
#define DOUBLEDFRAME_HH

#include "FrameSource.hh"

namespace openmsx {

/** Produces a video frame that has every line from the input frame twice
  * plus a possibly repeated line at the top.
  * This class does not copy the data from the input FrameSource.
  */
class DoubledFrame final : public FrameSource
{
public:
	void init(FrameSource* field, int skip);

private:
	[[nodiscard]] unsigned getLineWidth(unsigned line) const override;
	[[nodiscard]] std::span<const FrameSource::Pixel> getUnscaledLine(
		unsigned line, void* buf, unsigned bufWidth) const override;

private:
	/** The original frame whose data will be doubled.
	  */
	FrameSource* field;
	int skip;
};

} // namespace openmsx

#endif

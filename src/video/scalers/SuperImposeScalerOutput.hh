#ifndef SUPERIMPOSESCALEROUTPUT_HH
#define SUPERIMPOSESCALEROUTPUT_HH

#include "ScalerOutput.hh"
#include "PixelOperations.hh"

namespace openmsx {

class RawFrame;

template<typename Pixel>
class SuperImposeScalerOutput final : public ScalerOutput<Pixel>
{
public:
	SuperImposeScalerOutput(ScalerOutput<Pixel>& output,
	                        const RawFrame& superImpose_,
	                        const PixelOperations<Pixel>& pixelOps_);

	unsigned getWidth()  const override;
	unsigned getHeight() const override;
	Pixel* acquireLine(unsigned y) override;
	void   releaseLine(unsigned y, Pixel* buf)  override;
	void   fillLine   (unsigned y, Pixel color) override;

private:
	const Pixel* getSrcLine(unsigned y, Pixel* buf);

	ScalerOutput<Pixel>& output;
	const RawFrame& superImpose;
	const PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif

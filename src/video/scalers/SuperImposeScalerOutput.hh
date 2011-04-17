// $Id:$

#ifndef SUPERIMPOSESCALEROUTPUT_HH
#define SUPERIMPOSESCALEROUTPUT_HH

#include "ScalerOutput.hh"
#include "PixelOperations.hh"

namespace openmsx {

class RawFrame;

template<typename Pixel>
class SuperImposeScalerOutput : public ScalerOutput<Pixel>
{
public:
	SuperImposeScalerOutput(ScalerOutput<Pixel>& output,
	                        const RawFrame& superImpose_,
	                        const PixelOperations<Pixel>& pixelOps_);

	virtual unsigned getWidth()  const;
	virtual unsigned getHeight() const;
	virtual Pixel* acquireLine(unsigned y);
	virtual void   releaseLine(unsigned y, Pixel* buf);
	virtual void   fillLine   (unsigned y, Pixel color);

private:
	const Pixel* getSrcLine(unsigned y);

	ScalerOutput<Pixel>& output;
	const RawFrame& superImpose;
	const PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif

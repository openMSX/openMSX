// $Id$

#ifndef SCALE2XSCALER_HH
#define SCALE2XSCALER_HH

#include "Scaler2.hh"

namespace openmsx {

/** Runs the Scale2x scaler algorithm.
  */
template <class Pixel>
class Scale2xScaler: public Scaler2<Pixel>
{
public:
	Scale2xScaler(SDL_PixelFormat* format);

	virtual void scale256(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale512(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);

private:
	void scaleLine256Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
	void scaleLine512Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
};

} // namespace openmsx

#endif

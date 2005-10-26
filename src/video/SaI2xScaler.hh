// $Id$

#ifndef SAI2XSCALER_HH
#define SAI2XSCALER_HH

#include "Scaler2.hh"

namespace openmsx {

/** 2xSaI algorithm: edge-detection which produces a rounded look.
  * Algorithm was developed by Derek Liauw Kie Fa.
  */
template <class Pixel>
class SaI2xScaler: public Scaler2<Pixel>
{
public:
	SaI2xScaler(SDL_PixelFormat* format);
	virtual void scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale512(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);

private:
	void scaleLine256(
		const Pixel* srcLine0, const Pixel* srcLine1,
		const Pixel* srcLine2, const Pixel* srcLine3,
		Pixel* dstUpper, Pixel* dstLower);
	void scaleLine512(
		const Pixel* srcLine0, const Pixel* srcLine1,
		const Pixel* srcLine2, const Pixel* srcLine3,
		Pixel* dstUpper, Pixel* dstLower);
};

} // namespace openmsx

#endif

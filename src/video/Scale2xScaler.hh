// $Id$

#ifndef __SCALE2XSCALER_HH__
#define __SCALE2XSCALER_HH__

#include "Scaler.hh"


namespace openmsx {

/** Runs the Scale2x scaler algorithm.
  */
template <class Pixel>
class Scale2xScaler: public Scaler<Pixel>
{
public:
	virtual void scale256(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);
	virtual void scale512(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);
private:
	void scaleLine256Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
	void scaleLine512Half(Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2);
};

} // namespace openmsx

#endif // __SCALE2XSCALER_HH__

// $Id$

#ifndef __SCALE2XSCALER_HH__
#define __SCALE2XSCALER_HH__

#include "Scalers.hh"

namespace openmsx {

/** Runs the Scale2x scaler algorithm.
  */
template <class Pixel>
class Scale2xScaler: public SimpleScaler<Pixel>
{
public:
	Scale2xScaler();
	void scaleLine256(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
	//void scaleLine512(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
private:
	void scaleLine256Half(
		Pixel* dst,
		const Pixel* src0, const Pixel* src1, const Pixel* src2,
		int count
		);
};

} // namespace openmsx

#endif // __SCALE2XSCALER_HH__

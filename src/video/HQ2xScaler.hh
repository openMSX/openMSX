// $Id$

#ifndef __HQ2XSCALER_HH__
#define __HQ2XSCALER_HH__

#include "Scalers.hh"
#include "openmsx.hh"

namespace openmsx {

typedef word Pix16;
typedef unsigned int Pix32;

/** Runs the hq2x scaler algorithm.
  */
template <class Pixel>
class HQ2xScaler: public SimpleScaler<Pixel>
{
public:
	HQ2xScaler();
	void scaleLine256(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
	//void scaleLine512(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
private:

	Pix32 LUT16to32[65536];
	int RGBtoYUV[65536];

	static inline Pix16 readPixel(Pixel *pIn);
	static inline void pset(Pixel *pOut, Pix32 colour);
	inline bool edge(Pix16 w1, Pix16 w2);
};

} // namespace openmsx

#endif // __HQ2XSCALER_HH__

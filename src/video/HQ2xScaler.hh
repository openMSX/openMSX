// $Id$

#ifndef __HQ2XSCALER_HH__
#define __HQ2XSCALER_HH__

#include "Scaler.hh"
#include "openmsx.hh"


namespace openmsx {

typedef unsigned int Pix32;

/** Runs the hq2x scaler algorithm.
  */
template <class Pixel>
class HQ2xScaler: public Scaler<Pixel>
{
public:
	virtual void scale256(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );
	virtual void scale512(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );
private:

	static inline Pix32 readPixel(const Pixel* pIn);
	static inline void pset(Pixel* pOut, Pix32 colour);
	inline bool edge(Pix32 c1, Pix32 c2);
	void scaleLine256(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY,
		const int prevLine, const int nextLine );
	void scaleLine512(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY,
		const int prevLine, const int nextLine );
};

} // namespace openmsx

#endif // __HQ2XSCALER_HH__

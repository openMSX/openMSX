// $Id$

#ifndef __SIMPLESCALER_HH__
#define __SIMPLESCALER_HH__

#include "Scaler.hh"


namespace openmsx {

/** Scaler which assigns the colour of the original pixel to all pixels in
  * the 2x2 square.
  */
template <class Pixel>
class SimpleScaler: public Scaler<Pixel>
{
public:
	SimpleScaler();
	void scaleLine256(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
	void scaleLine512(SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
private:
	/** Copies the given line.
	  * @param src Source: surface to copy from.
	  * @param srcY Line number on source surface.
	  * @param dst Destination: surface to copy to.
	  * @param dstY Line number on destination surface.
	  */
	inline void copyLine(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY );
};

} // namespace openmsx

#endif // __SIMPLESCALER_HH__

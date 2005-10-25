// $Id$

#ifndef BLENDER_HH
#define BLENDER_HH

#include <SDL.h>

namespace openmsx {

/** Utility class for blending colors.
  */
template <class Pixel> class Blender
{
public:
	/** Create a blender which operates on the provided SDL screen format.
	  */
	Blender(const SDL_PixelFormat* format)
	{
		int rBit = ~(format->Rmask << 1) & format->Rmask;
		int gBit = ~(format->Gmask << 1) & format->Gmask;
		int bBit = ~(format->Bmask << 1) & format->Bmask;
		blendMask = rBit | gBit | bBit;
	}

	/** Blend the given colors into a single color.
	  */
	inline Pixel blend(Pixel color1, Pixel color2) const
	{
		return ((color1 & ~blendMask) >> 1) +
		       ((color2 & ~blendMask) >> 1) +
		        (color1 & blendMask);
	}

	inline Pixel getMask() const
	{
		return blendMask;
	}

private:
	/** Mask used for blending.
	  * The least significant bit of R,G,B must be 1,
	  * all other bits must be 0.
	  *     0000BBBBGGGGRRRR
	  * --> 0000000100010001
	  */
	Pixel blendMask;
};

} // namespace openmsx

#endif

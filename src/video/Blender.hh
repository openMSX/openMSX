// $Id$

#ifndef __BLENDER_HH__
#define __BLENDER_HH__

#include <SDL.h>

namespace openmsx {

/** Small utility class for blending colours.
  * The blender can be applied to 8bpp pixels safely,
  * but in that case it doesn't actually blend.
  */
template <class Pixel>
class Blender
{
private:
	/** Mask used for blending.
	  * The least significant bit of R,G,B must be 1,
	  * all other bits must be 0.
	  *     0000BBBBGGGGRRRR 
	  * --> 0000000100010001
	  */
	Pixel blendMask;

	/** Use factory methods instead.
	  */
	Blender(Pixel blendMask) {
		this->blendMask = blendMask;
	}

public:

	/** A dummy blender which can be used if you need to pass a blender object
	  * but don't intend to actually blend anything.
	  */
	static Blender dummy() {
		return Blender(0);
	}

	/** Create a blender which uses the given pixel spec.
	  * @param pixelSpec
	  * 	Specification of the pixel format.
	  * 	The least significant bit of R,G,B must be 1,
	  * 	all other bits must be 0.
	  */
	static Blender createFromMask(Pixel pixelSpec) {
		return Blender(pixelSpec);
	}

	/** Create a blender which operates on the provided SDL screen format.
	  */
	static Blender createFromFormat(const SDL_PixelFormat *format) {
		// Calculate blendMask.
		int rBit = ~(format->Rmask << 1) & format->Rmask;
		int gBit = ~(format->Gmask << 1) & format->Gmask;
		int bBit = ~(format->Bmask << 1) & format->Bmask;
		return Blender(rBit | gBit | bBit);
	}

	/** Blend the given colours into a single colour.
	  */
	Pixel blend(Pixel colour1, Pixel colour2) {
		return
			((colour1 & ~blendMask) >> 1) + ((colour2 & ~blendMask) >> 1)
			+ (colour1 & blendMask);
	}

};

} // namespace openmsx

#endif // __BLENDER_HH__

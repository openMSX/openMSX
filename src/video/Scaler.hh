// $Id$

#ifndef __SCALER_HH__
#define __SCALER_HH__

#include "openmsx.hh"
#include <SDL.h>
#include <cassert>
#include <memory>

using std::auto_ptr;


namespace openmsx {

/** Enumeration of Scalers known to openMSX.
  */
enum ScalerID {
	/** SimpleScaler. */
	SCALER_SIMPLE,
	/** SaI2xScaler. */
	SCALER_SAI2X,
	/** Scale2xScaler. */
	SCALER_SCALE2X,
	/** HQ2xScaler. */
	SCALER_HQ2X,
};

/** Abstract base class for scalers.
  * A scaler is an algorithm that converts low-res graphics to hi-res graphics.
  */
template <class Pixel>
class Scaler
{
public:
	virtual ~Scaler() {}

	/** Instantiates a Scaler.
	  * @param id Identifies the scaler algorithm.
	  * @param format Pixel format of the surfaces the scaler will be used on.
	  * @return A Scaler object, owned by the caller.
	  */
	static auto_ptr<Scaler> createScaler(ScalerID id, SDL_PixelFormat* format);

	/** Scales the given area.
	  * The default implementation scales each pixel to a 2x2 square.
	  * @param src Source: the image to be scaled.
	  *   It should be 320 pixels wide.
	  * @param srcY Y-coordinate of the top source line (inclusive).
	  * @param endSrcY Y-coordinate of the bottom source line (exclusive).
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide and twice as high as the source image.
	  * @param dstY Y-coordinate of the top destination line.
	  *   Note: The scaler must be able to handle the case where dstY is
	  *         inside the destination surface, but dstY + 1 is not.
	  */
	virtual void scale256(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );

	/** Scales the given area.
	  * The default implementation scales each pixel to a 1x2 rectangle.
	  * @param src Source: the image to be scaled.
	  *   It should be 640 pixels wide.
	  * @param srcY Y-coordinate of the top source line (inclusive).
	  * @param endSrcY Y-coordinate of the bottom source line (exclusive).
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide and twice as high as the source image.
	  * @param dstY Y-coordinate of the top destination line.
	  *   Note: The scaler must be able to handle the case where dstY is
	  *         inside the destination surface, but dstY + 1 is not.
	  */
	virtual void scale512(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );

protected:
	Scaler();

	inline static Pixel* linePtr(SDL_Surface* surface, int y) {
		assert(0 <= y && y < surface->h);
		return (Pixel*)((byte*)surface->pixels + y * surface->pitch);
	}

	/** Copies the given line.
	  * @param src Source: surface to copy from.
	  * @param srcY Line number on source surface.
	  * @param dst Destination: surface to copy to.
	  * @param dstY Line number on destination surface.
	  */
	static void copyLine(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY );
};

} // namespace openmsx

#endif // __SCALER_HH__

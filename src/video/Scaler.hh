// $Id$

#ifndef SCALER_HH
#define SCALER_HH

#include "openmsx.hh"
#include <SDL.h>
#include <cassert>
#include <memory>

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
	/** HQ2xLiteScaler. */
	SCALER_HQ2XLITE,
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
	static std::auto_ptr<Scaler> createScaler(ScalerID id, SDL_PixelFormat* format);

	/** Fills the given area, which contains only a single colour.
	  * @param colour Colour the area should be filled with.
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide.
	  * @param dstY Y-coordinate of the top destination line.
	  * @param endDstY Y-coordinate of the bottom destination line (exclusive).
	  */
	virtual void scaleBlank(
		Pixel colour,
		SDL_Surface* dst, int dstY, int endDstY );

	/** Scales the given area 200% horizontally and vertically.
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

	/** Scales the given area 200% vertically.
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

	// Utility methods  (put in seperate class?)
	
	/** Get the start address of a line in a surface
	 */ 
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
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
	
	/** Copies the given line.
	  * @param pIn ptr to start of source line
	  * @param pOut ptr to start of destination line
	  * @param width the width of the line
	  * @param inCache Indicates wheter the destination line should be
	  *                cached. Not caching is (a lot) faster when the
	  *                destination line is no needed later in the algorithm.
	  *                Otherwise caching is faster.
	  */
	static void copyLine(const Pixel* pIn, Pixel* pOut, unsigned width,
	                     bool inCache = false);

	/** Scales the given line a factor 2 horizontally.
	  * @param src Source: surface to copy from.
	  * @param srcY Line number on source surface.
	  * @param dst Destination: surface to copy to.
	  * @param dstY Line number on destination surface.
	  */
	static void scaleLine(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY);
	
	/** Scales the given line a factor 2 horizontally.
	  * @param pIn ptr to start of source line
	  * @param pOut ptr to start of destination line
	  * @param width the width of the source line
	  * @param inCache Indicates wheter the destination line should be
	  *                cached. Not caching is (a lot) faster when the
	  *                destination line is no needed later in the algorithm.
	  *                Otherwise caching is faster.
	  */
	static void scaleLine(const Pixel* pIn, Pixel* pOut, unsigned width,
	                      bool inCache = false);

	/** Fill a line with a constant colour
	 *  @param pOut ptr to start of line (typically obtained from linePtr())
	 *  @param colour the fill colour
	 *  @param width the width of the line (typically 320 or 640)
	 */
	static void fillLine(Pixel* pOut, Pixel colour, unsigned width);

protected:
	Scaler();
};

} // namespace openmsx

#endif

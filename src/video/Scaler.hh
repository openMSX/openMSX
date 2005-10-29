// $Id$

#ifndef SCALER_HH
#define SCALER_HH

#include "openmsx.hh"
#include <SDL.h>
#include <cassert>
#include <memory>

namespace openmsx {

class RenderSettings;
class FrameSource;

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
	/** HQ3xScaler. */
	SCALER_HQ3X,
	/** HQ2xLiteScaler. */
	SCALER_HQ2XLITE,
	/** HQ3xLiteScaler. */
	SCALER_HQ3XLITE,
	/** RGBTriplet3xScaler. */
	SCALER_RGBTRIPLET3X,
	/** SimpleScaler3x. */
	SCALER_SIMPLE3X,
	/** Low resolution (320x240) scaler */
	SCALER_LOW
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
	static std::auto_ptr<Scaler> createScaler(
		ScalerID id, SDL_PixelFormat* format,
		RenderSettings& renderSettings);

	/** Fills the given area, which contains only a single color.
	  * @param color Color the area should be filled with.
	  * @param dst Destination: image to store the scaled output in.
	  * @param startY Destination Y-coordinate of the top line.
	  * @param endY Destination Y-coordinate of the bottom line (exclusive).
	  */
	virtual void scaleBlank(Pixel color, SDL_Surface* dst,
	                        unsigned startY, unsigned endY);

	/** Scales the given area. Scaling factor depends on the concrete scaler
	  * The default implementation scales each pixel to a 2x2 square.
	  * @param src Source: the image to be scaled.
	  *   It should be 320 pixels wide.
	  * @param srcStartY Y-coordinate of the top source line (inclusive).
	  * @param srcEndY Y-coordinate of the bottom source line (exclusive).
	  * @param dst Destination: image to store the scaled output in.
	  * @param dstStartY Y-coordinate of the top destination line (inclusive).
	  * @param dstEndY Y-coordinate of the bottom destination line (exclusive).
	  */
	virtual void scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY) = 0;
	virtual void scale256(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;

	/** Scales the given area. Scaling factor depends on the concrete scaler
	  * The default implementation scales each pixel to a 1x2 rectangle.
	  * @param src Source: the image to be scaled.
	  *   It should be 640 pixels wide.
	  * @param dst Destination: image to store the scaled output in.
	  * @param startY Y-coordinate of the top source line (inclusive).
	  * @param endY Y-coordinate of the bottom source line (exclusive).
	  * @param lower True iff frame must be displayed half a line lower
	  */
	virtual void scale512(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY) = 0;
	virtual void scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;

	virtual void scale192(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY) = 0;
	virtual void scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;

	virtual void scale384(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY) = 0;
	virtual void scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;

	virtual void scale640(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY) = 0;
	virtual void scale640(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;

	virtual void scale768(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY) = 0;
	virtual void scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;

	virtual void scale1024(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY) = 0;
	virtual void scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                       unsigned startY, unsigned endY) = 0;


	/** Get the start address of a line in a surface
	  *  TODO should become a method of the SDL_Surface replacement
	  *       in the future
	  */
	inline static Pixel* linePtr(SDL_Surface* surface, unsigned y) {
		assert(y < (unsigned)surface->h);
		return (Pixel*)((byte*)surface->pixels + y * surface->pitch);
	}
};

} // namespace openmsx

#endif

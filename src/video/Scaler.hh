// $Id$

#ifndef __SCALER_HH__
#define __SCALER_HH__

#include <SDL/SDL.h>


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
	static Scaler* createScaler(ScalerID id, SDL_PixelFormat* format);

	/** Scales the given line.
	  * @param src Source: the image to be scaled.
	  *   It should be 320 pixels wide.
	  * @param srcY Y-coordinate of the source line.
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide and twice as high as the source image.
	  * @param dstY Y-coordinate of the destination line.
	  *   Note: The scaler must be able to handle the case where dstY is
	  *         inside the destination surface, but dstY + 1 is not.
	  */
	virtual void scaleLine256(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY ) = 0;

	/** Scales the given line.
	  * @param src Source: the image to be scaled.
	  *   It should be 640 pixels wide.
	  * @param srcY Y-coordinate of the source line.
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide and twice as high as the source image.
	  * @param dstY Y-coordinate of the destination line.
	  *   Note: The scaler must be able to handle the case where dstY is
	  *         inside the destination surface, but dstY + 1 is not.
	  */
	virtual void scaleLine512(
		SDL_Surface* src, int srcY, SDL_Surface* dst, int dstY ) = 0;

protected:
	Scaler();
};

} // namespace openmsx

#endif // __SCALER_HH__

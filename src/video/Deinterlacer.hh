// $Id$

#ifndef __DEINTERLACER_HH__
#define __DEINTERLACER_HH__

struct SDL_Surface;

namespace openmsx {

/** Deinterlace image filter: takes lines from the even and odd page to
  * produce a single deinterlaced output frame.
  * Its interface is similar to a scaler, however there are two input
  * surfaces (even and odd page) instead of one.
  */
template <class Pixel>
class Deinterlacer
{
public:
	/** Deinterlaces the given line.
	  * @param src0 Source 0: even page of the image to be scaled.
	  *   It should be 320 pixels wide.
	  * @param src1 Source 1: odd page of the image to be scaled.
	  *   It should be 320 pixels wide.
	  * @param srcY Y-coordinate of the source line.
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide and twice as high as the source image.
	  * @param dstY Y-coordinate of the destination line.
	  */
	void deinterlaceLine256(
		SDL_Surface* src0, SDL_Surface* src1, int srcY,
		SDL_Surface* dst, int dstY);

	/** Deinterlaces the given line.
	  * @param src0 Source 0: even page of the image to be deinterlaced.
	  *   It should be 320 pixels wide.
	  * @param src1 Source 1: odd page of the image to be deinterlaced.
	  *   It should be 320 pixels wide.
	  * @param srcY Y-coordinate of the source line.
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide and twice as high as the source image.
	  * @param dstY Y-coordinate of the destination line.
	  */
	void deinterlaceLine512(
		SDL_Surface* src0, SDL_Surface* src1, int srcY,
		SDL_Surface* dst, int dstY);
};

} // namespace openmsx

#endif // __DEINTERLACER_HH__

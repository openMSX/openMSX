// $Id$

#ifndef DEINTERLACER_HH
#define DEINTERLACER_HH

struct SDL_Surface;

namespace openmsx {

class RawFrame;

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
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide and twice as high as the source image.
	  * @param y Y-coordinate
	  */
	void deinterlaceLine256(
		RawFrame& src0, RawFrame& src1, SDL_Surface* dst, unsigned y);

	/** Deinterlaces the given line.
	  * @param src0 Source 0: even page of the image to be deinterlaced.
	  *   It should be 640 pixels wide.
	  * @param src1 Source 1: odd page of the image to be deinterlaced.
	  *   It should be 640 pixels wide.
	  * @param dst Destination: image to store the scaled output in.
	  *   It should be 640 pixels wide and twice as high as the source image.
	  * @param y Y-coordinate
	  */
	void deinterlaceLine512(
		RawFrame& src0, RawFrame& src1, SDL_Surface* dst, unsigned y);
};

} // namespace openmsx

#endif

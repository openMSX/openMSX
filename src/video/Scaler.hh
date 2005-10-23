// $Id$

#ifndef SCALER_HH
#define SCALER_HH

#include "Blender.hh"
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
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY
		) = 0;
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
	virtual void scale512(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower) = 0;
	virtual void scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;

	virtual void scale192(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower) = 0;
	virtual void scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;
	virtual void scale384(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower) = 0;
	virtual void scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;
	virtual void scale640(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower) = 0;
	virtual void scale640(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;
	virtual void scale768(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower) = 0;
	virtual void scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY) = 0;
	virtual void scale1024(FrameSource& src, SDL_Surface* dst,
	                       unsigned startY, unsigned endY, bool lower) = 0;
	virtual void scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                       unsigned startY, unsigned endY) = 0;


	// Utility methods

	/** Get the start address of a line in a surface
	 */
	inline static Pixel* linePtr(SDL_Surface* surface, unsigned y) {
		assert(y < (unsigned)surface->h);
		return (Pixel*)((byte*)surface->pixels + y * surface->pitch);
	}

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

	// TODO optimize these routines
	void scale_1on3(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_1on2(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_1on1(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_2on1(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_4on1(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_2on3(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_4on3(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_8on3(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_2on9(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_4on9(const Pixel* inPixels, Pixel* outPixels, int nrPixels);
	void scale_8on9(const Pixel* inPixels, Pixel* outPixels, int nrPixels);


protected:
	Scaler(SDL_PixelFormat* format);

	/** Return the average of two pixels
	  */
	inline Pixel blend(Pixel p1, Pixel p2) {
		return blender.blend(p1, p2);
	}

	/** Calculate the average of two input lines.
	  * @param pIn0 First input line
	  * @param pIn1 Second input line
	  * @param pOut Output line
	  * @param width Width of the lines in pixels
	  */
	void average(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut,
                     unsigned width);

	/** Make the input line halve as wide
	  * @param pIn Input line
	  * @param pOut Output line
	  * @param width Width of the output line in pixels
	  */
	void halve(const Pixel* pIn, Pixel* pOut, unsigned width);

	SDL_PixelFormat format;
	Blender<Pixel> blender;

private:
	inline unsigned red(Pixel pixel);
	inline unsigned green(Pixel pixel);
	inline unsigned blue(Pixel pixel);
	inline Pixel combine(unsigned r, unsigned g, unsigned b);

	template <unsigned w1, unsigned w2>
	inline Pixel blendPixels2(const Pixel* source);
	template <unsigned w1, unsigned w2, unsigned w3>
	inline Pixel blendPixels3(const Pixel* source);
	template <unsigned w1, unsigned w2, unsigned w3, unsigned w4>
	inline Pixel blendPixels4(const Pixel* source);
};

} // namespace openmsx

#endif

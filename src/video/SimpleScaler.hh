// $Id$

#ifndef SIMPLESCALER_HH
#define SIMPLESCALER_HH

#include "Scaler2.hh"
#include "Scanline.hh"

namespace openmsx {

class RenderSettings;

/**
 * Helper class to perform 'pixel x scalar' calculations.
 * Also converts the result to 32bpp.
 */
template<typename Pixel> class Multiply32;

template<> class Multiply32<word> {
public:
	Multiply32(SDL_PixelFormat* format);
	void setFactor32(unsigned factor);
	inline unsigned mul32(word p) const;
	inline word conv32(unsigned p) const;
private:
	unsigned tab[0x10000];
	unsigned factor;
	unsigned Rshift1, Gshift1, Bshift1;
	unsigned Rshift2, Gshift2, Bshift2;
	unsigned Rshift3, Gshift3, Bshift3;
	word     Rmask1,  Gmask1,  Bmask1;
	word     Rmask2,  Gmask2,  Bmask2;
};

template<> class Multiply32<unsigned> {
public:
	Multiply32(SDL_PixelFormat* format);
	inline void setFactor32(unsigned factor);
	inline unsigned mul32(unsigned p) const;
	inline unsigned conv32(unsigned p) const;
private:
	unsigned factor;
};


/** Scaler which assigns the color of the original pixel to all pixels in
  * the 2x2 square. Optionally it can draw darkended scanlines (scanline has
  * the averga color from the pixel above and below). It can also optionally
  * perform a horizontal blur.
  */
template <class Pixel>
class SimpleScaler: public Scaler2<Pixel>
{
public:
	SimpleScaler(SDL_PixelFormat* format, RenderSettings& renderSettings);

	virtual void scaleBlank(Pixel color, OutputSurface& dst,
	                        unsigned startY, unsigned endY);
	virtual void scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale512(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		OutputSurface& dst, unsigned dstStartY, unsigned dstEndY);

private:
	void drawScanline(const Pixel* in1, const Pixel* in2, Pixel* out,
	                  int factor);
	void blur256(const Pixel* pIn, Pixel* pOut, unsigned alpha);
	void blur512(const Pixel* pIn, Pixel* pOut, unsigned alpha);

	RenderSettings& settings;

	Multiply32<Pixel> mult1;
	Multiply32<Pixel> mult2;
	Multiply32<Pixel> mult3;

	Scanline<Pixel> scanline;
};

} // namespace openmsx

#endif

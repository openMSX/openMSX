// $Id$

#ifndef SIMPLESCALER_HH
#define SIMPLESCALER_HH

#include "Scaler.hh"
#include "Blender.hh"

namespace openmsx {

class IntegerSetting;

/**
 * Helper class to perform 'pixel x scalar' calculations. This is a template
 * class with specializations for word (16bpp) and unsigned (32bpp).
 */
template<typename Pixel> class Multiply;

template<> class Multiply<word> {
public:
	Multiply(SDL_PixelFormat* format);
	inline word multiply(word p, unsigned factor);
	void setFactor(unsigned factor);
	inline unsigned mul32(word p);
	inline word conv32(unsigned p);
private:
	unsigned tab[0x10000];
	unsigned factor;
	unsigned Rshift1, Gshift1, Bshift1;
	unsigned Rshift2, Gshift2, Bshift2;
	unsigned Rshift3, Gshift3, Bshift3;
	word     Rmask1,  Gmask1,  Bmask1;
	word     Rmask2,  Gmask2,  Bmask2;
};

template<> class Multiply<unsigned> {
public:
	Multiply(SDL_PixelFormat* format);
	inline unsigned multiply(unsigned p, unsigned factor);
	inline void setFactor(unsigned factor);
	inline unsigned mul32(unsigned p);
	inline unsigned conv32(unsigned p);
private:
	unsigned factor;
};

template<typename Pixel> class Darkener;

template<> class Darkener<word> {
public:
	Darkener(SDL_PixelFormat* format);
	void setFactor(unsigned f);
	word* getTable();
private:
	word tab[0x10000];
	unsigned factor;
	SDL_PixelFormat* format;
};

template<> class Darkener<unsigned> {
public:
	Darkener(SDL_PixelFormat* format);
	void setFactor(unsigned f);
	word* getTable();
};


/** Scaler which assigns the color of the original pixel to all pixels in
  * the 2x2 square. Optionally it can draw darkended scanlines (scanline has
  * the averga color from the pixel above and below). It can also optionally
  * perform a horizontal blur.
  */
template <class Pixel>
class SimpleScaler: public Scaler<Pixel>
{
public:
	SimpleScaler(SDL_PixelFormat* format);
	virtual ~SimpleScaler();

	virtual void scaleBlank(Pixel color, SDL_Surface* dst,
	                        unsigned startY, unsigned endY, bool lower);
	virtual void scale256(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale512(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);

private:
	void blur256(const Pixel* pIn, Pixel* pOut, unsigned alpha);
	void blur512(const Pixel* pIn, Pixel* pOut, unsigned alpha);
	void average(const Pixel* src1, const Pixel* src2, Pixel* dst,
	             unsigned alpha);

	IntegerSetting& scanlineSetting;
	IntegerSetting& blurSetting;

	Blender<Pixel> blender;
	Multiply<Pixel> mult1;
	Multiply<Pixel> mult2;
	Multiply<Pixel> mult3;
	Darkener<Pixel> darkener;
};

} // namespace openmsx

#endif

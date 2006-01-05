// $Id$

#ifndef SCANLINE_HH
#define SCANLINE_HH

#include "PixelOperations.hh"
#include "openmsx.hh"

namespace openmsx {

/**
 * Helper class to perform 'pixel x scalar' calculations.
 */
template<typename Pixel> class Multiply;

template<> class Multiply<word>
{
public:
	explicit Multiply(const SDL_PixelFormat* format);
	void setFactor(unsigned f);
	inline word multiply(word p, unsigned factor) const;
	inline word multiply(word p) const;
	inline const word* getTable() const;
private:
	word tab[0x10000];
	unsigned factor;
	const SDL_PixelFormat* format;
};

template<> class Multiply<unsigned>
{
public:
	explicit Multiply(const SDL_PixelFormat* format);
	void setFactor(unsigned f);
	inline unsigned multiply(unsigned p, unsigned factor) const;
	inline unsigned multiply(unsigned p) const;
	const unsigned* getTable() const;
private:
	unsigned factor;
};

/**
 * Helper class to draw scalines
 */
template <class Pixel> class Scanline
{
public:
	explicit Scanline(const PixelOperations<Pixel>& pixelOps);

	/** Draws a scanline. The scanline will be the average of the two
	  * input lines and darkened by a certain factor.
	  * @param src1 First input line.
	  * @param src2 Second input line.
	  * @param dst Output line.
	  * @param factor Darkness factor, 0 means completely black,
	  *               255 means no darkening.
	  * @param width Line width in pixels.
	  */
	void draw(const Pixel* src1, const Pixel* src2, Pixel* dst,
	          unsigned factor, unsigned width);

	/** Darken one pixel. Typically used to implement drawBlank().
	 */
	Pixel darken(Pixel p, unsigned factor);

	/** Darken and blend two pixels.
	 */
	Pixel darken(Pixel p1, Pixel p2, unsigned factor);

protected:
	Multiply<Pixel> darkener;
	PixelOperations<Pixel> pixelOps;
};

} // namespace openmsx

#endif

// $Id$

#ifndef __SIMPLESCALER_HH__
#define __SIMPLESCALER_HH__

#include "Scaler.hh"
#include "SettingListener.hh"


namespace openmsx {

class IntegerSetting;


template <typename Pixel> class Darkener;

template<> class Darkener<word> {
public:
	Darkener(SDL_PixelFormat* format);
	inline word darken(word p, unsigned factor);
	void setFactor(unsigned f);
	inline word darken(word p);
	inline unsigned darkenDouble(word p);
	inline unsigned* getTable();
private:
	SDL_PixelFormat* format;
	unsigned factor;
	unsigned tab[0x10000];
};

template<> class Darkener<unsigned> {
public:
	Darkener(SDL_PixelFormat* format);
	inline unsigned darken(unsigned p, unsigned factor);
	inline void setFactor(unsigned f);
	inline unsigned darken(unsigned p);
	inline unsigned darkenDouble(unsigned p);
	unsigned* getTable();
private:
	unsigned factor;
};



/** Scaler which assigns the colour of the original pixel to all pixels in
  * the 2x2 square.
  */
template <class Pixel>
class SimpleScaler: public Scaler<Pixel>, private SettingListener
{
public:
	SimpleScaler(SDL_PixelFormat* format);
	virtual ~SimpleScaler();
	virtual void scaleBlank(
		Pixel colour,
		SDL_Surface* dst, int dstY, int endDstY );
	virtual void scale256(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );
	virtual void scale512(
		SDL_Surface* src, int srcY, int endSrcY,
		SDL_Surface* dst, int dstY );
private:
	// SettingListener interface:
	virtual void update(const SettingLeafNode* setting);
	
	IntegerSetting* scanlineAlphaSetting;
	/** Current alpha value, range [0..255]. */
	int scanlineAlpha;

	Darkener<Pixel> darkener;
};

} // namespace openmsx

#endif // __SIMPLESCALER_HH__

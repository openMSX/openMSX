// $Id$

#ifndef _BLURSCALER_HH_
#define _BLURSCALER_HH_

#include "Scaler.hh"
#include "Blender.hh"

namespace openmsx {

class IntegerSetting;

template <class Pixel>
class BlurScaler : public Scaler<Pixel>
{
public:
	BlurScaler(SDL_PixelFormat* format);
	virtual ~BlurScaler();

	virtual void scaleBlank(Pixel colour, SDL_Surface* dst,
	                        int dstY, int endDstY);
	virtual void scale256(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);
	virtual void scale512(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);

private:
	inline Pixel multiply(Pixel pixel, unsigned factor);
	void blur256(const Pixel* pIn, Pixel* pOut, unsigned alpha);
	void blur512(const Pixel* pIn, Pixel* pOut, unsigned alpha);
	void average(const Pixel* src1, const Pixel* src2, Pixel* dst,
	             unsigned alpha);
	
	IntegerSetting& scanlineSetting;
	IntegerSetting& blurSetting;
	Blender<Pixel> blender;
	SDL_PixelFormat* format;
};

} // namespace openmsx

#endif // _BLURSCALER_HH_

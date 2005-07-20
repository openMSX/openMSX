// $Id$

#ifndef LOWSCALER_HH
#define LOWSCALER_HH

#include "Scaler.hh"
#include "Blender.hh"

namespace openmsx {

template <typename Pixel>
class LowScaler : public Scaler<Pixel>
{
public:
	LowScaler(SDL_PixelFormat* format);
	virtual ~LowScaler();

	virtual void scaleBlank(Pixel color,
	                        SDL_Surface* dst, int dstY, int endDstY);
	virtual void scale256(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);
	virtual void scale512(SDL_Surface* src, int srcY, int endSrcY,
	                      SDL_Surface* dst, int dstY);

private:
	void halve(const Pixel* pIn, Pixel* pOut);

	Blender<Pixel> blender;
};

} // namespace openmsx

#endif

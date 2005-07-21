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

	virtual void scaleBlank(Pixel color, SDL_Surface* dst,
	                        unsigned startY, unsigned endY, bool lower);
	virtual void scale256(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale512(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);

private:
	void halve(const Pixel* pIn, Pixel* pOut);

	Blender<Pixel> blender;
};

} // namespace openmsx

#endif

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
	virtual void scale256(RawFrame& src0, RawFrame& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale512(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale512(RawFrame& src0, RawFrame& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);

	virtual void scale192(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale192(RawFrame& src0, RawFrame& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale384(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale384(RawFrame& src0, RawFrame& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale768(RawFrame& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale768(RawFrame& src0, RawFrame& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale1024(RawFrame& src, SDL_Surface* dst,
	                       unsigned startY, unsigned endY, bool lower);
	virtual void scale1024(RawFrame& src0, RawFrame& src1, SDL_Surface* dst,
	                       unsigned startY, unsigned endY);

private:
	void halve       (const Pixel* pIn,                     Pixel* pOut);
	void average     (const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut);
	void averageHalve(const Pixel* pIn0, const Pixel* pIn1, Pixel* pOut);

	Blender<Pixel> blender;
};

} // namespace openmsx

#endif

// $Id$

#ifndef SIMPLE3XSCALER_HH
#define SIMPLE3XSCALER_HH

#include "Scaler3.hh"
#include "PixelOperations.hh"
#include "Scanline.hh"

namespace openmsx {

class RenderSettings;

template <class Pixel>
class Simple3xScaler : public Scaler3<Pixel>
{
public:
	Simple3xScaler(SDL_PixelFormat* format,
			const RenderSettings& renderSettings);

	virtual void scaleBlank(
                Pixel color, SDL_Surface* dst,
                unsigned startY, unsigned endY);
	
	virtual void scale192(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale384(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale512(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale640(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale768(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);

	virtual void scale1024(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);

private:
	template <typename ScaleOp>
	void doScale1(FrameSource& src, unsigned srcStartY, unsigned srcEndY,
	              SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY,
	              ScaleOp scale);
	PixelOperations<Pixel> pixelOps;
	Scanline<Pixel> scanline;
	const RenderSettings& settings;

};

} // namespace openmsx

#endif

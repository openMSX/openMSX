// $Id$

#ifndef SCALER2_HH
#define SCALER2_HH

#include "Scaler.hh"

namespace openmsx {

/** Base class for 2x scalers.
  */
template <class Pixel> class Scaler2 : public Scaler<Pixel>
{
public:
	/*virtual void scale256(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);*/
	virtual void scale256(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale512(
		FrameSource& src, unsigned srcStartY, unsigned srcEndY,
		SDL_Surface* dst, unsigned dstStartY, unsigned dstEndY);
	virtual void scale512(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale192(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale192(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale384(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale384(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale640(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale640(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale768(FrameSource& src, SDL_Surface* dst,
	                      unsigned startY, unsigned endY, bool lower);
	virtual void scale768(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                      unsigned startY, unsigned endY);
	virtual void scale1024(FrameSource& src, SDL_Surface* dst,
	                       unsigned startY, unsigned endY, bool lower);
	virtual void scale1024(FrameSource& src0, FrameSource& src1, SDL_Surface* dst,
	                       unsigned startY, unsigned endY);

protected:
	Scaler2(SDL_PixelFormat* format);
};

} // namespace openmsx

#endif

#ifndef LDSDLRASTERIZER_HH
#define LDSDLRASTERIZER_HH

#include "LDRasterizer.hh"
#include "noncopyable.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class VisibleSurface;
class RawFrame;
class PostProcessor;

/** Rasterizer using a frame buffer approach: it writes pixels to a single
  * rectangular pixel buffer.
  */
template <class Pixel>
class LDSDLRasterizer final : public LDRasterizer, private noncopyable
{
public:
	LDSDLRasterizer(
		VisibleSurface& screen,
		std::unique_ptr<PostProcessor> postProcessor);
	~LDSDLRasterizer();

	// Rasterizer interface:
	virtual PostProcessor* getPostProcessor() const;
	virtual void frameStart(EmuTime::param time);
	virtual void drawBlank(int r, int g, int b);
	virtual RawFrame* getRawFrame();

private:
	/** The video post processor which displays the frames produced by this
	  *  rasterizer.
	  */
	const std::unique_ptr<PostProcessor> postProcessor;

	/** The next frame as it is delivered by the VDP, work in progress.
	  */
	std::unique_ptr<RawFrame> workFrame;

	const SDL_PixelFormat pixelFormat;
};

} // namespace openmsx

#endif

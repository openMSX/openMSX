// $Id$

#ifndef LDPIXELRENDERER_HH
#define LDPIXELRENDERER_HH

#include "LDRenderer.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class EventDistributor;
class Display;
class LDRasterizer;
class LaserdiscPlayer;

/** Generic implementation of a pixel-based Renderer.
  * Uses a Rasterizer to plot actual pixels for a specific video system.
  */
class LDPixelRenderer : public LDRenderer, private noncopyable
{
public:
	LDPixelRenderer(LaserdiscPlayer& ld, Display& display);
	virtual ~LDPixelRenderer();

	// Renderer interface:
	virtual void frameStart(EmuTime::param time);
	virtual void frameEnd(EmuTime::param time);
	virtual void drawBlank(int r, int g, int b);
	virtual void drawBitmap(const byte* frame);

private:
	EventDistributor& eventDistributor;
	const std::auto_ptr<LDRasterizer> rasterizer;
};

} // namespace openmsx

#endif

// $Id$

#ifndef __V9990SDLRASTERIZER_HH__
#define __V9990SDLRASTERIZER_HH__

#include "V9990Rasterizer.hh"
#include "Renderer.hh"


namespace openmsx {

class V9990;
class V9990VRAM;


/** Rasterizer using SDL.
  */
template <class Pixel, Renderer::Zoom zoom>
class V9990SDLRasterizer : public V9990Rasterizer
{
public:
	/** Constructor.
	  */
	V9990SDLRasterizer(V9990* vdp, SDL_Surface* screen);

	/** Destructor.
	  */
	virtual ~V9990SDLRasterizer();

	// Layer interface:
	virtual void paint();
	virtual const string& getName();

	// Rasterizer interface:
	virtual void reset();
	virtual void frameStart();
	virtual void frameEnd();
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(int fromX, int fromY,
		int displayX, int displayY, int displayWidth, int displayHeight);

private:
	/** The VDP of which the video output is being rendered.
	  */
	V9990* vdp;

	/** The VRAM whose contents are rendered.
	  */
	V9990VRAM* vram;

	/** The surface which is visible to the user.
	  */
	SDL_Surface* screen;

};

} // namespace openmsx

#endif //__V9990SDLRASTERIZER_HH__

// $Id$

#ifndef __SDLLORENDERER_HH__
#define __SDLLORENDERER_HH__

#include <SDL/SDL.h>
#include "openmsx.hh"
#include "Renderer.hh"


class MSXTMS9928a;

/** Factory method to create SDLLoRenderer objects.
  */
Renderer *createSDLLoRenderer(MSXTMS9928a *vdp, bool fullScreen);

/** Low-res (320x240) renderer on SDL.
  */
template <class Pixel> class SDLLoRenderer : public Renderer
{
	// TODO: gcc gives a warning for this, but I don't think
	//   there is anything wrong or even suspicious about it.
	friend Renderer *createSDLLoRenderer(MSXTMS9928a *, bool);
public:
	virtual ~SDLLoRenderer();

	void toggleFullScreen();

	/** Put an image on the screen.
	  */
	void putImage();

	void fullScreenRefresh();

private:
	typedef void (SDLLoRenderer::*RenderMethod)(Pixel *pixelPtr, int line);
	//typedef void (SDLLoRenderer::*RenderMethod)(void *pixelPtr, int line);
	static RenderMethod modeToRenderMethod[];

	static const int WIDTH = 320;
	static const int HEIGHT = 240;

	/** Private: use the createSDLLoRenderer factory method instead.
	  */
	SDLLoRenderer(MSXTMS9928a *vdp, SDL_Surface *screen);

	void mode0(Pixel *pixelPtr, int line);
	void mode1(Pixel *pixelPtr, int line);
	void mode2(Pixel *pixelPtr, int line);
	void mode12(Pixel *pixelPtr, int line);
	void mode3(Pixel *pixelPtr, int line);
	void modebogus(Pixel *pixelPtr, int line);
	void mode23(Pixel *pixelPtr, int line);
	void modeblank(Pixel *pixelPtr, int line);

	/** Draw sprites on this line over the background.
	  * @param dirty 32-entry array that stores which characters are
	  *   covered with sprites and must therefore be redrawn next frame.
	  *   This method will update the array according to the sprites drawn.
	  * @return Were any pixels drawn?
	  */
	bool drawSprites(Pixel *pixelPtr, int line, bool *dirty);

	void drawEmptyLine(Pixel *linePtr, Pixel colour);
	void drawBorders(Pixel *linePtr, Pixel colour,
		int displayStart, int displayStop);

	MSXTMS9928a *vdp;
	Pixel XPal[16];
	Pixel currBorderColours[HEIGHT];

	/** The surface which is visible to the user.
	  */
	SDL_Surface *screen;
	/** The surface which the image is rendered on.
	  */
	SDL_Surface *canvas;
	/** Pointers to the start of each line.
	  */
	Pixel *linePtrs[HEIGHT];
};

/*
#ifndef __SDLLORENDERER_CC__
#include "SDLLoRenderer.cc"
#endif //__SDLLORENDERER_CC__
*/

#endif //__SDLLORENDERER_HH__


// $Id$

#ifndef __SDLLORENDERER_HH__
#define __SDLLORENDERER_HH__

#include <SDL/SDL.h>
#include "MSXTMS9928a.hh"



#define WIDTH 320
#define HEIGHT 240
#define DEPTH 8

#if DEPTH == 8
typedef byte Pixel;
#elif DEPTH == 15 || DEPTH == 16
typedef word Pixel;
#elif DEPTH == 32
typedef unsigned int Pixel;
#else
#error DEPTH must be 8, 15, 16 or 32 bits per pixel.
#endif

class MSXTMS9928a;

// TODO: Introduce an abstract base class to SDLLoRenderer.
class SDLLoRenderer
{
public:
	typedef void (SDLLoRenderer::*RenderMethod)(Pixel *pixelPtr, int line);
	static RenderMethod modeToRenderMethod[];

	SDLLoRenderer(MSXTMS9928a *vdp, bool fullScreen);
	~SDLLoRenderer();
	void toggleFullScreen();
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

	/** Put an image on the screen.
	  */
	void putImage();

	void fullScreenRefresh();

private:
	MSXTMS9928a *vdp;

	Pixel XPal[16];
	Pixel currBorderColours[HEIGHT];

	SDL_Surface *screen;

	/** Actual pixel data.
	  */
	Pixel pixelData[WIDTH * HEIGHT];
	/** Pointers to the start of each line.
	  */
	Pixel *linePtrs[HEIGHT];
};

#endif //__SDLLORENDERER_HH__


// $Id$

#ifndef __SDLVIDEOSYSTEM_HH__
#define __SDLVIDEOSYSTEM_HH__

#include "VideoSystem.hh"
#include "RendererFactory.hh"

struct SDL_Surface;


namespace openmsx {

class VDP;
class Rasterizer;
class V9990;
class V9990Rasterizer;

class SDLVideoSystem: public VideoSystem
{
public:
	SDLVideoSystem(RendererFactory::RendererID id);
	virtual ~SDLVideoSystem();

	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP* vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990* vdp);
	virtual bool checkSettings();
	virtual bool prepare();
	virtual void flush();
	virtual void takeScreenShot(const string& filename);

private:
	RendererFactory::RendererID id;
	SDL_Surface* screen;
};

} // namespace openmsx

#endif // __SDLVIDEOSYSTEM_HH__

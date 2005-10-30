// $Id$

#ifndef SDLVIDEOSYSTEM_HH
#define SDLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include <memory>

struct SDL_Surface;

namespace openmsx {

class MSXMotherBoard;
class RenderSettings;
class Display;
class VDP;
class Rasterizer;
class V9990;
class V9990Rasterizer;
class Layer;

class SDLVideoSystem: public VideoSystem
{
public:
	SDLVideoSystem(MSXMotherBoard& motherboard);
	virtual ~SDLVideoSystem();

	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP& vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
	virtual bool checkSettings();
	virtual bool prepare();
	virtual void flush();
	virtual void takeScreenShot(const std::string& filename);

private:
	void getWindowSize(unsigned& width, unsigned& height);

	RenderSettings& renderSettings;
	Display& display;
	SDL_Surface* screen;
	std::auto_ptr<Layer> snowLayer;
	std::auto_ptr<Layer> console;
	std::auto_ptr<Layer> iconLayer;
};

} // namespace openmsx

#endif

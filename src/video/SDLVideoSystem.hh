// $Id$

#ifndef SDLVIDEOSYSTEM_HH
#define SDLVIDEOSYSTEM_HH

#include "VideoSystem.hh"

struct SDL_Surface;

namespace openmsx {

class UserInputEventDistributor;
class RenderSettings;
class Console;
class Display;
class VDP;
class Rasterizer;
class V9990;
class V9990Rasterizer;

class SDLVideoSystem: public VideoSystem
{
public:
	SDLVideoSystem(UserInputEventDistributor& userInputEventDistributor,
	               RenderSettings& renderSettings_,
	               Console& console, Display& display);
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
};

} // namespace openmsx

#endif

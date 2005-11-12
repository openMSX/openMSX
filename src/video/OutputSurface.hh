// $Id$

#ifndef OUTPUTSURFACE_HH
#define OUTPUTSURFACE_HH

#include <string>
#include <memory>
#include <cassert>
#include <SDL.h>

namespace openmsx {

class Layer;
class MSXMotherBoard;
class CommandController;
class EventDistributor;
class Display;

class OutputSurface
{
public:
	virtual ~OutputSurface();

	unsigned getWidth() const  { return surface->w; }
	unsigned getHeight() const { return surface->h; }
	SDL_PixelFormat* getFormat() { return &format; }

	template <typename Pixel>
	Pixel* getLinePtr(unsigned y, Pixel* /*dummy*/) {
		return (Pixel*)(data + y * pitch);
	}

	void setWindowTitle(const std::string& title);
	bool setFullScreen(bool fullscreen);
	virtual bool init() = 0;
	virtual void drawFrameBuffer() = 0;
	virtual void finish() = 0;
	virtual void takeScreenShot(const std::string& filename) = 0;

	virtual std::auto_ptr<Layer> createSnowLayer() = 0;
	virtual std::auto_ptr<Layer> createConsoleLayer(
		MSXMotherBoard& motherboard) = 0;
	virtual std::auto_ptr<Layer> createIconLayer(
		CommandController& commandController,
		EventDistributor& eventDistributor, Display& display) = 0;

protected:
	OutputSurface();
	void createSurface(unsigned width, unsigned height, int flags);

	SDL_Surface* surface;
	SDL_PixelFormat format;
	char* data;
	unsigned pitch;
};

} // namespace openmsx

#endif

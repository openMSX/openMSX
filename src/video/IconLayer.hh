// $Id$

#ifndef _ICONLAYER_HH_
#define _ICONLAYER_HH_

#include "Display.hh"
#include "EventListener.hh"
#include "LedEvent.hh"
#include <memory>

class SDL_Surface;

namespace openmsx {

template <class IMAGE>
class IconLayer : public Layer, private EventListener
{
public:
	IconLayer(SDL_Surface* screen);
	virtual ~IconLayer();

	// Layer interface:
	virtual void paint();
	virtual const string& getName();

private:
	// EventListener interface:
	virtual bool signalEvent(const Event& event);

	SDL_Surface* outputScreen;
	std::auto_ptr<IMAGE> iconOn;
	std::auto_ptr<IMAGE> iconOff;
};

class SDLImage;
class GLImage;
typedef IconLayer<SDLImage> SDLIconLayer;
typedef IconLayer<GLImage>  GLIconLayer;

} // namespace openmsx

#endif

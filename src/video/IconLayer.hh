// $Id$

#ifndef _ICONLAYER_HH_
#define _ICONLAYER_HH_

#include "Display.hh"
#include "EventListener.hh"
#include "LedEvent.hh"

class SDL_Surface;

namespace openmsx {

template <class IMAGE>
class IconLayer : public Layer, private EventListener
{
public:
	IconLayer(SDL_Surface* screen);
	virtual ~IconLayer();

	virtual void paint();
	virtual const string& getName();

private:
	// EventListener
	virtual bool signalEvent(const Event& event);

	SDL_Surface* outputScreen;
	IMAGE* icon;
};

class SDLImage;
class GLImage;
typedef IconLayer<SDLImage> SDLIconLayer;
typedef IconLayer<GLImage>  GLIconLayer;

} // namespace openmsx

#endif

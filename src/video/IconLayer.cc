// $Id$

#include "IconLayer.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "SDLImage.hh"
#include "GLImage.hh"
#include <SDL.h>

namespace openmsx {

// Force template instantiation
template class IconLayer<SDLImage>;
template class IconLayer<GLImage>;


static bool init = false;
static bool ledStatus[LedEvent::NUM_LEDS];


template <class IMAGE>
IconLayer<IMAGE>::IconLayer(SDL_Surface* screen)
	: outputScreen(screen)
{
	if (!init) {
		init = true;
		memset(ledStatus, 0, sizeof(ledStatus));
	}
	try {
		SystemFileContext context;
		icon = new IMAGE(screen, context.resolve("skins/led.png"));
	} catch (FileException& e) {
		icon = NULL;
	}
	EventDistributor::instance().registerEventListener(LED_EVENT, *this);
}

template <class IMAGE>
IconLayer<IMAGE>::~IconLayer()
{
	EventDistributor::instance().unregisterEventListener(LED_EVENT, *this);
	delete icon;
}

template <class IMAGE>
void IconLayer<IMAGE>::paint()
{
	for (int i = 0; i < LedEvent::NUM_LEDS; ++i) {
		if (ledStatus[i] && icon) {
			icon->draw(50 * i, 0);
		}
	}
}

template <class IMAGE>
const string& IconLayer<IMAGE>::getName()
{
	static const string NAME = "icon layer";
	return NAME;
}

template <class IMAGE>
bool IconLayer<IMAGE>::signalEvent(const Event& event)
{
	assert(event.getType() == LED_EVENT);
	const LedEvent& ledEvent = static_cast<const LedEvent&>(event);
	ledStatus[ledEvent.getLed()] = ledEvent.getStatus();
	return true;
}

} // namespace openmsx

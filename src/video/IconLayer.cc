// $Id$

#include "IconLayer.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "Timer.hh"
#include "SDLImage.hh"
#include "components.hh"
#ifdef COMPONENT_GL
#include "GLImage.hh"
#endif
#include <SDL.h>

namespace openmsx {

// Force template instantiation
template class IconLayer<SDLImage>;
#ifdef COMPONENT_GL
template class IconLayer<GLImage>;
#endif


static bool init = false;
static bool ledStatus[LedEvent::NUM_LEDS];
static unsigned long long ledTime[LedEvent::NUM_LEDS];

static const unsigned long long fadeTime = 5000000;
static const unsigned long long fadeDuration = 5000000;

template <class IMAGE>
IconLayer<IMAGE>::IconLayer(SDL_Surface* screen)
	// Just assume partial coverage and let paint() sort it out.
	: Layer(COVER_PARTIAL, Z_ICONS)
	, outputScreen(screen)
{
	if (!init) {
		init = true;
		unsigned long long now = Timer::getTime();
		for (int i = 0; i < LedEvent::NUM_LEDS; ++i) {
			ledStatus[i] = false;
			ledTime[i] = now;
		}
		
	}
	SystemFileContext context;
	try {
		iconOn = new IMAGE(screen, context.resolve("skins/led.png"));
	} catch (FileException& e) {
		iconOn = NULL;
	}
	try {
		iconOff = new IMAGE(screen, context.resolve("skins/led-off.png"));
	} catch (FileException& e) {
		iconOff = NULL;
	}
	EventDistributor::instance().registerEventListener(LED_EVENT, *this);
}

template <class IMAGE>
IconLayer<IMAGE>::~IconLayer()
{
	EventDistributor::instance().unregisterEventListener(LED_EVENT, *this);
	delete iconOn;
	delete iconOff;
}

template <class IMAGE>
void IconLayer<IMAGE>::paint()
{
	for (int i = 0; i < LedEvent::NUM_LEDS; ++i) {
		unsigned long long now = Timer::getTime();
		unsigned long long diff = now - ledTime[i];
		byte alpha;
		if (diff > (fadeTime + fadeDuration)) {
			continue;
		} else if (diff < fadeTime) {
			alpha = 255;
		} else {
			alpha = 255 - (255 * (diff - fadeTime) / fadeDuration);
		}
		IMAGE* icon = ledStatus[i] ? iconOn : iconOff;
		if (icon) {
			icon->draw(50 * i, 0, alpha);
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
	LedEvent::Led led = ledEvent.getLed();
	bool status = ledEvent.getStatus();
	if (status != ledStatus[led]) {
		ledStatus[led] = status;
		ledTime[led] = Timer::getTime();
	}
	return true;
}

} // namespace openmsx

// $Id$

#include "IconLayer.hh"
#include "EventDistributor.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "Timer.hh"
#include "SDLImage.hh"
#include "IntegerSetting.hh"
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

	createSettings(LedEvent::POWER, "power");
	createSettings(LedEvent::CAPS,  "caps");
	createSettings(LedEvent::KANA,  "kana");
	createSettings(LedEvent::PAUSE, "pause");
	createSettings(LedEvent::TURBO, "turbo");
	createSettings(LedEvent::FDD,   "fdd");

	fadeTimeSetting.reset(new IntegerSetting("icon.fade-delay",
		"Time (in ms) after which the icons start to fade (0 means no fading)",
		5000, 0, 1000000));
	fadeDurationSetting.reset(new IntegerSetting("icon.fade-duration",
		"Time (in ms) it takes for the icons the fade from completely opaque "
		"to completely transparent", 5000, 0, 1000000));
	
	EventDistributor::instance().registerEventListener(LED_EVENT, *this);
}

template <class IMAGE>
void IconLayer<IMAGE>::createSettings(LedEvent::Led led, const string& name)
{
	string icon_name = "icon." + name;
	ledInfo[led].xcoord.reset(new IntegerSetting(icon_name + ".xcoord",
		"X-coordinate for LED icon", ((int)led) * 50, 0, 640));
	ledInfo[led].ycoord.reset(new IntegerSetting(icon_name + ".ycoord",
		"Y-coordinate for LED icon", 0, 0, 480));
	ledInfo[led].active.reset(new FilenameSetting(icon_name + ".active",
		"Image for active LED icon", "skins/led.png"));
	ledInfo[led].nonActive.reset(new FilenameSetting(icon_name + ".non-active",
		"Image for active LED icon", "skins/led-off.png"));

	ledInfo[led].active->setChecker(this);
	ledInfo[led].nonActive->setChecker(this);
}


template <class IMAGE>
IconLayer<IMAGE>::~IconLayer()
{
	EventDistributor::instance().unregisterEventListener(LED_EVENT, *this);
}

template <class IMAGE>
void IconLayer<IMAGE>::paint()
{
	unsigned long long fadeTime     = 1000 * fadeTimeSetting->getValue();
	unsigned long long fadeDuration = 1000 * fadeDurationSetting->getValue();

	for (int i = 0; i < LedEvent::NUM_LEDS; ++i) {
		unsigned long long now = Timer::getTime();
		unsigned long long diff = now - ledTime[i];
		byte alpha;
		if (fadeTime == 0) {
			// no fading, draw completely opaque
			alpha = 255;
		} else if (diff > (fadeTime + fadeDuration)) {
			// completely faded out, don't even bother to draw
			continue;
		} else if (diff < fadeTime) {
			// no fading yet
			alpha = 255;
		} else {
			// fading out
			alpha = 255 - (255 * (diff - fadeTime) / fadeDuration);
		}
		IMAGE* icon = ledStatus[i] ? ledInfo[i].iconOn.get()
		                           : ledInfo[i].iconOff.get();
		if (icon) {
			int x = ledInfo[i].xcoord->getValue();
			int y = ledInfo[i].ycoord->getValue();
			icon->draw(x, y, alpha);
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

template <class IMAGE>
void IconLayer<IMAGE>::check(SettingImpl<FilenameSetting::Policy>& setting,
                             string& value)
{
	string filename = static_cast<FilenameSetting&>(setting).
		getFileContext().resolve(value);

	for (int i = 0; i < LedEvent::NUM_LEDS; ++i) {
		if (&setting == ledInfo[i].active.get()) {
			ledInfo[i].iconOn.reset(new IMAGE(outputScreen, filename));
			break;
		}
		if (&setting == ledInfo[i].nonActive.get()) {
			ledInfo[i].iconOff.reset(new IMAGE(outputScreen, filename));
			break;
		}
	}
}

} // namespace openmsx

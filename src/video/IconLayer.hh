// $Id$

#ifndef ICONLAYER_HH
#define ICONLAYER_HH

#include "Layer.hh"
#include "EventListener.hh"
#include "LedEvent.hh"
#include "FilenameSetting.hh"
#include <memory>

class SDL_Surface;

namespace openmsx {

class CommandController;
class EventDistributor;
class Display;
class IntegerSetting;

template <class IMAGE>
class IconLayer : public Layer, private EventListener,
                  private SettingChecker<FilenameSetting::Policy>
{
public:
	IconLayer(CommandController& commandController,
	          EventDistributor& eventDistributor,
	          Display& display, SDL_Surface* screen);
	virtual ~IconLayer();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

private:
	void createSettings(CommandController& commandController,
	                    LedEvent::Led led, const std::string& name);

	// EventListener interface:
	virtual void signalEvent(const Event& event);

	// SettingChecker
	virtual void check(SettingImpl<FilenameSetting::Policy>& setting,
	                   std::string& value);

	EventDistributor& eventDistributor;
	Display& display;
	SDL_Surface* outputScreen;
	double scaleFactor;

	struct LedInfo {
		std::auto_ptr<IntegerSetting> xcoord;
		std::auto_ptr<IntegerSetting> ycoord;
		std::auto_ptr<IntegerSetting> fadeTime[2];
		std::auto_ptr<IntegerSetting> fadeDuration[2];
		std::auto_ptr<FilenameSetting> name[2];
		std::auto_ptr<IMAGE> icon[2];
	};
	LedInfo ledInfo[LedEvent::NUM_LEDS];
};

class SDLImage;
class GLImage;
typedef IconLayer<SDLImage> SDLIconLayer;
typedef IconLayer<GLImage>  GLIconLayer;

} // namespace openmsx

#endif

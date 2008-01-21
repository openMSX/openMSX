// $Id$

#ifndef ICONLAYER_HH
#define ICONLAYER_HH

#include "Layer.hh"
#include "LedEvent.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class CommandController;
class OutputSurface;
class Display;
class IconStatus;
class IntegerSetting;
class FilenameSetting;
template <class IMAGE> class IconSettingChecker;

template <class IMAGE>
class IconLayer : public Layer, private noncopyable
{
public:
	IconLayer(CommandController& commandController,
	          Display& display, IconStatus& iconStatus,
	          OutputSurface& output);
	virtual ~IconLayer();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

private:
	void createSettings(CommandController& commandController,
	                    LedEvent::Led led, const std::string& name);

	Display& display;
	IconStatus& iconStatus;
	OutputSurface& output;
	double scaleFactor;
	const std::auto_ptr<IconSettingChecker<IMAGE> > iconSettingChecker;

	struct LedInfo {
		std::auto_ptr<IntegerSetting> xcoord;
		std::auto_ptr<IntegerSetting> ycoord;
		std::auto_ptr<IntegerSetting> fadeTime[2];
		std::auto_ptr<IntegerSetting> fadeDuration[2];
		std::auto_ptr<FilenameSetting> name[2];
		std::auto_ptr<IMAGE> icon[2];
	};
	LedInfo ledInfo[LedEvent::NUM_LEDS];

	friend class IconSettingChecker<IMAGE>;
};

class SDLImage;
class GLImage;
typedef IconLayer<SDLImage> SDLIconLayer;
typedef IconLayer<GLImage>  GLIconLayer;

} // namespace openmsx

#endif

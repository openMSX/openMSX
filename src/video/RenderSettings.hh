// $Id$

#ifndef RENDERSETTINGS_HH
#define RENDERSETTINGS_HH

#include <memory>
#include "RendererFactory.hh"
#include "Scaler.hh"
#include "SettingListener.hh"
#include "EventListener.hh"

namespace openmsx {

class SettingsConfig;
class InfoCommand;
class IntegerSetting;
class FloatSetting;
class BooleanSetting;
class VideoSourceSetting;


/** Singleton containing all settings for renderers.
  * Keeping the settings here makes sure they are preserved when the user
  * switches to another renderer.
  */
class RenderSettings: private SettingListener, private EventListener
{
public:
	/** Render accuracy: granularity of the rendered area.
	  */
	enum Accuracy { ACC_SCREEN, ACC_LINE, ACC_PIXEL };

	/** Video sources: devices which produce a video stream.
	  */
	enum VideoSource { VIDEO_MSX, VIDEO_GFX9000 };

	/** Gets the singleton instance. */
	static RenderSettings& instance();

	/** Accuracy [screen, line, pixel]. */
	EnumSetting<Accuracy>* getAccuracy() const { return accuracy.get(); }

	/** Deinterlacing [on, off]. */
	BooleanSetting* getDeinterlace() const { return deinterlace.get(); }

	/** The current max frameskip. */
	IntegerSetting* getMaxFrameSkip() const { return maxFrameSkip.get(); }

	/** The current min frameskip. */
	IntegerSetting* getMinFrameSkip() const { return minFrameSkip.get(); }

	/** Full screen [on, off]. */
	BooleanSetting* getFullScreen() const { return fullScreen.get(); }

	/** The amount of gamma correction. */
	FloatSetting* getGamma() const { return gamma.get(); }

	/** The amount of glow [0..100]. */
	IntegerSetting* getGlow() const { return glow.get(); }

	/** The amount of horizontal blur [0..100]. */
	IntegerSetting* getHorizontalBlur() const { return horizontalBlur.get(); }

	/** The current renderer. */
	RendererFactory::RendererSetting* getRenderer() const { return renderer.get(); }

	/** The current scaling algorithm. */
	EnumSetting<ScalerID>* getScaler() const { return scaler.get(); }

	/** The alpha value [0..100] of the scanlines. */
	IntegerSetting* getScanlineAlpha() const { return scanlineAlpha.get(); }

	/** The video source to display on the screen. */
	VideoSourceSetting* getVideoSource() const { return videoSource.get(); }

private:
	RenderSettings();
	virtual ~RenderSettings();

	// SettingListener interface:
	virtual void update(const Setting* setting);

	// EventListener interface:
	virtual bool signalEvent(const Event& event);

	void checkRendererSwitch();

	// Please keep the settings ordered alphabetically.
	std::auto_ptr<EnumSetting<Accuracy> > accuracy;
	std::auto_ptr<BooleanSetting> deinterlace;
	std::auto_ptr<BooleanSetting> fullScreen;
	std::auto_ptr<FloatSetting> gamma;
	std::auto_ptr<IntegerSetting> glow;
	std::auto_ptr<IntegerSetting> horizontalBlur;
	std::auto_ptr<IntegerSetting> maxFrameSkip;
	std::auto_ptr<IntegerSetting> minFrameSkip;
	std::auto_ptr<RendererFactory::RendererSetting> renderer;
	std::auto_ptr<EnumSetting<ScalerID> > scaler;
	std::auto_ptr<IntegerSetting> scanlineAlpha;
	std::auto_ptr<VideoSourceSetting> videoSource;

	RendererFactory::RendererID currentRenderer;
	bool videoSources[VIDEO_GFX9000 + 1];
};

} // namespace openmsx

#endif

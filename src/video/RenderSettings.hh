// $Id$

#ifndef RENDERSETTINGS_HH
#define RENDERSETTINGS_HH

#include "RendererFactory.hh"
#include "Scaler.hh"
#include <memory>

namespace openmsx {

class IntegerSetting;
class FloatSetting;
class BooleanSetting;
class VideoSourceSetting;

/** Class containing all settings for renderers.
  * Keeping the settings here makes sure they are preserved when the user
  * switches to another renderer.
  */
class RenderSettings
{
public:
	/** Render accuracy: granularity of the rendered area.
	  */
	enum Accuracy { ACC_SCREEN, ACC_LINE, ACC_PIXEL };

	RenderSettings();
	~RenderSettings();

	/** Accuracy [screen, line, pixel]. */
	EnumSetting<Accuracy>& getAccuracy() const { return *accuracy; }

	/** Deinterlacing [on, off]. */
	BooleanSetting& getDeinterlace() const { return *deinterlace; }

	/** The current max frameskip. */
	IntegerSetting& getMaxFrameSkip() const { return *maxFrameSkip; }

	/** The current min frameskip. */
	IntegerSetting& getMinFrameSkip() const { return *minFrameSkip; }

	/** Full screen [on, off]. */
	BooleanSetting& getFullScreen() const { return *fullScreen; }

	/** The amount of gamma correction. */
	FloatSetting& getGamma() const { return *gamma; }

	/** The amount of glow [0..100]. */
	IntegerSetting& getGlow() const { return *glow; }

	/** The amount of horizontal blur [0..100]. */
	IntegerSetting& getHorizontalBlur() const { return *horizontalBlur; }

	/** The current renderer. */
	RendererFactory::RendererSetting& getRenderer() const { return *renderer; }

	/** The current scaling algorithm. */
	EnumSetting<ScalerID>& getScaler() const { return *scaler; }

	/** The alpha value [0..100] of the scanlines. */
	IntegerSetting& getScanlineAlpha() const { return *scanlineAlpha; }

	/** The video source to display on the screen. */
	VideoSourceSetting& getVideoSource() const { return *videoSource; }

private:
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
};

} // namespace openmsx

#endif

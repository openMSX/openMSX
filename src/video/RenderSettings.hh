// $Id$

#ifndef __RENDERSETTINGS_HH__
#define __RENDERSETTINGS_HH__

#include <memory>
#include "RendererFactory.hh"
#include "Scaler.hh"

using std::auto_ptr;

namespace openmsx {

class SettingsConfig;
class InfoCommand;
class IntegerSetting;
class FloatSetting;
class BooleanSetting;


/** Singleton containing all settings for renderers.
  * Keeping the settings here makes sure they are preserved when the user
  * switches to another renderer.
  */
class RenderSettings
{
public:
	/** Render accuracy: granularity of the rendered area.
	  */
	enum Accuracy { ACC_SCREEN, ACC_LINE, ACC_PIXEL };

	static RenderSettings& instance();

	/** Accuracy [screen, line, pixel] */
	EnumSetting<Accuracy>* getAccuracy() { return accuracy.get(); }

	/** Deinterlacing [on, off]. */
	BooleanSetting* getDeinterlace() { return deinterlace.get(); }

	/** The current max frameskip. */
	IntegerSetting* getMaxFrameSkip() { return maxFrameSkip.get(); }

	/** The current min frameskip. */
	IntegerSetting* getMinFrameSkip() { return minFrameSkip.get(); }

	/** Full screen [on, off]. */
	BooleanSetting* getFullScreen() { return fullScreen.get(); }

	/** The amount of gamma correction. */
	FloatSetting* getGamma() { return gamma.get(); }

	/** The amount of glow [0..100]. */
	IntegerSetting* getGlow() { return glow.get(); }

	/** The amount of horizontal blur [0..100]. */
	IntegerSetting* getHorizontalBlur() { return horizontalBlur.get(); }

	/** The current renderer. */
	RendererFactory::RendererSetting* getRenderer() { return renderer.get(); }

	/** The current scaling algorithm. */
	EnumSetting<ScalerID>* getScaler() { return scaler.get(); }

	/** The alpha value [0..100] of the scanlines. */
	IntegerSetting* getScanlineAlpha() { return scanlineAlpha.get(); }

private:
	RenderSettings();
	~RenderSettings();

	// Please keep the settings ordered alphabetically.
	auto_ptr<EnumSetting<Accuracy> > accuracy;
	auto_ptr<BooleanSetting> deinterlace;
	auto_ptr<IntegerSetting> maxFrameSkip;
	auto_ptr<IntegerSetting> minFrameSkip;
	auto_ptr<BooleanSetting> fullScreen;
	auto_ptr<FloatSetting> gamma;
	auto_ptr<IntegerSetting> glow;
	auto_ptr<IntegerSetting> horizontalBlur;
	auto_ptr<RendererFactory::RendererSetting> renderer;
	auto_ptr<EnumSetting<ScalerID> > scaler;
	auto_ptr<IntegerSetting> scanlineAlpha;
};

} // namespace openmsx

#endif // __RENDERSETTINGS_HH__


// $Id$

#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "VideoSourceSetting.hh"
#include <cmath>

namespace openmsx {

RenderSettings::RenderSettings(CommandController& commandController)
{
	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracy.reset(new EnumSetting<Accuracy>(commandController,
		"accuracy", "rendering accuracy", ACC_PIXEL, accMap));

	deinterlace.reset(new BooleanSetting(commandController,
		"deinterlace", "deinterlacing on/off", true));

	maxFrameSkip.reset(new IntegerSetting(commandController,
		"maxframeskip", "set the max amount of frameskip", 3, 0, 100));

	minFrameSkip.reset(new IntegerSetting(commandController,
		"minframeskip", "set the min amount of frameskip", 0, 0, 100));

	fullScreen.reset(new BooleanSetting(commandController,
		"fullscreen", "full screen display on/off", false));

	gamma.reset(new FloatSetting(commandController, "gamma",
		"amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0));

	brightness.reset(new FloatSetting(commandController, "brightness",
		"brightness video setting: "
		"0 is normal, lower is darker, higher is brighter",
		0.0, -100.0, 100.0));

	contrast.reset(new FloatSetting(commandController, "contrast",
		"contrast video setting: "
		"0 is normal, lower is less contrast, higher is more contrast",
		0.0, -100.0, 100.0));

	glow.reset(new IntegerSetting(commandController,
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100));

	noise.reset(new FloatSetting(commandController,
		"noise", "amount of noise to add to the frame",
		0.0, 0.0, 100.0));

	horizontalBlur.reset(new IntegerSetting(commandController,
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100));

	videoSource.reset(new VideoSourceSetting(commandController));

	// Get user-preferred renderer from config.
	renderer = RendererFactory::createRendererSetting(commandController);

	EnumSetting<ScaleAlgorithm>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["SaI"] = SCALER_SAI;
	scalerMap["ScaleNx"] = SCALER_SCALE;
	scalerMap["hq"] = SCALER_HQ;
	scalerMap["hqlite"] = SCALER_HQLITE;
	scalerMap["RGBtriplet"] = SCALER_RGBTRIPLET;
	scaleAlgorithm.reset(new EnumSetting<ScaleAlgorithm>(commandController,
		"scale_algorithm", "scale algorithm",
		SCALER_SIMPLE, scalerMap));

	scaleFactor.reset(new IntegerSetting(commandController,
		"scale_factor", "scale factor", 2, 1, 4));

	scanlineAlpha.reset(new IntegerSetting(commandController,
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100));

	limitSprites.reset(new BooleanSetting(commandController,
		"limitsprites", "limit number of sprites per line "
		"(on for realism, off to reduce sprite flashing)", true));

	EnumSetting<bool>::Map cmdMap;
	cmdMap["real"]   = false;
	cmdMap["broken"] = true;
	cmdTiming.reset(new EnumSetting<bool>(commandController,
		"cmdtiming", "VDP command timing", false, cmdMap,
		Setting::DONT_SAVE));
}

RenderSettings::~RenderSettings()
{
}

int RenderSettings::getBlurFactor() const
{
	return (horizontalBlur->getValue()) * 256 / 100;
}

int RenderSettings::getScanlineFactor() const
{
	return 255 - ((scanlineAlpha->getValue() * 255) / 100);
}

static double conv(double x, double brightness, double contrast, double gamma)
{
	double y = (x + brightness - 0.5) * contrast + 0.5;
	if (y <= 0.0) return 0.0;
	if (y >= 1.0) return 1.0;
	return ::pow(y, gamma);
}

void RenderSettings::transformRGB(double& r, double& g, double& b)
{
	double brightness = getBrightness().getValue() / 100.0;
	double contrast = getContrast().getValue();
	contrast = (contrast >= 0.0) ? (1 + contrast / 25.0)
	                             : (1 + contrast / 125.0);
	double gamma = 1.0 / getGamma().getValue();
	r = conv(r, brightness, contrast, gamma);
	g = conv(g, brightness, contrast, gamma);
	b = conv(b, brightness, contrast, gamma);
}

} // namespace openmsx

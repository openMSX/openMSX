// $Id$

#include "RenderSettings.hh"
#include "SettingsConfig.hh"
#include "CliCommOutput.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"

namespace openmsx {

RenderSettings::RenderSettings()
{
	const XMLElement* config =
		SettingsConfig::instance().findChild("renderer");

	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracy.reset(new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", ACC_PIXEL, accMap));

	deinterlace.reset(new BooleanSetting(
		"deinterlace", "deinterlacing on/off", true));

	maxFrameSkip.reset(new IntegerSetting(
		"maxframeskip", "set the max amount of frameskip",
		3, 0, 100));
	minFrameSkip.reset(new IntegerSetting(
		"minframeskip", "set the min amount of frameskip",
		0, 0, 100));

	bool fsBool = false;
	if (config) {
		fsBool = config->getChildDataAsBool("full_screen", fsBool);
	}
	fullScreen.reset(new BooleanSetting(
		"fullscreen", "full screen display on/off", fsBool));

	gamma.reset(new FloatSetting(
		"gamma", "amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0));

	glow.reset(new IntegerSetting(
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100));

	horizontalBlur.reset(new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100));

	// Get user-preferred renderer from config.
	string rendererName = "SDLHi";
	if (config) {
		rendererName = config->getChildData("type", rendererName);
	}
	renderer = RendererFactory::createRendererSetting(rendererName);

	EnumSetting<ScalerID>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xSaI"] = SCALER_SAI2X;
	scalerMap["Scale2x"] = SCALER_SCALE2X;
	scalerMap["hq2x"] = SCALER_HQ2X;
	scaler.reset(new EnumSetting<ScalerID>(
		"scaler", "scaler algorithm", SCALER_SIMPLE, scalerMap));

	scanlineAlpha.reset(new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100));
}

RenderSettings::~RenderSettings()
{
}

RenderSettings& RenderSettings::instance()
{
	static RenderSettings oneInstance;
	return oneInstance;
}

} // namespace openmsx

// $Id$

#include "RenderSettings.hh"
#include "SettingsConfig.hh"
#include "Config.hh"
#include "CliCommOutput.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"

namespace openmsx {

RenderSettings::RenderSettings()
	: settingsConfig(SettingsConfig::instance())
{
	Config* config = settingsConfig.getConfigById("renderer");

	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracy = new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", ACC_LINE, accMap);

	deinterlace = new BooleanSetting(
		"deinterlace", "deinterlacing on/off", true);

	maxFrameSkip = new IntegerSetting(
		"maxframeskip", "set the max amount of frameskip",
		3, 0, 100);
	minFrameSkip = new IntegerSetting(
		"minframeskip", "set the min amount of frameskip",
		0, 0, 100);

	bool fsBool = config->getParameterAsBool("full_screen", false);
	fullScreen = new BooleanSetting(
		"fullscreen", "full screen display on/off", fsBool);

	gamma = new FloatSetting(
		"gamma", "amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0);

	glow = new IntegerSetting(
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100);

	horizontalBlur = new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100);

	// Get user-preferred renderer from config.
	string rendererName = config->getType();
	renderer = RendererFactory::createRendererSetting(rendererName);

	EnumSetting<ScalerID>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xSaI"] = SCALER_SAI2X;
	scalerMap["Scale2x"] = SCALER_SCALE2X;
	scalerMap["hq2x"] = SCALER_HQ2X;
	scaler = new EnumSetting<ScalerID>(
		"scaler", "scaler algorithm", SCALER_SIMPLE, scalerMap);

	scanlineAlpha = new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100);
}

RenderSettings::~RenderSettings()
{
	delete scanlineAlpha;
	delete scaler;
	delete horizontalBlur;
	delete glow;
	delete gamma;
	delete fullScreen;
	delete minFrameSkip;
	delete maxFrameSkip;
	delete deinterlace;
	delete accuracy;
}

RenderSettings& RenderSettings::instance()
{
	static RenderSettings oneInstance;
	return oneInstance;
}

} // namespace openmsx

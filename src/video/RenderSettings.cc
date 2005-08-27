// $Id$

#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "VideoSourceSetting.hh"

namespace openmsx {

RenderSettings::RenderSettings()
{
	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracy.reset(new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", ACC_PIXEL, accMap));

	deinterlace.reset(new BooleanSetting(
		"deinterlace", "deinterlacing on/off", true));

	maxFrameSkip.reset(new IntegerSetting(
		"maxframeskip", "set the max amount of frameskip", 3, 0, 100));

	minFrameSkip.reset(new IntegerSetting(
		"minframeskip", "set the min amount of frameskip", 0, 0, 100));

	fullScreen.reset(new BooleanSetting(
		"fullscreen", "full screen display on/off", false));

	gamma.reset(new FloatSetting(
		"gamma", "amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0));

	glow.reset(new IntegerSetting(
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100));

	horizontalBlur.reset(new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100));

	videoSource.reset(new VideoSourceSetting());

	// Get user-preferred renderer from config.
	renderer = RendererFactory::createRendererSetting();

	EnumSetting<ScalerID>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xSaI"] = SCALER_SAI2X;
	scalerMap["Scale2x"] = SCALER_SCALE2X;
	scalerMap["hq2x"] = SCALER_HQ2X;
	scalerMap["hq2xlite"] = SCALER_HQ2XLITE;
	scalerMap["low"] = SCALER_LOW;
	scaler.reset(new EnumSetting<ScalerID>(
		"scaler", "scaler algorithm", SCALER_SIMPLE, scalerMap));

	scanlineAlpha.reset(new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100));
}

RenderSettings::~RenderSettings()
{
}

} // namespace openmsx

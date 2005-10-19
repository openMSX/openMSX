// $Id$

#include "RenderSettings.hh"
#include "IntegerSetting.hh"
#include "FloatSetting.hh"
#include "BooleanSetting.hh"
#include "VideoSourceSetting.hh"

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

	gamma.reset(new FloatSetting(commandController,
		"gamma", "amount of gamma correction: low is dark, high is bright",
		1.1, 0.1, 5.0));

	glow.reset(new IntegerSetting(commandController,
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		0, 0, 100));

	horizontalBlur.reset(new IntegerSetting(commandController,
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100));

	videoSource.reset(new VideoSourceSetting(commandController));

	// Get user-preferred renderer from config.
	renderer = RendererFactory::createRendererSetting(commandController);

	EnumSetting<ScalerID>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xSaI"] = SCALER_SAI2X;
	scalerMap["Scale2x"] = SCALER_SCALE2X;
	scalerMap["hq2x"] = SCALER_HQ2X;
	scalerMap["hq3x"] = SCALER_HQ3X;
	scalerMap["hq2xlite"] = SCALER_HQ2XLITE;
	scalerMap["hq3xlite"] = SCALER_HQ3XLITE;
	scalerMap["low"] = SCALER_LOW;
	scaler.reset(new EnumSetting<ScalerID>(commandController,
		"scaler", "scaler algorithm", SCALER_SIMPLE, scalerMap));

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

} // namespace openmsx

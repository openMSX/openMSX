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
	XMLElement& config =
		SettingsConfig::instance().getCreateChild("renderer");

	EnumSetting<Accuracy>::Map accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	XMLElement& accElem = config.getCreateChild("accuracy", "pixel");
	Accuracy accValue = ACC_PIXEL;
	EnumSetting<Accuracy>::Map::const_iterator accIt =
		accMap.find(accElem.getData());
	if (accIt != accMap.end()) {
		accValue = accIt->second;
	}
	accuracy.reset(new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", accValue, accMap, &accElem));

	XMLElement& deintElem = config.getCreateChild("deinterlace", "on");
	deinterlace.reset(new BooleanSetting(
		"deinterlace", "deinterlacing on/off",
		deintElem.getDataAsBool(), &deintElem));

	XMLElement& maxSkipElem = config.getCreateChild("maxframeskip", "3");
	maxFrameSkip.reset(new IntegerSetting(
		"maxframeskip", "set the max amount of frameskip",
		maxSkipElem.getDataAsInt(), 0, 100, &maxSkipElem));

	XMLElement& minSkipElem = config.getCreateChild("minframeskip", "0");
	minFrameSkip.reset(new IntegerSetting(
		"minframeskip", "set the min amount of frameskip",
		minSkipElem.getDataAsInt(), 0, 100, &minSkipElem));

	XMLElement& fsElem = config.getCreateChild("full_screen", false);
	fullScreen.reset(new BooleanSetting("fullscreen",
		"full screen display on/off", fsElem.getDataAsBool(), &fsElem));

	XMLElement& gammaElem = config.getCreateChild("gamma", "1.1");
	gamma.reset(new FloatSetting(
		"gamma", "amount of gamma correction: low is dark, high is bright",
		gammaElem.getDataAsDouble(), 0.1, 5.0, &gammaElem));

	XMLElement& glowElem = config.getCreateChild("glow", "0");
	glow.reset(new IntegerSetting(
		"glow", "amount of afterglow effect: 0 = none, 100 = lots",
		glowElem.getDataAsInt(), 0, 100, &glowElem));

	XMLElement& blurElem = config.getCreateChild("blur", "50");
	horizontalBlur.reset(new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		blurElem.getDataAsInt(), 0, 100, &blurElem));

	// Get user-preferred renderer from config.
	XMLElement& rendererElem = config.getCreateChild("type", "SDLHi");
	renderer = RendererFactory::createRendererSetting(rendererElem);

	EnumSetting<ScalerID>::Map scalerMap;
	scalerMap["simple"] = SCALER_SIMPLE;
	scalerMap["2xSaI"] = SCALER_SAI2X;
	scalerMap["Scale2x"] = SCALER_SCALE2X;
	scalerMap["hq2x"] = SCALER_HQ2X;
	scalerMap["blur"] = SCALER_BLUR;
	XMLElement& scalerElem = config.getCreateChild("scaler", "simple");
	ScalerID scalerValue = SCALER_SIMPLE;
	EnumSetting<ScalerID>::Map::const_iterator scalerIt =
		scalerMap.find(scalerElem.getData());
	if (scalerIt != scalerMap.end()) {
		scalerValue = scalerIt->second;
	}
	scaler.reset(new EnumSetting<ScalerID>(
		"scaler", "scaler algorithm", scalerValue, scalerMap, &scalerElem));

	XMLElement& scanElem = config.getCreateChild("scanline", "20");
	scanlineAlpha.reset(new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		scanElem.getDataAsInt(), 0, 100, &scanElem));
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

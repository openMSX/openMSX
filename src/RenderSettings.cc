// $Id$

#include "RenderSettings.hh"
#include "MSXConfig.hh"


RenderSettings::RenderSettings()
{
	Config *config = MSXConfig::instance()->getConfigById("renderer");
	
	std::map<const std::string, Accuracy> accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracy = new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", ACC_LINE, accMap);
	
	deinterlace = new BooleanSetting(
		"deinterlace", "deinterlacing on/off", true);
	
	bool fsBool = false;
	if (config->hasParameter("full_screen")) {
		fsBool = config->getParameterAsBool("full_screen");
	}
	fullScreen = new BooleanSetting(
		"fullscreen", "full screen display on/off", fsBool);
	
	horizontalBlur = new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100);
	
	scanlineAlpha = new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100);
}

RenderSettings::~RenderSettings()
{
	delete accuracy;
	delete deinterlace;
	delete horizontalBlur;
	delete scanlineAlpha;
}

// $Id$

#include "RenderSettings.hh"


RenderSettings::RenderSettings()
{
	scanlineAlpha = new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100);
	
	horizontalBlur = new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100);
	
	deinterlace = new BooleanSetting(
		"deinterlace", "deinterlacing on/off", true);
	
	std::map<const std::string, Accuracy> accMap;
	accMap["screen"] = ACC_SCREEN;
	accMap["line"]   = ACC_LINE;
	accMap["pixel"]  = ACC_PIXEL;
	accuracy = new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", ACC_LINE, accMap);
	
	std::map<const std::string, bool> cmdMap;
	cmdMap["real"]   = false;
	cmdMap["broken"] = true;
	cmdTiming = new EnumSetting<bool>(
		"cmdtiming", "VDP command timing", false, cmdMap);
}

RenderSettings::~RenderSettings()
{
	delete scanlineAlpha;
	delete horizontalBlur;
	delete deinterlace;
	delete accuracy;
	delete cmdTiming;
}

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
	
	std::map<const std::string, Accuracy> map;
	map["screen"] = ACC_SCREEN;
	map["line"]   = ACC_LINE;
	map["pixel"]  = ACC_PIXEL;
	accuracy = new EnumSetting<Accuracy>(
		"accuracy", "rendering accuracy", ACC_LINE, map);
}

RenderSettings::~RenderSettings()
{
	delete scanlineAlpha;
	delete horizontalBlur;
	delete deinterlace;
	delete accuracy;
}

// $Id$

#include "RenderSettings.hh"

RenderSettings::RenderSettings()
{
	scanlineAlpha = new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		20, 0, 100 );
	horizontalBlur = new IntegerSetting(
		"blur", "amount of horizontal blur effect: 0 = none, 100 = full",
		50, 0, 100 );
}


// $Id$

#include "RenderSettings.hh"

RenderSettings::RenderSettings()
{
	scanlineAlpha = new IntegerSetting(
		"scanline", "amount of scanline effect: 0 = none, 100 = full",
		25, 0, 100 );
}


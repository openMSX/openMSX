// $Id$

#ifndef __RENDERSETTINGS_HH__
#define __RENDERSETTINGS_HH__

#include "Settings.hh"

/** Singleton containing all settings for renderers.
  * Keeping the settings here makes sure they are preserved when the user
  * switches to another renderer.
  */
class RenderSettings
{
private:
	RenderSettings();

	IntegerSetting *scanlineAlpha;

public:
	/** Get singleton instance.
	  */
	static RenderSettings *getInstance() {
		static RenderSettings *instance = NULL;
		if (!instance) instance = new RenderSettings();
		return instance;
	}

	/** The alpha value [0..100] of the scanlines. */
	IntegerSetting *getScanlineAlpha() { return scanlineAlpha; }

};

#endif // __RENDERSETTINGS_HH__


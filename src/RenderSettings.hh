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
	IntegerSetting *horizontalBlur;

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

	/** The amount of horizontal blur [0..100]. */
	IntegerSetting *getHorizontalBlur() { return horizontalBlur; }

};

#endif // __RENDERSETTINGS_HH__


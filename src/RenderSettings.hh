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
public:
	/** Render accuracy: granularity of the rendered area.
	  */
	enum Accuracy { ACC_SCREEN, ACC_LINE, ACC_PIXEL };

private:
	RenderSettings();
	~RenderSettings();

	IntegerSetting *scanlineAlpha;
	IntegerSetting *horizontalBlur;
	BooleanSetting *deinterlace;
	EnumSetting<Accuracy> *accuracy;

public:
	/** Get singleton instance.
	  */
	static RenderSettings *instance() {
		static RenderSettings oneInstance;
		return &oneInstance;
	}

	/** The alpha value [0..100] of the scanlines. */
	IntegerSetting *getScanlineAlpha() { return scanlineAlpha; }

	/** The amount of horizontal blur [0..100]. */
	IntegerSetting *getHorizontalBlur() { return horizontalBlur; }

	/** Deinterlacing [on, off]. */
	BooleanSetting *getDeinterlace() { return deinterlace; }

	/** Accuracy [screen, line, pixel] */
	EnumSetting<Accuracy> *getAccuracy() { return accuracy; }

};

#endif // __RENDERSETTINGS_HH__


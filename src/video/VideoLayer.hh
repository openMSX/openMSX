// $Id$

#ifndef __VIDEOLAYER_HH__
#define __VIDEOLAYER_HH__

#include "Display.hh"
#include "SettingListener.hh"
#include "RenderSettings.hh"

namespace openmsx {

template <class T> class EnumSetting;
class BooleanSetting;

class VideoLayer: public Layer, protected SettingListener
{
public:
	/** Destructor.
	  */
	virtual ~VideoLayer();

protected:
	/** Constructor.
	  */
	VideoLayer(RenderSettings::VideoSource videoSource);

	// SettingListener interface:
	virtual void update(const Setting* setting);

private:
	/** Calculates the current Z coordinate of this layer. */
	ZIndex calcZ();
	/** Calculates the current coverage of this layer. */
	Coverage getCoverage();

	/** Video source that displays on this layer. */
	RenderSettings::VideoSource videoSource;
	/** Reference to "videosource" setting. */
	EnumSetting<RenderSettings::VideoSource>& videoSourceSetting;
	/** Reference to "power" setting. */
	BooleanSetting& powerSetting;

};

} // namespace openmsx

#endif //__VIDEOLAYER_HH__

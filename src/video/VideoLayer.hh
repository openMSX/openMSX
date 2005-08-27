// $Id$

#ifndef VIDEOLAYER_HH
#define VIDEOLAYER_HH

#include "Layer.hh"
#include "SettingListener.hh"
#include "VideoSourceSetting.hh"

namespace openmsx {

class RenderSettings;
class Display;
template <class T> class EnumSetting;
class BooleanSetting;

class VideoLayer: public Layer, protected SettingListener
{
public:
	virtual ~VideoLayer();
	VideoSource getVideoSource() const;

protected:
	VideoLayer(VideoSource videoSource, RenderSettings& renderSettings,
	           Display& display);

	// SettingListener interface:
	virtual void update(const Setting* setting);

private:
	/** Calculates the current Z coordinate of this layer. */
	ZIndex calcZ();
	/** Calculates the current coverage of this layer. */
	Coverage getCoverage();

	/** Video source that displays on this layer. */
	VideoSource videoSource;
	/** Settings shared between all renderers. */
	RenderSettings& renderSettings;
	/** Reference to "videosource" setting. */
	VideoSourceSetting& videoSourceSetting;
	/** Activate the videosource */
	VideoSourceActivator videoSourceActivator;
	/** Reference to "power" setting. */
	BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif

// $Id$

#ifndef VIDEOLAYER_HH
#define VIDEOLAYER_HH

#include "Layer.hh"
#include "Observer.hh"
#include "VideoSourceSetting.hh"
#include "noncopyable.hh"

namespace openmsx {

class CommandController;
class RenderSettings;
class Display;
class BooleanSetting;

class VideoLayer: public Layer, protected Observer<Setting>, private noncopyable
{
public:
	virtual ~VideoLayer();
	VideoSource getVideoSource() const;

protected:
	VideoLayer(VideoSource videoSource,
	           CommandController& commandController,
	           Display& display);

	// Observer<Setting> interface:
	virtual void update(const Setting& setting);

private:
	/** Calculates the current Z coordinate of this layer. */
	ZIndex calcZ();
	/** Calculates the current coverage of this layer. */
	Coverage getCoverage();

	/** Video source that displays on this layer. */
	VideoSource videoSource;
	/** Settings shared between all renderers. */
	Display& display;
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

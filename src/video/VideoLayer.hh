// $Id$

#ifndef VIDEOLAYER_HH
#define VIDEOLAYER_HH

#include "Layer.hh"
#include "VideoSource.hh"
#include "Observer.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class CommandController;
class RenderSettings;
class Display;
class Setting;
class BooleanSetting;
class VideoSourceSetting;
class VideoSourceActivator;

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

	/** Settings shared between all renderers. */
	Display& display;
	RenderSettings& renderSettings;
	/** Reference to "videosource" setting. */
	VideoSourceSetting& videoSourceSetting;
	/** Activate the videosource */
	const std::auto_ptr<VideoSourceActivator> videoSourceActivator;
	/** Reference to "power" setting. */
	BooleanSetting& powerSetting;
	/** Video source that displays on this layer. */
	VideoSource videoSource;
};

} // namespace openmsx

#endif

// $Id$

#ifndef VIDEOLAYER_HH
#define VIDEOLAYER_HH

#include "Layer.hh"
#include "VideoSource.hh"
#include "Observer.hh"
#include "MSXEventListener.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class MSXMotherBoard;
class RenderSettings;
class Display;
class Setting;
class BooleanSetting;
class VideoSourceSetting;
class VideoSourceActivator;

class VideoLayer: public Layer, protected Observer<Setting>,
                  private MSXEventListener, private noncopyable
{
public:
	virtual ~VideoLayer();
	VideoSource getVideoSource() const;
	void setTransparency(bool enabled);
	bool getTransparency() const { return transparency; }

protected:
	VideoLayer(MSXMotherBoard& motherBoard,
	           VideoSource videoSource,
	           Display& display);

	// Observer<Setting> interface:
	virtual void update(const Setting& setting);

private:
	/** Calculates the current Z coordinate of this layer. */
	void calcZ();
	/** Calculates the current coverage of this layer. */
	void calcCoverage();

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         EmuTime::param time);

	/** This layer belongs to a specific machine. */
	MSXMotherBoard& motherBoard;
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
	/** Does this layer have transparent parts? */
	bool transparency;
};

} // namespace openmsx

#endif

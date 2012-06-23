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

	/** Create a raw (=non-postprocessed) screenshot. The 'height'
	 * parameter should be either '240' or '480'. The current image will be
	 * scaled to '320x240' or '640x480' and written to a png file. */
	virtual void takeRawScreenShot(
		unsigned height, const std::string& filename) = 0;

	// We used to test whether a Layer is active by looking at the
	// Z-coordinate (Z_MSX_ACTIVE vs Z_MSX_PASSIVE). Though in case of
	// Video9000 it's possible the Video9000 layer is selected, but we
	// still need to render this layer (the v99x8 or v9990 layer).
	void setVideo9000Active(bool active) { activeVideo9000 = active; }
	bool isActive() const;

protected:
	VideoLayer(MSXMotherBoard& motherBoard,
	           VideoSource videoSource);

	// Observer<Setting> interface:
	virtual void update(const Setting& setting);

private:
	/** Calculates the current Z coordinate of this layer. */
	void calcZ();
	/** Calculates the current coverage of this layer. */
	void calcCoverage();

	// MSXEventListener
	virtual void signalEvent(const shared_ptr<const Event>& event,
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
	const VideoSource videoSource;
	/** Active when Video9000 is shown. */
	bool activeVideo9000;
};

} // namespace openmsx

#endif

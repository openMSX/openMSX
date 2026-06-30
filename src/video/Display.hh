#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "OutputDimensions.hh"
#include "RenderSettings.hh"

#include "Command.hh"
#include "EventListener.hh"
#include "InfoTopic.hh"
#include "OSDGUI.hh"
#include "RTSchedulable.hh"

#include "CircularBuffer.hh"
#include "Observer.hh"

#include <cstdint>
#include <memory>
#include <vector>

namespace openmsx {

class CliComm;
class GLSnow;
class OSDGUILayer;
class Reactor;
class Setting;
class VideoLayer;
class VideoSystem;
class VideoSystemChangeListener;

/** Represents the output window/screen of openMSX.
  * A display contains several layers.
  */
class Display final : public EventListener, private Observer<Setting>
                    , private RTSchedulable
{
public:
	static constexpr std::string_view SCREENSHOT_DIR = "screenshots";
	static constexpr std::string_view SCREENSHOT_EXTENSION = ".png";

public:
	explicit Display(Reactor& reactor);
	~Display();

	void createVideoSystem();
	[[nodiscard]] VideoSystem& getVideoSystem();

	[[nodiscard]] CliComm& getCliComm() const;
	[[nodiscard]] RenderSettings& getRenderSettings() { return renderSettings; }
	[[nodiscard]] auto getRenderer() const { return currentRenderer; }
	[[nodiscard]] OSDGUI& getOSDGUI() { return osdGui; }
	[[nodiscard]] OutputDimensions getLastOutputDim() const { return outputDim; }
	[[nodiscard]] gl::ivec2 getViewSize() const { return outputDim.getViewSize(); }

	/** Redraw the display.
	  * The repaintImpl() methods are for internal and VideoSystem/VisibleSurface use only.
	  */
	void repaint();
	void repaintDelayed(uint64_t delta);
	void updateOutputDimensions(gl::ivec2 windowSize);
	void paintLayers(bool withOsd);

	void setSnowLayer(GLSnow* snow) { snowLayer = snow; }
	void setOSDLayer(OSDGUILayer* osd) { osdLayer = osd; }
	void addVideoLayer(VideoLayer& layer);
	void removeVideoLayer(VideoLayer& layer);

	void attach(VideoSystemChangeListener& listener);
	void detach(VideoSystemChangeListener& listener);

	[[nodiscard]] VideoLayer* findActiveLayer();
	[[nodiscard]] const auto& getAllLayers() const { return videoLayers; }

	[[nodiscard]] std::string getWindowTitle();

	/** Get/set x,y coordinates of top-left window corner.
	    Either the actual, or the last known coordinates. */
	[[nodiscard]] gl::ivec2 getWindowPosition();
	void setWindowPosition(gl::ivec2 pos);
	// should only be called from VisibleSurface
	void storeWindowPosition(gl::ivec2 pos);
	[[nodiscard]] gl::ivec2 retrieveWindowPosition();

	[[nodiscard]] gl::ivec2 getScaleFactorSize() const;

	// Get the latest fps value
	[[nodiscard]] float getFps() const;

private:
	void resetVideoSystem();

	// EventListener interface
	bool signalEvent(const Event& event) override;

	// RTSchedulable
	void executeRT() override;

	// Observer<Setting> interface
	void update(const Setting& setting) noexcept override;

	void checkRendererSwitch();
	void doRendererSwitch(RenderSettings::RendererID newRenderer);
	void doRendererSwitch2(RenderSettings::RendererID newRenderer);

private:
	GLSnow* snowLayer = nullptr;
	OSDGUILayer* osdLayer = nullptr;
	std::vector<VideoLayer*> videoLayers;
	std::unique_ptr<VideoSystem> videoSystem;

	std::vector<VideoSystemChangeListener*> listeners; // unordered

	// fps related data
	static constexpr unsigned NUM_FRAME_DURATIONS = 50;
	CircularBuffer<uint64_t, NUM_FRAME_DURATIONS> frameDurations;
	uint64_t frameDurationSum;
	uint64_t prevTimeStamp;

	struct ScreenShotCmd final : Command {
		explicit ScreenShotCmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens, TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} screenShotCmd;

	struct FpsInfoTopic final : InfoTopic {
		explicit FpsInfoTopic(InfoCommand& openMSXInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} fpsInfo;

	OSDGUI osdGui;

	Reactor& reactor;
	RenderSettings renderSettings;

	// the current renderer
	RenderSettings::RendererID currentRenderer = RenderSettings::RendererID::UNINITIALIZED;

	OutputDimensions outputDim;

	bool renderFrozen = false;
	bool switchInProgress = false;
};

} // namespace openmsx

#endif

#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "RenderSettings.hh"
#include "Command.hh"
#include "InfoTopic.hh"
#include "OSDGUI.hh"
#include "EventListener.hh"
#include "RTSchedulable.hh"
#include "Observer.hh"
#include "CircularBuffer.hh"
#include <memory>
#include <vector>
#include <cstdint>

namespace openmsx {

class Layer;
class Reactor;
class VideoSystem;
class CliComm;
class VideoSystemChangeListener;
class Setting;
class OutputSurface;

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
	using Layers = std::vector<Layer*>;

	explicit Display(Reactor& reactor);
	~Display();

	void createVideoSystem();
	[[nodiscard]] VideoSystem& getVideoSystem();

	[[nodiscard]] CliComm& getCliComm() const;
	[[nodiscard]] RenderSettings& getRenderSettings() { return renderSettings; }
	[[nodiscard]] auto getRenderer() const { return currentRenderer; }
	[[nodiscard]] OSDGUI& getOSDGUI() { return osdGui; }

	/** Redraw the display.
	  * The repaintImpl() methods are for internal and VideoSystem/VisibleSurface use only.
	  */
	void repaint();
	void repaintDelayed(uint64_t delta);
	void repaintImpl();
	void repaintImpl(OutputSurface& surface);

	void addLayer(Layer& layer);
	void removeLayer(Layer& layer);
	void updateZ(Layer& layer);

	void attach(VideoSystemChangeListener& listener);
	void detach(VideoSystemChangeListener& listener);

	bool isFullScreen() const { return fullscreen; };
	void setFullScreen(bool flag);

	int getDisplayWidth();
	int getDisplayHeight();

	[[nodiscard]] Layer* findActiveLayer() const;
	[[nodiscard]] const Layers& getAllLayers() const { return layers; }

	[[nodiscard]] OutputSurface* getOutputSurface();

	[[nodiscard]] std::string getWindowTitle();

	/** Get/set x,y coordinates of top-left window corner.
	    Either the actual, or the last known coordinates. */
	[[nodiscard]] gl::ivec2 getWindowPosition();
	void setWindowPosition(gl::ivec2 pos);
	// should only be called from VisibleSurface
	void storeWindowPosition(gl::ivec2 pos);
	[[nodiscard]] gl::ivec2 retrieveWindowPosition();

	[[nodiscard]] gl::ivec2 getWindowSize() const;

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

	/** Find front most opaque layer.
	  */
	[[nodiscard]] Layers::iterator baseLayer();

private:
	Layers layers; // sorted on z
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

	bool renderFrozen = false;
	bool switchInProgress = false;

	bool fullscreen = false;
};

} // namespace openmsx

#endif

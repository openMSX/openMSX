#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "RenderSettings.hh"
#include "Command.hh"
#include "CommandConsole.hh"
#include "InfoTopic.hh"
#include "OSDGUI.hh"
#include "EventListener.hh"
#include "LayerListener.hh"
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
                    , private LayerListener, private RTSchedulable
{
public:
	using Layers = std::vector<Layer*>;

	explicit Display(Reactor& reactor);
	~Display();

	void createVideoSystem();
	[[nodiscard]] VideoSystem& getVideoSystem();

	[[nodiscard]] CliComm& getCliComm() const;
	[[nodiscard]] RenderSettings& getRenderSettings() { return renderSettings; }
	[[nodiscard]] OSDGUI& getOSDGUI() { return osdGui; }
	[[nodiscard]] CommandConsole& getCommandConsole() { return commandConsole; }

	/** Redraw the display.
	  * The repaintImpl() methods are for internal and VideoSystem/VisibleSurface use only.
	  */
	void repaint();
	void repaintDelayed(uint64_t delta);
	void repaintImpl();
	void repaintImpl(OutputSurface& surface);

	void addLayer(Layer& layer);
	void removeLayer(Layer& layer);

	void attach(VideoSystemChangeListener& listener);
	void detach(VideoSystemChangeListener& listener);

	[[nodiscard]] Layer* findActiveLayer() const;
	[[nodiscard]] const Layers& getAllLayers() const { return layers; }

	[[nodiscard]] OutputSurface* getOutputSurface();

	[[nodiscard]] std::string getWindowTitle();

private:
	void resetVideoSystem();

	// EventListener interface
	int signalEvent(const Event& event) override;

	// RTSchedulable
	void executeRT() override;

	// Observer<Setting> interface
	void update(const Setting& setting) noexcept override;

	void checkRendererSwitch();
	void doRendererSwitch();
	void doRendererSwitch2();

	/** Find front most opaque layer.
	  */
	[[nodiscard]] Layers::iterator baseLayer();

	// LayerListener interface
	void updateZ(Layer& layer) noexcept override;

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
		void tabCompletion(std::vector<std::string>& tokens) const override;
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
	CommandConsole commandConsole;

	// the current renderer
	RenderSettings::RendererID currentRenderer = RenderSettings::UNINITIALIZED;

	bool renderFrozen;
	bool switchInProgress = false;
};

} // namespace openmsx

#endif

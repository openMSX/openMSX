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
	VideoSystem& getVideoSystem();

	CliComm& getCliComm() const;
	RenderSettings& getRenderSettings() { return renderSettings; }
	OSDGUI& getOSDGUI() { return osdGui; }
	CommandConsole& getCommandConsole() { return commandConsole; }

	/** Redraw the display.
	  */
	void repaint();
	void repaint(OutputSurface& surface);
	void repaintDelayed(uint64_t delta);

	void addLayer(Layer& layer);
	void removeLayer(Layer& layer);

	void attach(VideoSystemChangeListener& listener);
	void detach(VideoSystemChangeListener& listener);

	Layer* findActiveLayer() const;
	const Layers& getAllLayers() const { return layers; }

private:
	void resetVideoSystem();
	void setWindowTitle();

	// EventListener interface
	int signalEvent(const std::shared_ptr<const Event>& event) override;

	// RTSchedulable
	void executeRT() override;

	// Observer<Setting> interface
	void update(const Setting& setting) override;

	void checkRendererSwitch();
	void doRendererSwitch();
	void doRendererSwitch2();

	/** Find frontmost opaque layer.
	  */
	Layers::iterator baseLayer();

	// LayerListener interface
	void updateZ(Layer& layer) override;

	Layers layers; // sorted on z
	std::unique_ptr<VideoSystem> videoSystem;

	std::vector<VideoSystemChangeListener*> listeners; // unordered

	// fps related data
	static const unsigned NUM_FRAME_DURATIONS = 50;
	CircularBuffer<uint64_t, NUM_FRAME_DURATIONS> frameDurations;
	uint64_t frameDurationSum;
	uint64_t prevTimeStamp;

	struct ScreenShotCmd final : Command {
		ScreenShotCmd(CommandController& commandController);
		void execute(array_ref<TclObject> tokens, TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	} screenShotCmd;

	struct FpsInfoTopic final : InfoTopic {
		FpsInfoTopic(InfoCommand& openMSXInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} fpsInfo;

	OSDGUI osdGui;

	Reactor& reactor;
	RenderSettings renderSettings;
	CommandConsole commandConsole;

	// the current renderer
	RenderSettings::RendererID currentRenderer;

	bool renderFrozen;
	bool switchInProgress;
};

} // namespace openmsx

#endif

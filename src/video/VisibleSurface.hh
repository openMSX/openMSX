#ifndef VISIBLESURFACE_HH
#define VISIBLESURFACE_HH

#include "EventListener.hh"
#include "Observer.hh"
#include "OutputSurface.hh"
#include "RTSchedulable.hh"
#include "SDLSurfacePtr.hh"

#include <memory>
#include <optional>

namespace openmsx {

class CliComm;
class CommandConsole;
class Display;
class EventDistributor;
class ImGuiManager;
class InputEventGenerator;
class Layer;
class OSDGUI;
class Reactor;
class Setting;
class VideoSystem;
class BooleanSetting;

/** An OutputSurface which is visible to the user, such as a window or a
  * full screen display.
  */
class VisibleSurface final : public OutputSurface, public EventListener
                                , private Observer<Setting>, private RTSchedulable
{
public:
	VisibleSurface(Display& display,
	               RTScheduler& rtScheduler,
	               EventDistributor& eventDistributor,
	               InputEventGenerator& inputEventGenerator,
	               CliComm& cliComm,
	               VideoSystem& videoSystem,
		       BooleanSetting& pauseSetting);
	~VisibleSurface() override;

	[[nodiscard]] CliComm& getCliComm() const { return cliComm; }
	[[nodiscard]] Display& getDisplay() const { return display; }

	static void saveScreenshotGL(const OutputSurface& output,
	                             const std::string& filename);

	[[nodiscard]] std::optional<gl::vec2> getMouseCoord() const;
	void updateWindowTitle();
	bool setFullScreen(bool fullscreen);
	void resize();

	/** When a complete frame is finished, call this method.
	  * It will 'actually' display it. E.g. when using double buffering
	  * it will swap the front and back buffer.
	  */
	void finish();

	[[nodiscard]] std::unique_ptr<Layer> createSnowLayer();
	[[nodiscard]] std::unique_ptr<Layer> createOSDGUILayer(OSDGUI& gui);
	[[nodiscard]] std::unique_ptr<Layer> createImGUILayer(ImGuiManager& manager);

	/** Create an off-screen OutputSurface which has similar properties
	  * as this VisibleSurface. E.g. used to re-render the current frame
	  * without OSD elements to take a screenshot.
	  */
	[[nodiscard]] std::unique_ptr<OutputSurface> createOffScreenSurface();

	void fullScreenUpdated(bool fullScreen);

	/** Returns x,y coordinates of top-left window corner,
	    or returns a nullopt when in fullscreen mode. */
	[[nodiscard]] std::optional<gl::ivec2> getWindowPosition() const;
	void setWindowPosition(gl::ivec2 pos);

	// OutputSurface
	void saveScreenshot(const std::string& filename) override;

	// Observer
	void update(const Setting& setting) noexcept override;

	// EventListener
	bool signalEvent(const Event& event) override;

	// RTSchedulable
	void executeRT() override;

private:
	void updateCursor();
	void createSurface(gl::ivec2 size, unsigned flags);
	void setViewPort(gl::ivec2 logicalSize, bool fullScreen);

private:
	Display& display;
	EventDistributor& eventDistributor;
	InputEventGenerator& inputEventGenerator;
	CliComm& cliComm;
	VideoSystem& videoSystem;
	BooleanSetting& pauseSetting;

	struct VSyncObserver : openmsx::Observer<Setting> {
		void update(const Setting& setting) noexcept override;
	} vSyncObserver;

	[[no_unique_address]] SDLSubSystemInitializer<SDL_INIT_VIDEO> videoSubSystem;
	SDLWindowPtr window;
	SDL_GLContext glContext;

	bool grab = false;
	bool guiActive = false;

	static int windowPosX;
	static int windowPosY;
};

} // namespace openmsx

#endif

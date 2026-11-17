#ifndef VISIBLESURFACE_HH
#define VISIBLESURFACE_HH

#include "EventListener.hh"
#include "Observer.hh"
#include "OutputSurface.hh"
#include "RTSchedulable.hh"
#include "SDLSurfacePtr.hh"

#include <cstdint>
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

	[[nodiscard]] std::optional<gl::ivec2> getMouseCoord() const;
	[[nodiscard]] gl::vec2 getMsxPixelSize() const;
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

	/** Show, hide or change the host cursor, depending on fullscreen,
	  * grab-input, pointer_hide_delay, ImGui and crosshair requests. */
	void updateCursor();

private:
	void createSurface(gl::ivec2 size, SDL_WindowFlags flags);
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
	void updateSwapInterval();
	void setVSyncSuspended(bool suspend);
	void suspendVSyncForEvent();
	void suspendVSyncForSlowSwaps();
	void resumeVSyncForEvent();
	void checkSwapTime(uint64_t start);

	[[no_unique_address]] SDLSubSystemInitializer<SDL_INIT_VIDEO> videoSubSystem;
	SDLWindowPtr window;
	SDL_GLContext glContext;

	bool grab = false;
	bool guiActive = false;
	// On some systems, a buffer swap with vsync of a window that isn't
	// visible blocks for a long time (e.g. a second under Xwayland on GNOME,
	// see issue #1508), and that stalls the emulation. So vsync is suspended
	// (without changing the vsync setting):
	// - while the window is minimized or occluded (according to the window
	//   events), until an event says it may be visible again,
	// - when consecutive buffer swaps take much longer than a frame. Not all
	//   systems send an event when the window becomes visible again (e.g. a
	//   covered window under X11), so then vsync is also switched on again
	//   after a while, to see whether that or the next buffer swap still
	//   blocks ('probing'). The first retry comes after 30 s, the next ones
	//   every 60 s, as long as the window turns out to be still hidden.
	bool vSyncSuspended = false;
	bool vSyncProbing = false;
	int slowSwaps = 0; // consecutive slow buffer swaps
	int fastSwaps = 0; // consecutive fast buffer swaps while probing
	// The time until vsync is switched on again after slow buffer swaps
	// suspended it: initially, and the maximum after doubling (in
	// microseconds, like Timer::getTime()).
	static constexpr uint64_t VSYNC_RETRY_DELAY_MIN =  30'000'000;
	static constexpr uint64_t VSYNC_RETRY_DELAY_MAX =  60'000'000;
	uint64_t vSyncRetryTime = 0; // 0 = no retry
	uint64_t vSyncRetryDelay = VSYNC_RETRY_DELAY_MIN;

	static int windowPosX;
	static int windowPosY;
};

} // namespace openmsx

#endif

#ifndef SDLVIDEOSYSTEM_HH
#define SDLVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "EventListener.hh"
#include "Observer.hh"
#include "gl_vec.hh"
#include "components.hh"
#include <memory>

namespace openmsx {

class Display;
class Layer;
class Reactor;
class RenderSettings;
class Setting;
class VisibleSurface;

class SDLVideoSystem final : public VideoSystem, private EventListener
                           , private Observer<Setting>
{
public:
	/** Activates this video system.
	  * @throw InitException If initialisation fails.
	  */
	explicit SDLVideoSystem(Reactor& reactor);

	/** Deactivates this video system.
	  */
	~SDLVideoSystem() override;

	// VideoSystem interface:
	[[nodiscard]] std::unique_ptr<Rasterizer> createRasterizer(VDP& vdp) override;
	[[nodiscard]] std::unique_ptr<V9990Rasterizer> createV9990Rasterizer(
		V9990& vdp) override;
#if COMPONENT_LASERDISC
	std::unique_ptr<LDRasterizer> createLDRasterizer(
		LaserdiscPlayer& ld) override;
#endif
	void flush() override;
	void takeScreenShot(const std::string& filename, bool withOsd) override;
	void updateWindowTitle() override;
	[[nodiscard]] gl::vec2 getMouseCoord() override;
	[[nodiscard]] OutputSurface* getOutputSurface() override;
	void showCursor(bool show) override;
	[[nodiscard]] bool getCursorEnabled() override;
	[[nodiscard]] std::string getClipboardText() override;
	void setClipboardText(zstring_view text) override;
	[[nodiscard]] std::optional<gl::ivec2> getWindowPosition() override;
	void setWindowPosition(gl::ivec2 pos) override;
	void repaint() override;

private:
	// EventListener
	bool signalEvent(const Event& event) override;
	// Observer
	void update(const Setting& subject) noexcept override;

private:
	Reactor& reactor;
	Display& display;
	RenderSettings& renderSettings;
	std::unique_ptr<VisibleSurface> screen;
	std::unique_ptr<Layer> snowLayer;
	std::unique_ptr<Layer> iconLayer;
	std::unique_ptr<Layer> osdGuiLayer;
	std::unique_ptr<Layer> imGuiLayer;
};

} // namespace openmsx

#endif

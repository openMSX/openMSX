#ifndef VIDEOSYSTEM_HH
#define VIDEOSYSTEM_HH

#include "gl_vec.hh"
#include <string>
#include <memory>
#include "components.hh"

namespace openmsx {

class Rasterizer;
class VDP;
class V9990Rasterizer;
class LDRasterizer;
class V9990;
class LaserdiscPlayer;
class OutputSurface;

/** Video back-end system.
  */
class VideoSystem
{
public:
	virtual ~VideoSystem() = default;

	/** Create the rasterizer selected by the current renderer setting.
	  * Video systems that use a rasterizer must override this method.
	  * @param vdp The VDP whose display will be rendered.
	  * @return The rasterizer created.
	  */
	[[nodiscard]] virtual std::unique_ptr<Rasterizer> createRasterizer(VDP& vdp) = 0;

	/** Create the V9990 rasterizer selected by the current renderer setting.
	  * Video systems that use a rasterizer must override this method.
	  * @param vdp The V9990 whose display will be rendered.
	  * @return The rasterizer created.
	  */
	[[nodiscard]] virtual std::unique_ptr<V9990Rasterizer> createV9990Rasterizer(
		V9990& vdp) = 0;

#if COMPONENT_LASERDISC
	[[nodiscard]] virtual std::unique_ptr<LDRasterizer> createLDRasterizer(
		LaserdiscPlayer &ld) = 0;
#endif

	/** Requests that this renderer checks its settings against the
	  * current RenderSettings. If possible, update the settings of this
	  * renderer.
	  * The implementation in the Renderer base class checks whether the
	  * right renderer is selected. Subclasses are encouraged to check
	  * more settings.
	  * @return True if the settings were still in sync
	  *     or were successfully synced;
	  *     false if the renderer is unable to bring the settings in sync.
	  * TODO: Text copied from Renderer interface,
	  *       if this stays here then rewrite text accordingly.
	  */
	[[nodiscard]] virtual bool checkSettings();

	/** Finish pending drawing operations and make them visible to the user.
	  */
	virtual void flush() = 0;

	/** Take a screenshot.
	  * The default implementation throws an exception.
	  * @param filename Name of the file to save the screenshot to.
	  * @param withOsd Should OSD elements be included in the screenshot.
	  * @throws MSXException If taking the screen shot fails.
	  */
	virtual void takeScreenShot(const std::string& filename, bool withOsd);

	/** Called when the window title string has changed.
	  */
	virtual void updateWindowTitle();

	/** Returns the current mouse pointer coordinates.
	  */
	virtual gl::ivec2 getMouseCoord() = 0;

	/** TODO */
	[[nodiscard]] virtual OutputSurface* getOutputSurface() = 0;
	virtual void showCursor(bool show) = 0;
	virtual bool getCursorEnabled() = 0;
	
	/** Requests a repaint of the output surface. An implementation might
	 *  start a repaint directly, or trigger a queued rendering. */
	virtual void repaint() = 0;

protected:
	VideoSystem() = default;
};

} // namespace openmsx

#endif

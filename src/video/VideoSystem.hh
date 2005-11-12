// $Id$

#ifndef VIDEOSYSTEM_HH
#define VIDEOSYSTEM_HH

#include <string>

namespace openmsx {

class Rasterizer;
class VDP;
class V9990Rasterizer;
class V9990;

/** Video back-end system.
  */
class VideoSystem
{
public:
	virtual ~VideoSystem();

	/** Create the rasterizer selected by the current renderer setting.
	  * Video systems that use a rasterizer must override this method.
	  * @param vdp The VDP whose display will be rendered.
	  * @return The rasterizer created.
	  */
	virtual Rasterizer* createRasterizer(VDP& vdp);

	/** Create the V9990 rasterizer selected by the current renderer setting.
	  * Video systems that use a rasterizer must override this method.
	  * @param vdp The V9990 whose display will be rendered.
	  * @return The rasterizer created.
	  */
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp) = 0;

	/** Requests that this renderer checks its settings against the
	  * current RenderSettings. If possible, update the settings of this
	  * renderer.
	  * The implementation in the Renderer base class checks whether the
	  * right renderer is selected. Subclasses are encouraged to check
	  * more settings.
	  * @return True if the settings were still in sync
	  * 	or were succesfully synced;
	  * 	false if the renderer is unable to bring the settings in sync.
	  * TODO: Text copied from Renderer interface,
	  *       if this stays here then rewrite text accordingly.
	  */
	virtual bool checkSettings();

	/** Prepare video system for drawing operations.
	  * The default implementation does nothing, successfully.
	  * @return true iff successful.
	  * TODO: What should be done if preparation is not successful?
	  */
	virtual bool prepare();

	/** Finish pending drawing operations and make them visible to the user.
	  */
	virtual void flush() = 0;

	/** Take a screenshot.
	  * The default implementation throws an exception.
	  * @param filename Name of the file to save the screenshot to.
	  * @throw CommandException If taking the screen shot fails.
	  */
	virtual void takeScreenShot(const std::string& filename);

	/** Change the window title.
	  */
	virtual void setWindowTitle(const std::string& title);
};

} // namespace openmsx

#endif

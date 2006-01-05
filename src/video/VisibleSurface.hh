// $Id$

#ifndef VISIBLESURFACE_HH
#define VISIBLESURFACE_HH

#include "OutputSurface.hh"
#include <string>
#include <memory>
#include <SDL.h>

namespace openmsx {

class Layer;
class MSXMotherBoard;
class CommandController;
class EventDistributor;
class Display;
class IconStatus;

/** An OutputSurface which is visible to the user, such as a window or a
  * full screen display.
  * This class provides a frame buffer based renderer a common interface,
  * no matter whether the back-end is plain SDL or SDL+OpenGL.
  */
class VisibleSurface: public OutputSurface
{
public:
	virtual ~VisibleSurface();

	void setWindowTitle(const std::string& title);
	bool setFullScreen(bool fullscreen);
	virtual bool init() = 0;
	virtual void drawFrameBuffer() = 0;
	virtual void finish() = 0;
	virtual void takeScreenShot(const std::string& filename) = 0;

	virtual std::auto_ptr<Layer> createSnowLayer() = 0;
	virtual std::auto_ptr<Layer> createConsoleLayer(
		MSXMotherBoard& motherboard) = 0;
	virtual std::auto_ptr<Layer> createIconLayer(
		CommandController& commandController,
		Display& display, IconStatus& iconStatus) = 0;

protected:
	VisibleSurface();
	void createSurface(unsigned width, unsigned height, int flags);
};

} // namespace openmsx

#endif

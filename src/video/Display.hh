// $Id$

#ifndef DISPLAY_HH
#define DISPLAY_HH

#include "Layer.hh"
#include "EventListener.hh"
#include "Command.hh"
#include "openmsx.hh"
#include <memory>
#include <string>
#include <vector>

namespace openmsx {

class VideoSystem;

/** Represents the output window/screen of openMSX.
  * A display contains several layers.
  */
class Display: private EventListener, private LayerListener
{
public:
	// TODO: Keep as singleton?
	static std::auto_ptr<Display> INSTANCE;

	Display(std::auto_ptr<VideoSystem> videoSystem);
	virtual ~Display();

	virtual bool signalEvent(const Event& event);

	const std::auto_ptr<VideoSystem>& getVideoSystem() {
		return videoSystem;
	}

	/** Redraw the display.
	  */
	void repaint();

	void addLayer(Layer* layer);

private:
	virtual void updateCoverage(Layer* layer, Layer::Coverage coverage);
	virtual void updateZ(Layer* layer, Layer::ZIndex z);

	typedef std::vector<Layer*> Layers;

	/** Find frontmost opaque layer.
	  */
	Layers::iterator baseLayer();

	Layers layers;

	std::auto_ptr<VideoSystem> videoSystem;

	class ScreenShotCmd : public SimpleCommand {
	public:
		ScreenShotCmd(Display& display);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		Display& display;
	} screenShotCmd;
};

} // namespace openmsx

#endif // DISPLAY_HH


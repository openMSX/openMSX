// $Id$

#ifndef __DISPLAY_HH__
#define __DISPLAY_HH__

#include "EventListener.hh"
#include "Command.hh"
#include "openmsx.hh"
#include <memory>
#include <string>
#include <vector>

namespace openmsx {

class LayerListener;
class VideoSystem;


/** Interface for display layers.
  */
class Layer
{
public:
	virtual ~Layer();

	/** Paint this layer.
	  */
	virtual void paint() = 0;

	/** Returns the name of this layer. Used for debugging.
	  */
	virtual const std::string& getName() = 0;

protected:
	/** Describes how much of the screen is currently covered by a particular
	  * layer.
	  */
	enum Coverage {
		/** Layer fully covers the screen: any underlying layers are invisible.
		  */
		COVER_FULL,
		/** Layer partially covers the screen: it may cover only part of the
		  * screen area, or it may be (semi-)transparent in places.
		  */
		COVER_PARTIAL,
		/** Layer is not visible, that is completely transparent.
		  */
		COVER_NONE,
	};

	/** Determines stacking order of layers:
	  * layers with higher Z-indices are closer to the viewer.
	  */
	enum ZIndex {
		Z_DUMMY = -1,
		Z_BACKGROUND = 0,
		Z_MSX_PASSIVE = 30,
		Z_MSX_ACTIVE = 40,
		Z_ICONS = 90,
		Z_CONSOLE = 100,
	};

	/** Construct a layer. */
	Layer(Coverage coverage = COVER_NONE, ZIndex z = Z_DUMMY);

	/** Changes the current coverage of this layer.
	  */
	virtual void setCoverage(Coverage coverage);

	/** Changes the current Z-index of this layer.
	  */
	virtual void setZ(ZIndex z);

private:
	friend class LayerListener;
	friend class Display;

	/** The display this layer is part of. */
	LayerListener* display;

	/** Inspected by Display to determine which layers to paint. */
	Coverage coverage;

	/** Inspected by Display to determine the order in which layers are
	  * painted.
	  */
	ZIndex z;

};


class LayerListener {
public:
	virtual void updateCoverage(Layer* layer, Layer::Coverage coverage) = 0;
	virtual void updateZ(Layer* layer, Layer::ZIndex z) = 0;
};


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

	/** Find frontmost opaque layer.
	  */
	std::vector<Layer*>::iterator baseLayer();

	typedef std::vector<Layer*> Layers;
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

#endif //__DISPLAY_HH__


// $Id$

#ifndef __DISPLAY_HH__
#define __DISPLAY_HH__

//#include "VideoSystem.hh"
#include "EventListener.hh"
#include "openmsx.hh"
#include <memory>
#include <string>
#include <vector>

using std::auto_ptr;
using std::string;
using std::vector;




namespace openmsx {

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
	virtual const string& getName() = 0;
};


/** Represents the output window/screen of openMSX.
  * A display contains several layers.
  */
class Display: private EventListener
{
public:
	/** Systems that use display layers.
	  */
	/*
	enum {
		LAYER_VDP,
		LAYER_V9990,
		LAYER_CONSOLE,
	} LayerID;
	*/

	// TODO: Keep as singleton?
	static auto_ptr<Display> INSTANCE;

	Display(auto_ptr<VideoSystem> videoSystem);
	virtual ~Display();

	virtual bool signalEvent(const Event& event);

	const auto_ptr<VideoSystem>& getVideoSystem() {
		return videoSystem;
	}

	/** Redraw the display if a visible layer is dirty.
	  */
	void repaint();

	// TODO: Add as methods to LayerInfo and expose LayerInfo to outside.
	void addLayer(Layer* layer);
	void layerToBack(Layer* layer);
	void layerToFront(Layer* layer);
	void markDirty(Layer* layer);
	bool isActive(Layer* layer);
	void setAlpha(Layer* layer, byte alpha);

private:

	class LayerInfo {
	public:
		LayerInfo(Layer* layer);
		Layer* layer;
		bool dirty;
		byte alpha;
	};

	// TODO: Add as methods to LayerInfo and expose LayerInfo to outside.
	vector<LayerInfo*>::iterator findLayer(Layer* layer);

	/** Find frontmost opaque layer.
	  */
	vector<LayerInfo*>::iterator baseLayer();

	vector<LayerInfo*> layers;

	bool forceRepaint;
	auto_ptr<VideoSystem> videoSystem;
};

} // namespace openmsx

#endif //__DISPLAY_HH__


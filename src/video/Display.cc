// $Id$

#include "Display.hh"
#include "VideoSystem.hh"
#include "EventDistributor.hh"
#include "CommandException.hh"
#include <cassert>


namespace openmsx {

// Layer:

Layer::~Layer()
{
}

// Display:

auto_ptr<Display> Display::INSTANCE;

Display::Display(auto_ptr<VideoSystem> videoSystem)
{
	this->videoSystem = videoSystem;
	forceRepaint = false;

	EventDistributor::instance().registerEventListener(
		FINISH_FRAME_EVENT, *this, EventDistributor::NATIVE );
}

Display::~Display()
{
	EventDistributor::instance().unregisterEventListener(
		FINISH_FRAME_EVENT, *this, EventDistributor::NATIVE );
	for (vector<LayerInfo*>::iterator it = layers.begin();
		it != layers.end(); ++it
	) {
		delete (*it)->layer;
		delete *it;
	}
}

vector<Display::LayerInfo*>::iterator Display::baseLayer()
{
	vector<LayerInfo*>::iterator it = layers.end();
	while (true) {
		if (it == layers.begin()) {
			// There should always be at least one opaque layer.
			assert(false);
			return it;
		}
		--it;
		if ((*it)->alpha == 255) return it;
	}
}

bool Display::signalEvent(const Event& event)
{
	repaint();
	return true;
}

void Display::repaint()
{
	vector<LayerInfo*>::iterator it = baseLayer();
	bool mustPaint = forceRepaint;
	for (; it != layers.end(); ++it) {
		// TODO: Dirty check is wrong.
		//       Correct approach: draw if any layer is dirty.
		//if ((*it)->alpha != 0 && (mustPaint || (*it)->dirty)) {
		if ((*it)->alpha != 0) {
			assert(isActive((*it)->layer));
			// Paint this layer.
			//cerr << "Painting layer " << (*it)->layer->getName() << "\n";
			(*it)->layer->paint();
			(*it)->dirty = false;
			// All layers above this must be repainted too.
			mustPaint = true;
		}
	}
	forceRepaint = false;
	assert(videoSystem.get());
	videoSystem->flush();
}

void Display::addLayer(Layer* layer)
{
	layers.push_back(new LayerInfo(layer));
}

vector<Display::LayerInfo*>::iterator Display::findLayer(Layer* layer)
{
	for (vector<LayerInfo*>::iterator it = layers.begin();
		it != layers.end(); ++it
	) {
		if ((*it)->layer == layer) {
			return it;
		}
	}
	assert(false);
}

void Display::layerToBack(Layer* layer)
{
	vector<LayerInfo*>::iterator it = findLayer(layer);
	LayerInfo* info = *it;
	layers.erase(it);
	layers.insert(layers.begin(), info);
}

void Display::layerToFront(Layer* layer)
{
	vector<LayerInfo*>::iterator it = findLayer(layer);
	LayerInfo* info = *it;
	layers.erase(it);
	layers.push_back(info);
}

void Display::markDirty(Layer* layer)
{
	vector<LayerInfo*>::iterator it = findLayer(layer);
	(*it)->dirty = true;
}

bool Display::isActive(Layer* layer)
{
	vector<LayerInfo*>::iterator it = baseLayer();
	for (; it != layers.end(); ++it) {
		if ((*it)->layer == layer) {
			// Layer in active part of stack.
			return (*it)->alpha != 0;
		}
	}
	// Layer not in active part of stack.
	return false;
}

void Display::setAlpha(Layer* layer, byte alpha)
{
	vector<LayerInfo*>::iterator it = findLayer(layer);
	(*it)->alpha = alpha;
	forceRepaint = true;
}

// LayerInfo:

Display::LayerInfo::LayerInfo(Layer* layer)
{
	this->layer = layer;
	dirty = false;
	alpha = 0;
}

} // namespace openmsx


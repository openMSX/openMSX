// $Id$

#ifndef SDLSNOW_HH
#define SDLSNOW_HH

#include "Layer.hh"
#include "noncopyable.hh"

namespace openmsx {

class OutputSurface;
class Display;

/** Snow effect for background layer.
  */
template <class Pixel>
class SDLSnow : public Layer, private noncopyable
{
public:
	SDLSnow(OutputSurface& output, Display& display);

	// Layer interface:
	virtual void paint(OutputSurface& output);
	virtual string_ref getLayerName() const;

private:
	Display& display;

	/** Gray values for noise.
	  */
	Pixel gray[256];
};

} // namespace openmsx

#endif

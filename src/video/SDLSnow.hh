// $Id$

#ifndef SDLSNOW_HH
#define SDLSNOW_HH

#include "Layer.hh"
#include "noncopyable.hh"

namespace openmsx {

class OutputSurface;

/** Snow effect for background layer.
  */
template <class Pixel>
class SDLSnow : public Layer, private noncopyable
{
public:
	explicit SDLSnow(OutputSurface& output);

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

private:
	OutputSurface& output;

	/** Gray values for noise.
	  */
	Pixel gray[256];
};

} // namespace openmsx

#endif

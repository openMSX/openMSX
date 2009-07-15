// $Id$

#ifndef LDDUMMYRENDERER_HH
#define LDDUMMYRENDERER_HH

#include "LDRenderer.hh"
#include "Layer.hh"
#include "EmuTime.hh"

namespace openmsx {

/** Dummy Renderer
  */
class LDDummyRenderer : public LDRenderer, public Layer
{
public:
	// Renderer interface:
	void reset(EmuTime::param time);
	void frameStart(EmuTime::param time);
	void frameEnd(EmuTime::param time);

	// Layer interface:
	virtual void paint(OutputSurface& output);
	virtual const std::string& getName();

	void drawBlank(int r, int g, int b );
	void drawBitmap(byte *frame);
};

} // namespace openmsx

#endif

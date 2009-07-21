// $Id$

#ifndef LDDUMMYRENDERER_HH
#define LDDUMMYRENDERER_HH

#include "LDRenderer.hh"

namespace openmsx {

class LDDummyRenderer : public LDRenderer
{
public:
	void frameStart(EmuTime::param time);
	void frameEnd(EmuTime::param time);
	void drawBlank(int r, int g, int b );
	void drawBitmap(const byte* frame);
};

} // namespace openmsx

#endif

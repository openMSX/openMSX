// $Id$

#ifndef LDDUMMYRENDERER_HH
#define LDDUMMYRENDERER_HH

#include "LDRenderer.hh"

namespace openmsx {

class LDDummyRenderer : public LDRenderer
{
public:
	virtual void frameStart(EmuTime::param time);
	virtual void frameEnd();
	virtual void drawBlank(int r, int g, int b);
	virtual RawFrame* getRawFrame();
};

} // namespace openmsx

#endif

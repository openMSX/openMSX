// $Id$

#ifndef V9990DUMMYRENDERER_HH
#define V9990DUMMYRENDERER_HH

#include "V9990Renderer.hh"

namespace openmsx {

class V9990DummyRenderer : public V9990Renderer
{
public:
	// V9990Renderer interface:
	void reset(EmuTime::param time);
	void frameStart(EmuTime::param time);
	void frameEnd(EmuTime::param time);
	void renderUntil(EmuTime::param time);
	void updateDisplayEnabled(bool enabled, EmuTime::param time);
	void setDisplayMode(V9990DisplayMode mode, EmuTime::param time);
	void setColorMode(V9990ColorMode mode, EmuTime::param time);
	void updatePalette(int index, byte r, byte g, byte b, bool ys,
	                   EmuTime::param time);
	void updateSuperimposing(bool enabled, EmuTime::param time);
	void updateBackgroundColor(int index, EmuTime::param time);
	void updateScrollAX(EmuTime::param time);
	void updateScrollBX(EmuTime::param time);
	void updateScrollAYLow(EmuTime::param time);
	void updateScrollBYLow(EmuTime::param time);
};

} // namespace openmsx

#endif

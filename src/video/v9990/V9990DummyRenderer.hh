// $Id$

#ifndef V9990DUMMYRENDERER_HH
#define V9990DUMMYRENDERER_HH

#include "V9990Renderer.hh"

namespace openmsx {

class V9990DummyRenderer: public V9990Renderer
{
public:
	// V9990Renderer interface:
	void reset(const EmuTime& time);
	void frameStart(const EmuTime& time);
	void frameEnd(const EmuTime& time);
	void renderUntil(const EmuTime& time);
	void updateDisplayEnabled(bool enabled, const EmuTime& time);
	void setDisplayMode(V9990DisplayMode mode, const EmuTime& time);
	void setColorMode(V9990ColorMode mode, const EmuTime& time);
	void updatePalette(int index, byte r, byte g, byte b, const EmuTime& time);
	void updateBackgroundColor(int index, const EmuTime& time);
	void setImageWidth(int width);
	void updateScrollAX(const EmuTime& time);
	void updateScrollAY(const EmuTime& time);
	void updateScrollBX(const EmuTime& time);
	void updateScrollBY(const EmuTime& time);
};

} // namespace openmsx

#endif

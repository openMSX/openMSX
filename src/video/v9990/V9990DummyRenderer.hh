// $Id$

#ifndef __V9990DUMMYRENDERER_HH__
#define __V9990DUMMYRENDERER_HH__

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
	void setDisplayMode(V9990DisplayMode mode, const EmuTime& time);
	void setColorMode(V9990ColorMode mode, const EmuTime& time);
	void setPalette(int index, byte r, byte g, byte b, const EmuTime& time);
	void setBackgroundColor(int index);
	void setImageWidth(int width);
};

} // namespace openmsx

#endif

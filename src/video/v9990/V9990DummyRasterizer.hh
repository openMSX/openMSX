// $Id$

#ifndef V9990DUMMYRASTERIZER_HH
#define V9990DUMMYRASTERIZER_HH

#include "V9990Rasterizer.hh"

namespace openmsx {

class V9990DummyRasterizer : public V9990Rasterizer
{
public:
	// V9990Rasterizer interface
	virtual void paint();
	virtual ~V9990DummyRasterizer();
	virtual const std::string& getName();
	virtual void reset();
	virtual void frameStart();
	virtual void frameEnd();
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void setPalette(int index, byte r, byte g, byte b);
	virtual void setImageWidth(int width);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(int fromX, int fromY,
		                 int displayX, int displayY,
		                 int displayWidth, int displayHeight);
};

} // namespace openmsx

#endif

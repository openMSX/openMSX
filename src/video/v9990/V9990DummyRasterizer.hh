// $Id$

#ifndef _V9990DUMMYRASTERIZER_HH_
#define _V9990DUMMYRASTERIZER_HH_

#include "V9990Rasterizer.hh"

namespace openmsx {

class V9990DummyRasterizer : public V9990Rasterizer
{
public:
	virtual void paint();
	virtual const string& getName();
	virtual void reset();
	virtual void frameStart();
	virtual void frameEnd();
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(int fromX, int fromY,
		                 int displayX, int displayY,
		                 int displayWidth, int displayHeight);
};

} // namespace openmsx

#endif


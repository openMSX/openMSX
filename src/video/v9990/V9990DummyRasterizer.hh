// $Id$

#ifndef _V9990DUMMYRASTERIZER_HH_
#define _V9990DUMMYRASTERIZER_HH_

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
	virtual void frameStart(const V9990DisplayPeriod *horTiming,
                            const V9990DisplayPeriod *verTiming);
	virtual void frameEnd();
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void setBackgroundColor(int index);
	virtual void setPalette(int index, byte r, byte g, byte b);
	virtual void setImageWidth(int width);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(int fromX, int fromY,
		                 int displayX, int displayY,
		                 int displayWidth, int displayHeight);
};

} // namespace openmsx

#endif


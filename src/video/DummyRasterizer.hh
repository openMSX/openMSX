// $Id$

#ifndef DUMMYRASTERIZER_HH
#define DUMMYRASTERIZER_HH

#include "Rasterizer.hh"

namespace openmsx {

class DummyRasterizer : public Rasterizer
{
public:
	virtual bool isActive();
	virtual void reset();
	virtual void frameStart(EmuTime::param time);
	virtual void frameEnd();
	virtual void setDisplayMode(DisplayMode mode);
	virtual void setPalette(int index, int grb);
	virtual void setBackgroundColour(int index);
	virtual void setTransparency(bool enabled);
	virtual void setSuperimposing(bool enabled);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight);
	virtual void drawSprites(
		int fromX, int fromY,
		int displayX, int displayY,
		int displayWidth, int displayHeight);
	virtual bool isRecording() const;
};

} // namespace openmsx

#endif

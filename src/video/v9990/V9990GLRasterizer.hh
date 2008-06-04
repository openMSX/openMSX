// $Id$

#ifndef V9990GLRASTERIZER_HH
#define V9990GLRASTERIZER_HH

#include "V9990Rasterizer.hh"

namespace openmsx {

class V9990;

/** Rasterizer using SDL-GL
  */
class V9990GLRasterizer : public V9990Rasterizer
{
public:
	explicit V9990GLRasterizer(V9990& vdp);
	virtual ~V9990GLRasterizer();

	// Rasterizer interface:
	virtual bool isActive();
	virtual void reset();
	virtual void frameStart();
	virtual void frameEnd(const EmuTime& time);
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void setPalette(int index, byte r, byte g, byte b);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(int fromX, int fromY, int displayX,
	                         int displayY, int displayYA, int displayYB,
	                         int displayWidth, int displayHeight);
	virtual bool isRecording() const;
};

} // namespace openmsx

#endif

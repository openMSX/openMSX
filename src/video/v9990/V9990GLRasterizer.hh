// $Id$

#ifndef V9990GLRASTERIZER_HH
#define V9990GLRASTERIZER_HH

#include "V9990Rasterizer.hh"
#include "GLUtil.hh"
#include "openmsx.hh"

namespace openmsx {

class V9990;
class V9990VRAM;

/** Rasterizer using SDL-GL
  */
class V9990GLRasterizer : public V9990Rasterizer
{
public:
	/** Constructor
	  */
	V9990GLRasterizer(V9990* vdp);

	/** Destructor
	  */
	virtual ~V9990GLRasterizer();

	// Layer interface:
	virtual void paint();
	virtual const std::string& getName();

	// Rasterizer interface:
	virtual void reset();
	virtual void frameStart(const V9990DisplayPeriod *horTiming,
	                        const V9990DisplayPeriod *verTiming);
	virtual void frameEnd();
	virtual void setDisplayMode(V9990DisplayMode displayMode);
	virtual void setImageWidth(int width);
	virtual void setColorMode(V9990ColorMode colorMode);
	virtual void setPalette(int index, byte r, byte g, byte b);
	virtual void drawBorder(int fromX, int fromY, int limitX, int limitY);
	virtual void drawDisplay(int fromX, int fromY,
			         int displayX, int displayY,
				 int displayWidth, int displayHeight);

private:
	/** The VDP of which the video output is rendered
	  */
	V9990* vdp;

	/** The VRAM connected to the V9990 VDP
	  */
	V9990VRAM* vram;
};

} // namespace openmsx

#endif

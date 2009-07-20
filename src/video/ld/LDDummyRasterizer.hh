// $Id$

#ifndef LDDUMMYRASTERIZER_HH
#define LDDUMMYRASTERIZER_HH

#include "LDRasterizer.hh"

namespace openmsx {

class LDDummyRasterizer : public LDRasterizer
{
public:
	virtual bool isActive();
	virtual void reset();
	virtual void frameStart(EmuTime::param time);
	virtual void frameEnd();
	virtual bool isRecording() const;
	virtual void drawBlank(int r, int g, int b);
	virtual void drawBitmap(const byte* frame);
};

} // namespace openmsx

#endif

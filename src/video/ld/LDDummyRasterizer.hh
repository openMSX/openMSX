// $Id$

#ifndef LDDUMMYRASTERIZER_HH
#define LDDUMMYRASTERIZER_HH

#include "LDRasterizer.hh"

namespace openmsx {

class LDDummyRasterizer : public LDRasterizer
{
public:
	virtual void frameStart(EmuTime::param time);
	virtual void drawBlank(int r, int g, int b);
	virtual void drawBitmap(const byte* frame);
};

} // namespace openmsx

#endif

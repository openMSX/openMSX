// $Id$

#ifndef LDRASTERIZER_HH
#define LDRASTERIZER_HH

#include "EmuTime.hh"
#include "openmsx.hh"

namespace openmsx {

class RawFrame;

class LDRasterizer
{
public:
	virtual ~LDRasterizer() {}
	virtual void frameStart(EmuTime::param time) = 0;
	virtual void drawBlank(int r, int g, int b) = 0;
	virtual RawFrame* getRawFrame() = 0;
};

} // namespace openmsx

#endif

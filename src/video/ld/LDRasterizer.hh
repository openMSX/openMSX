#ifndef LDRASTERIZER_HH
#define LDRASTERIZER_HH

#include "EmuTime.hh"

namespace openmsx {

class PostProcessor;
class RawFrame;

class LDRasterizer
{
public:
	virtual ~LDRasterizer() {}
	virtual PostProcessor* getPostProcessor() const = 0;
	virtual void frameStart(EmuTime::param time) = 0;
	virtual void drawBlank(int r, int g, int b) = 0;
	virtual RawFrame* getRawFrame() = 0;

protected:
	LDRasterizer() {}
};

} // namespace openmsx

#endif

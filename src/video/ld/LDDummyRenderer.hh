#ifndef LDDUMMYRENDERER_HH
#define LDDUMMYRENDERER_HH

#include "LDRenderer.hh"

namespace openmsx {

class LDDummyRenderer : public LDRenderer
{
public:
	void frameStart(EmuTime::param time) override;
	void frameEnd() override;
	void drawBlank(int r, int g, int b) override;
	RawFrame* getRawFrame() override;
};

} // namespace openmsx

#endif

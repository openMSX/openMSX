// $Id$

#ifndef DUMMYVIDEOSYSTEM_HH
#define DUMMYVIDEOSYSTEM_HH

#include "VideoSystem.hh"
#include "components.hh"

namespace openmsx {

class DummyVideoSystem : public VideoSystem
{
public:
	// VideoSystem interface:
	virtual Rasterizer* createRasterizer(VDP& vdp);
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
#ifdef COMPONENT_LASERDISC
	virtual LDRasterizer* createLDRasterizer(LaserdiscPlayer& ld);
#endif
	virtual void flush();
	virtual OutputSurface* getOutputSurface();
};

} // namespace openmsx

#endif

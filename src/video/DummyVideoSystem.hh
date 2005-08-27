// $Id$

#ifndef DUMMYVIDEOSYSTEM_HH
#define DUMMYVIDEOSYSTEM_HH

#include "VideoSystem.hh"

namespace openmsx {

class Display;

class DummyVideoSystem: public VideoSystem
{
public:
	DummyVideoSystem(Display& display);
	virtual ~DummyVideoSystem();

	// VideoSystem interface:
	virtual V9990Rasterizer* createV9990Rasterizer(V9990& vdp);
	virtual void flush();
};

} // namespace openmsx

#endif

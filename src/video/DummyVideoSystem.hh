// $Id$

#ifndef __DUMMYVIDEOSYSTEM_HH__
#define __DUMMYVIDEOSYSTEM_HH__

#include "VideoSystem.hh"


namespace openmsx {

class Renderer;

class DummyVideoSystem: public VideoSystem
{
public:
	DummyVideoSystem();
	virtual ~DummyVideoSystem();

	// VideoSystem interface:
	virtual void flush();

	/** TODO: Only here for backwards compatibility. */
	Renderer* renderer;
};

} // namespace openmsx

#endif // __DUMMYVIDEOSYSTEM_HH__

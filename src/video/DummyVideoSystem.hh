// $Id$

#ifndef __DUMMYVIDEOSYSTEM_HH__
#define __DUMMYVIDEOSYSTEM_HH__

#include "VideoSystem.hh"


namespace openmsx {


class DummyVideoSystem: public VideoSystem
{
public:
	DummyVideoSystem();
	virtual ~DummyVideoSystem();

	// VideoSystem interface:
	virtual void flush();
};

} // namespace openmsx

#endif // __DUMMYVIDEOSYSTEM_HH__

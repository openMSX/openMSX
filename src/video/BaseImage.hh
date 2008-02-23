// $Id$

#ifndef BASEIMAGE_HH
#define BASEIMAGE_HH

#include "openmsx.hh"
#include "noncopyable.hh"

namespace openmsx {

class BaseImage : private noncopyable
{
public:
	virtual ~BaseImage() {}
	virtual void draw(unsigned x, unsigned y, byte alpha = 255) = 0;
};

} // namespace openmsx

#endif

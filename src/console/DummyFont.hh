// $Id$

#ifndef __DUMMYFONT_HH__
#define __DUMMYFONT_HH__

#include "Font.hh"

namespace openmsx {

class DummyFont : public Font
{
public:
	DummyFont();
	virtual void drawText(const std::string& str, int x, int y, byte alpha);
};

} // namespace openmsx

#endif

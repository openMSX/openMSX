// $Id$

#ifndef DUMMYFONT_HH
#define DUMMYFONT_HH

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

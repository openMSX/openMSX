// $Id$

#include "DummyFont.hh"

namespace openmsx {

DummyFont::DummyFont()
{
	// dummy values
	charWidth  = 6;
	charHeight = 8;
}

void DummyFont::drawText(const std::string& /*str*/, int /*x*/, int /*y*/,
                         byte /*alpha*/)
{
	// do nothing
}

} // namespace openmsx

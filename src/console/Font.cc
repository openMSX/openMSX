// $Id$

#include "Font.hh"

namespace openmsx {

Font::~Font()
{
}

unsigned Font::getHeight() const
{
	return charHeight;
}

unsigned Font::getWidth() const
{
	return charWidth;
}

} // namespace openmsx

// $Id$

#include "Font.hh"


namespace openmsx {

Font::~Font()
{
}

int Font::getHeight() const
{
	return charHeight;
}

int Font::getWidth() const
{
	return charWidth;
}

} // namespace openmsx

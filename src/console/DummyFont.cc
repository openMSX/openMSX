// $Id$

#include "DummyFont.hh"


DummyFont::DummyFont()
{
	// dummy values
	charWidth  = 6;
	charHeight = 8;
}

void DummyFont::drawText(const string &string, int x, int y)
{
	// do nothing
}

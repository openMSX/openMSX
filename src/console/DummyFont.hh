// $Id$

#ifndef __DUMMYFONT_HH__
#define __DUMMYFONT_HH__

#include "Font.hh"


class DummyFont : public Font
{
	public:
		DummyFont();
		virtual void drawText(const string &string, int x, int y);
};

#endif

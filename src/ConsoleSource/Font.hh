// $Id$

#ifndef __FONT_HH__
#define __FONT_HH__

#include <string>


class Font
{
	public:
		virtual void drawText(const std::string &string,
		                      int x, int y) = 0;
		int getHeight() const;
		int getWidth() const;

	protected:
		int charWidth;
		int charHeight;
};

#endif

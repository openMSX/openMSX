// $Id$

#ifndef __FONT_HH__
#define __FONT_HH__

#include <string>

using std::string;


namespace openmsx {

class Font
{
	public:
		virtual ~Font();
		virtual void drawText(const string &string,
		                      int x, int y) = 0;
		unsigned getHeight() const;
		unsigned getWidth() const;

	protected:
		unsigned charWidth;
		unsigned charHeight;
};

} // namespace openmsx

#endif

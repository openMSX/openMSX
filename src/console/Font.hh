// $Id$

#ifndef __FONT_HH__
#define __FONT_HH__

#include "openmsx.hh"
#include <string>

namespace openmsx {

class Font
{
public:
	virtual ~Font();
	virtual void drawText(const std::string& string, int x, int y,
	                      byte alpha = 255) = 0;
	unsigned getHeight() const;
	unsigned getWidth() const;

protected:
	unsigned charWidth;
	unsigned charHeight;
};

} // namespace openmsx

#endif

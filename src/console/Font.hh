// $Id$

#ifndef FONT_HH
#define FONT_HH

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

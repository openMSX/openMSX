// $Id$

#ifndef __GLFONT_HH__
#define __GLFONT_HH__

#include "Font.hh"
#include "GLUtil.hh"

namespace openmsx {

class GLFont : public Font
{
public:
	GLFont(GLuint texture, int width, int height, GLfloat* texCoord);
	virtual ~GLFont();

	virtual void drawText(const std::string& str, int x, int y, byte alpha);

private:
	GLuint fontTexture;
	int listBase;
};

} // namespace openmsx

#endif // __GLFONT_HH__

// $Id$

#ifndef __GLFONT_HH__
#define __GLFONT_HH__

#include "GLUtil.hh"
#ifdef __OPENGL_AVAILABLE__

#include "Font.hh"


namespace openmsx {

class GLFont : public Font
{
public:
	GLFont(GLuint texture, int width, int height, GLfloat* texCoord);
	virtual ~GLFont();

	virtual void drawText(const string& string, int x, int y);

private:
	GLuint fontTexture;
	int listBase;
};

} // namespace openmsx

#endif // __OPENGL_AVAILABLE__
#endif // __GLFONT_HH__

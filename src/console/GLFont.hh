// $Id$

#ifndef GLFONT_HH
#define GLFONT_HH

#include "Font.hh"
#include "GLUtil.hh"
#include "noncopyable.hh"

namespace openmsx {

class GLFont : public Font, private noncopyable
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

#endif

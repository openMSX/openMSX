// $Id$

#ifndef __GLFONT_HH__
#define __GLFONT_HH__

#include "GLUtil.hh"
#ifdef __OPENGL_AVAILABLE__

#include <string>
#include "Font.hh"


class GLFont : public Font
{
	public:
		GLFont(GLuint texture, int width, int height, GLfloat *texCoord);
		virtual ~GLFont();

		virtual void drawText(const std::string &string, int x, int y);

	private:
		GLuint fontTexture;
		int listBase;
};

#endif // __OPENGL_AVAILABLE__
#endif // __GLFONT_HH__

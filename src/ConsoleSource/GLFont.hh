// $Id$

#ifndef __GLFONT_HH__
#define __GLFONT_HH__

#include "config.h"
#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#else // HAVE_GL_H
#include <gl.h>
#endif
#include <string>


class GLFont
{
	public:
		GLFont(GLuint texture, int width, int height, GLfloat *texCoord);
		~GLFont();

		void drawText(const std::string &string, int x, int y);
		int height();
		int width();

	private:
		int charWidth;
		int charHeight;
		GLuint fontTexture;
		int listBase;
};

#endif

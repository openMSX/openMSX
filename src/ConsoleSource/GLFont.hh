// $Id$

#ifndef __GLFONT_HH__
#define __GLFONT_HH__

#include <string>
#include "SDL/SDL_opengl.h"


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

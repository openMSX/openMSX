// $Id$

#ifndef __GLCONSOLE_HH__
#define __GLCONSOLE_HH__

// Only compile on systems that have OpenGL headers.
#include "config.h"
#if (defined(HAVE_GL_GL_H) || defined(HAVE_GL_H))
#define __GLCONSOLE_AVAILABLE__

#ifdef HAVE_GL_GL_H
#include <GL/gl.h>
#else // HAVE_GL_H
#include <gl.h>
#endif

#include "SDLInteractiveConsole.hh"

// forward declaration
class GLFont;


class GLConsole : public SDLInteractiveConsole
{
	public:
		GLConsole();
		virtual ~GLConsole();

		virtual void drawConsole();

	private:
		int powerOfTwo(int a);
		GLuint loadTexture(const std::string &context,
		                   const std::string &filename,
		                   int &width, int &height, GLfloat *texCoord);

		static const int BLINK_RATE = 500;
		static const int CHAR_BORDER = 4;

		GLFont *font;
		GLuint backgroundTexture;
		GLfloat backTexCoord[4];
		int consoleWidth;
		int consoleHeight;
		int dispX;
		int dispY;
		bool blink;
		Uint32 lastBlinkTime;
};

#endif	// OpenGL header check.
#endif	// __GLCONSOLE_HH__

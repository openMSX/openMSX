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

#include "OSDConsoleRenderer.hh"


class GLConsole : public OSDConsoleRenderer
{
	public:
		GLConsole();
		virtual ~GLConsole();

		virtual bool loadFont(const std::string &filename);
		virtual bool loadBackground(const std::string &filename);
		virtual void drawConsole();
		virtual void updateConsole();

	private:
		int powerOfTwo(int a);
		bool loadTexture(const std::string &filename, GLuint &texture,
		                 int &width, int &height, GLfloat *texCoord);

		static const int BLINK_RATE = 500;
		static const int CHAR_BORDER = 4;

		class Font *font;
		GLuint backgroundTexture;
		BackgroundSetting* backgroundSetting;
		FontSetting* fontSetting;
		GLfloat backTexCoord[4];
		int consoleWidth;
		int consoleHeight;
		int dispX;
		int dispY;
		bool blink;
		unsigned lastBlinkTime;
		class Console* console;
};

#endif	// OpenGL header check.
#endif	// __GLCONSOLE_HH__

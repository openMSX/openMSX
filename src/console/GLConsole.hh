// $Id$

#ifndef __GLCONSOLE_HH__
#define __GLCONSOLE_HH__

#include "GLUtil.hh"
#ifdef __OPENGL_AVAILABLE__

#ifdef HAVE_SDL_IMAGE_H
#include "SDL_image.h"
#else
#include "SDL/SDL_image.h"
#endif

#include "OSDConsoleRenderer.hh"

namespace openmsx {

class Console;

class GLConsole : public OSDConsoleRenderer
{
	public:
		GLConsole(Console * console_);
		virtual ~GLConsole();

		virtual bool loadFont(const string &filename);
		virtual bool loadBackground(const string &filename);
		virtual void drawConsole();
		virtual void updateConsole();

	private:
		int powerOfTwo(int a);
		bool loadTexture(const string &filename, GLuint &texture,
				int &width, int &height, GLfloat *texCoord);

		GLuint backgroundTexture;
		BackgroundSetting* backgroundSetting;
		FontSetting* fontSetting;
		GLfloat backTexCoord[4];
		int consoleWidth;
		int consoleHeight;
		int dispX;
		int dispY;
		Console* console;
		void updateConsoleRect(SDL_Surface *screen);
};

} // namespace openmsx

#endif // __OPENGL_AVAILABLE__
#endif // __GLCONSOLE_HH__

// $Id$

#ifndef __GLCONSOLE_HH__
#define __GLCONSOLE_HH__

#include "GLUtil.hh"
#ifdef __OPENGL_AVAILABLE__

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

#endif // __OPENGL_AVAILABLE__
#endif // __GLCONSOLE_HH__

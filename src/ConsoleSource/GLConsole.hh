// $Id$

#ifndef __GLCONSOLE_HH__
#define __GLCONSOLE_HH__

#include "EventListener.hh"
#include "InteractiveConsole.hh"
#include "Command.hh"
#include "SDL_opengl.h"

// forward declaration
class GLFont;


class GLConsole : public InteractiveConsole, private EventListener
{
	public:
		GLConsole();
		virtual ~GLConsole();

		virtual void drawConsole();

	private:
		virtual void updateConsole();
		virtual bool signalEvent(SDL_Event &event);
		int powerOfTwo(int a);
		GLuint loadTexture(const std::string &filename, int &width, int &height, GLfloat *texCoord);

		static const int BLINK_RATE = 500;
		static const int CHAR_BORDER = 4;

		bool isVisible;
		GLFont *font;
		GLuint backgroundTexture;
		GLfloat backTexCoord[4];
		int consoleWidth;
		int consoleHeight;
		int dispX;
		int dispY;
		bool blink;
		Uint32 lastBlinkTime;

		class ConsoleCmd : public Command {
			public:
				ConsoleCmd(GLConsole *cons);
				virtual void execute(const std::vector<std::string> &tokens);
				virtual void help   (const std::vector<std::string> &tokens);
			private:
				GLConsole *console;
		};
		friend class ConsoleCmd;
		ConsoleCmd consoleCmd;
};

#endif

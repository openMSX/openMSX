// $Id$

#ifndef __OSDCONSOLERENDERER_HH__
#define __OSDCONSOLERENDERER_HH__

#include "ConsoleRenderer.hh"
#include "Settings.hh"

class OSDConsoleRenderer;
class FileContext;


class BackgroundSetting : public FilenameSetting
{
	public:
		BackgroundSetting(OSDConsoleRenderer *console,
		                  const std::string &filename);

	protected:
		virtual bool checkUpdate(const std::string &newValue);

	private:
		OSDConsoleRenderer* console;
};

class FontSetting : public FilenameSetting
{
	public:
		FontSetting(OSDConsoleRenderer *console,
		            const std::string &filename);

	protected:
		virtual bool checkUpdate(const std::string &newValue);

	private:
		OSDConsoleRenderer* console;
};


class OSDConsoleRenderer : public ConsoleRenderer
{
	public:
		OSDConsoleRenderer();
		virtual ~OSDConsoleRenderer();
		virtual bool loadBackground(const std::string &filename) = 0;
		virtual bool loadFont(const std::string &filename) = 0;
		virtual void drawConsole() = 0;

	protected:
		/** How transparent is the console? (0=invisible, 255=opaque)
		  * Note that when using a background image on the GLConsole,
		  * that image's alpha channel is used instead.
		  */
		static const int CONSOLE_ALPHA = 180;

		static const int BLINK_RATE = 500;
		static const int CHAR_BORDER = 4;

		std::string fontName;
		std::string backgroundName;
		FileContext* context;
};

#endif

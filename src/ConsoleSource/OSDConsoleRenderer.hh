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
		virtual bool checkUpdate(const std::string &newValue,
		                         const EmuTime &time);

	private:
		OSDConsoleRenderer* console;
};

class FontSetting : public FilenameSetting
{
	public:
		FontSetting(OSDConsoleRenderer *console,
		            const std::string &filename);

	protected:
		virtual bool checkUpdate(const std::string &newValue,
		                         const EmuTime &time);

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
		std::string fontName;
		std::string backgroundName;
		FileContext* context;
};

#endif

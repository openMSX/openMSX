// $Id$

#ifndef __SDLINTERACTIVECONSOLE_HH__
#define __SDLINTERACTIVECONSOLE_HH__

#include "EventListener.hh"
#include "InteractiveConsole.hh"
#include "Settings.hh"

class SDLInteractiveConsole;
class FileContext;


class BackgroundSetting : public FilenameSetting
{
	public:
		BackgroundSetting(SDLInteractiveConsole *console,
		                  const std::string &filename);

	protected:
		virtual bool checkUpdate(const std::string &newValue,
		                         const EmuTime &time);

	private:
		SDLInteractiveConsole* console;
};

class FontSetting : public FilenameSetting
{
	public:
		FontSetting(SDLInteractiveConsole *console,
		            const std::string &filename);

	protected:
		virtual bool checkUpdate(const std::string &newValue,
		                         const EmuTime &time);

	private:
		SDLInteractiveConsole* console;
};


class SDLInteractiveConsole : public InteractiveConsole, private EventListener
{
	public:
		SDLInteractiveConsole();
		virtual ~SDLInteractiveConsole();
		virtual bool loadBackground(const std::string &filename) = 0;
		virtual bool loadFont(const std::string &filename) = 0;

	protected:
		std::string fontName;
		std::string backgroundName;
		FileContext* context;
		BooleanSetting consoleSetting;

	private:
		virtual bool signalEvent(SDL_Event &event, const EmuTime &time);
};

#endif

// $Id$

#ifndef __OSDCONSOLERENDERER_HH__
#define __OSDCONSOLERENDERER_HH__

#include "ConsoleRenderer.hh"
#include "Settings.hh"
#include "DummyFont.hh"
#include "Console.hh"

#include <string>
using std::string;

#include <SDL/SDL.h>


namespace openmsx {

class OSDConsoleRenderer;
class FileContext;


class BackgroundSetting : public FilenameSetting
{
public:
	BackgroundSetting(OSDConsoleRenderer* console, const string settingName,
					const string& filename);

	virtual bool checkFile(const string& filename);

private:
	OSDConsoleRenderer* console;
};

class FontSetting : public FilenameSetting
{
public:
	FontSetting(OSDConsoleRenderer* console, const string settingName,
				const string& filename);

	virtual bool checkFile(const string& filename);

private:
	OSDConsoleRenderer* console;
};

class OSDConsoleRenderer : public ConsoleRenderer
{
public:
	OSDConsoleRenderer(Console& console);
	virtual ~OSDConsoleRenderer();
	virtual bool loadBackground(const string& filename) = 0;
	virtual bool loadFont(const string& filename) = 0;
	virtual void drawConsole() = 0;

	void setBackgroundName(const string& name);
	void setFontName(const string& name);

protected:
	void updateConsoleRect(SDL_Rect& rect);
	void initConsoleSize(void);

	/** How transparent is the console? (0=invisible, 255=opaque)
	  * Note that when using a background image on the GLConsole,
	  * that image's alpha channel is used instead.
	  */
	static const int CONSOLE_ALPHA = 180;
	static const int BLINK_RATE = 500;
	static const int CHAR_BORDER = 4;

	int consoleRows;
	int consoleColumns;
	EnumSetting<Console::Placement>* consolePlacementSetting;
	IntegerSetting* consoleRowsSetting;
	IntegerSetting* consoleColumnsSetting;
	class Font* font;
	FileContext* context;
	bool blink;
	unsigned lastBlinkTime;
	unsigned lastCursorPosition;

private:
	void adjustColRow();

	int currentMaxX;
	int currentMaxY;
	Console& console;
};

} // namespace openmsx

#endif // __OSDCONSOLERENDERER_HH__

// $Id$

#ifndef __OSDCONSOLERENDERER_HH__
#define __OSDCONSOLERENDERER_HH__

#include "ConsoleRenderer.hh"
#include "Settings.hh"
#include "DummyFont.hh"

#ifdef HAVE_SDL_IMAGE_H
#include "SDL_image.h"
#else
#include "SDL/SDL_image.h"
#endif


class OSDConsoleRenderer;
class Console;
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
		OSDConsoleRenderer(Console * console_);
		virtual ~OSDConsoleRenderer();
		virtual bool loadBackground(const std::string &filename) = 0;
		virtual bool loadFont(const std::string &filename) = 0;
		virtual void drawConsole() = 0;
		
		enum Placement {
			CP_TOPLEFT,    CP_TOP,    CP_TOPRIGHT,
			CP_LEFT,       CP_CENTER, CP_RIGHT,
			CP_BOTTOMLEFT, CP_BOTTOM, CP_BOTTOMRIGHT
		};
		void setBackgroundName(const std::string &name);
		void setFontName(const std::string &name);
		
	protected:
		/** How transparent is the console? (0=invisible, 255=opaque)
		  * Note that when using a background image on the GLConsole,
		  * that image's alpha channel is used instead.
		  */
		
		void updateConsoleRect(SDL_Rect & rect);
		void initConsoleSize(void);
		static const int CONSOLE_ALPHA = 180;
		static const int BLINK_RATE = 500;
		static const int CHAR_BORDER = 4;
		int consoleRows;
		int consoleColumns;
		static Placement consolePlacement;
		static std::string fontName;
		static std::string backgroundName;
		EnumSetting<Placement> *consolePlacementSetting;
		IntegerSetting* consoleRowsSetting;
		IntegerSetting* consoleColumnsSetting;
		class Font *font;
		FileContext* context;
		bool blink;
		unsigned lastBlinkTime;
		int lastCursorPosition;

	private:
		void adjustColRow();

		static int wantedColumns;
		static int wantedRows;
		int currentMaxX;
		int currentMaxY;
		Console * console;
};

#endif

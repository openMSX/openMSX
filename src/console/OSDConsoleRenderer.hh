// $Id$

#ifndef __OSDCONSOLERENDERER_HH__
#define __OSDCONSOLERENDERER_HH__

#include <memory>
#include <string>
#include <SDL.h>	// TODO get rid of this
#include "Display.hh"
#include "EnumSetting.hh"
#include "FilenameSetting.hh"
#include "DummyFont.hh"
#include "Console.hh"
#include "NonInheritable.hh"
#include "SettingListener.hh"

using std::auto_ptr;
using std::string;

namespace openmsx {

class OSDConsoleRenderer;
class IntegerSetting;
class Font;
class InputEventGenerator;
class EventDistributor;
class BooleanSetting;

NON_INHERITABLE_PRE(BackgroundSetting)
class BackgroundSetting : public FilenameSettingBase,
                          NON_INHERITABLE(BackgroundSetting)
{
public:
	BackgroundSetting(OSDConsoleRenderer& console,
	                  const string& initialValue);
	virtual ~BackgroundSetting();

	virtual bool checkFile(const string& filename);

private:
	OSDConsoleRenderer& console;
};

NON_INHERITABLE_PRE(FontSetting)
class FontSetting : public FilenameSettingBase, NON_INHERITABLE(FontSetting)
{
public:
	FontSetting(OSDConsoleRenderer& console,
	            const string& initialValue);
	virtual ~FontSetting();

	virtual bool checkFile(const string& filename);

private:
	OSDConsoleRenderer& console;
};

class OSDConsoleRenderer: public Layer, private SettingListener
{
public:
	virtual ~OSDConsoleRenderer();
	virtual bool loadBackground(const string& filename) = 0;
	virtual bool loadFont(const string& filename) = 0;

protected:
	OSDConsoleRenderer(Console& console);
	void initConsole();
	void updateConsoleRect(SDL_Rect& rect);

	/** How transparent is the console? (0=invisible, 255=opaque)
	  * Note that when using a background image on the GLConsole,
	  * that image's alpha channel is used instead.
	  */
	static const int CONSOLE_ALPHA = 180;
	static const int BLINK_RATE = 500;
	static const int CHAR_BORDER = 4;

	enum Placement {
		CP_TOPLEFT,    CP_TOP,    CP_TOPRIGHT,
		CP_LEFT,       CP_CENTER, CP_RIGHT,
		CP_BOTTOMLEFT, CP_BOTTOM, CP_BOTTOMRIGHT
	};

	int consoleRows;
	int consoleColumns;
	auto_ptr<EnumSetting<Placement> > consolePlacementSetting;
	auto_ptr<IntegerSetting> consoleRowsSetting;
	auto_ptr<IntegerSetting> consoleColumnsSetting;
	auto_ptr<BackgroundSetting> backgroundSetting;
	auto_ptr<FontSetting> fontSetting;
	auto_ptr<Font> font;
	bool blink;
	unsigned lastBlinkTime;
	unsigned lastCursorPosition;
	SDL_Rect destRect;
	Console& console;

private:
	void adjustColRow();
	void update(const SettingLeafNode* setting);
	void setActive(bool active);

	bool active;
	EventDistributor& eventDistributor;
	InputEventGenerator& inputEventGenerator;
	BooleanSetting& consoleSetting;
};

} // namespace openmsx

#endif // __OSDCONSOLERENDERER_HH__

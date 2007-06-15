// $Id$

#ifndef OSDCONSOLERENDERER_HH
#define OSDCONSOLERENDERER_HH

#include "Layer.hh"
#include "Observer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>
#include <string>

namespace openmsx {

class Reactor;
class IntegerSetting;
class Font;
class Setting;
class BooleanSetting;
class FilenameSetting;
class Display;
class Console;
class OSDSettingChecker;
template <typename T> class EnumSetting;

class OSDConsoleRenderer : public Layer, private Observer<Setting>,
                           private noncopyable
{
public:
	virtual ~OSDConsoleRenderer();
	virtual void loadBackground(const std::string& filename) = 0;
	virtual void loadFont(const std::string& filename) = 0;
	virtual unsigned getScreenW() const = 0;
	virtual unsigned getScreenH() const = 0;

	Display& getDisplay() const;
	Console& getConsole() const;

protected:
	explicit OSDConsoleRenderer(Reactor& reactor);
	void initConsole();
	bool updateConsoleRect();
	byte getVisibility() const;

	/** How transparent is the console? (0=invisible, 255=opaque)
	  * Note that when using a background image on the GLConsole,
	  * that image's alpha channel is used instead.
	  */
	static const int CONSOLE_ALPHA = 180;
	static const unsigned long long BLINK_RATE = 500000; // us
	static const int CHAR_BORDER = 4;

	enum Placement {
		CP_TOPLEFT,    CP_TOP,    CP_TOPRIGHT,
		CP_LEFT,       CP_CENTER, CP_RIGHT,
		CP_BOTTOMLEFT, CP_BOTTOM, CP_BOTTOMRIGHT
	};

	std::auto_ptr<EnumSetting<Placement> > consolePlacementSetting;
	std::auto_ptr<IntegerSetting> consoleRowsSetting;
	std::auto_ptr<IntegerSetting> consoleColumnsSetting;
	std::auto_ptr<FilenameSetting> backgroundSetting;
	std::auto_ptr<FilenameSetting> fontSetting;
	std::auto_ptr<Font> font;

	unsigned long long lastBlinkTime;
	unsigned destX;
	unsigned destY;
	unsigned destW;
	unsigned destH;
	unsigned lastCursorX;
	unsigned lastCursorY;
	bool blink;

private:
	void adjustColRow();
	void update(const Setting& setting);
	void setActive(bool active);

	unsigned long long time;
	Reactor& reactor;
	BooleanSetting& consoleSetting;
	bool active;
        const std::auto_ptr<OSDSettingChecker> settingChecker;

        friend class OSDSettingChecker;
};

} // namespace openmsx

#endif

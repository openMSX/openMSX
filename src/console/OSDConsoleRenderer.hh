// $Id$

#ifndef OSDCONSOLERENDERER_HH
#define OSDCONSOLERENDERER_HH

#include "Layer.hh"
#include "Observer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class Reactor;
class CommandConsole;
class TTFFont;
class BaseImage;
class Setting;
class BooleanSetting;
class IntegerSetting;
class FilenameSetting;
class OSDSettingChecker;
template <typename T> class EnumSetting;

class OSDConsoleRenderer : public Layer, private Observer<Setting>,
                           private noncopyable
{
public:
	OSDConsoleRenderer(Reactor& reactor, unsigned screenW, unsigned screenH,
	                   bool openGL);
	~OSDConsoleRenderer();

private:
	// Layer
	virtual void paint(OutputSurface& output);
	virtual const std::string& getName();

	// Observer
	void update(const Setting& setting);

	void adjustColRow();
	void setActive(bool active);

	bool updateConsoleRect();
	void loadFont      (const std::string& value);
	void loadBackground(const std::string& value);
	byte getVisibility() const;
	void drawText(OutputSurface& output, const std::string& text,
	              int x, int y, byte alpha);

	enum Placement {
		CP_TOPLEFT,    CP_TOP,    CP_TOPRIGHT,
		CP_LEFT,       CP_CENTER, CP_RIGHT,
		CP_BOTTOMLEFT, CP_BOTTOM, CP_BOTTOMRIGHT
	};

	Reactor& reactor;
	CommandConsole& console;
	BooleanSetting& consoleSetting;
	const std::auto_ptr<OSDSettingChecker> settingChecker;
	std::auto_ptr<EnumSetting<Placement> > consolePlacementSetting;
	std::auto_ptr<IntegerSetting> fontSizeSetting;
	std::auto_ptr<IntegerSetting> consoleRowsSetting;
	std::auto_ptr<IntegerSetting> consoleColumnsSetting;
	std::auto_ptr<FilenameSetting> backgroundSetting;
	std::auto_ptr<FilenameSetting> fontSetting;
	std::auto_ptr<TTFFont> font;
	std::auto_ptr<BaseImage> backgroundImage;

	unsigned long long lastBlinkTime;
	unsigned long long activeTime;
	const unsigned screenW;
	const unsigned screenH;
	unsigned destX;
	unsigned destY;
	unsigned destW;
	unsigned destH;
	unsigned lastCursorX;
	unsigned lastCursorY;
	bool blink;
	bool active;
	const bool openGL;

	friend class OSDSettingChecker;
};

} // namespace openmsx

#endif

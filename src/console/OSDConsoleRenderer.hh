// $Id$

#ifndef OSDCONSOLERENDERER_HH
#define OSDCONSOLERENDERER_HH

#include "Layer.hh"
#include "Observer.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <list>
#include <memory>

namespace openmsx {

class Reactor;
class CommandConsole;
class ConsoleLine;
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
	OSDConsoleRenderer(Reactor& reactor, CommandConsole& console,
	                   unsigned screenW, unsigned screenH, bool openGL);
	~OSDConsoleRenderer();

private:
	// Layer
	virtual void paint(OutputSurface& output);
	virtual string_ref getLayerName() const;

	// Observer
	void update(const Setting& setting);

	void adjustColRow();
	void setActive(bool active);

	bool updateConsoleRect();
	void loadFont      (const std::string& value);
	void loadBackground(const std::string& value);
	byte getVisibility() const;
	void drawText(OutputSurface& output, const ConsoleLine& text,
	              int x, int y, byte alpha);
	void drawText2(OutputSurface& output, string_ref text,
                       int& x, int y, byte alpha, unsigned rgb);

	bool getFromCache(string_ref text, unsigned rgb,
	                  BaseImage*& image, unsigned& width);
	void insertInCache(const std::string& text, unsigned rgb,
	                   std::unique_ptr<BaseImage> image, unsigned width);

	enum Placement {
		CP_TOPLEFT,    CP_TOP,    CP_TOPRIGHT,
		CP_LEFT,       CP_CENTER, CP_RIGHT,
		CP_BOTTOMLEFT, CP_BOTTOM, CP_BOTTOMRIGHT
	};

	struct TextCacheElement {
		TextCacheElement(const std::string& text_, unsigned rgb_,
		                 std::unique_ptr<BaseImage> image_,
		                 unsigned width_);
		TextCacheElement(TextCacheElement&& rhs);

		std::string text;
		std::unique_ptr<BaseImage> image;
		unsigned rgb;
		unsigned width;
	};
	typedef std::list<TextCacheElement> TextCache;

	Reactor& reactor;
	CommandConsole& console;
	BooleanSetting& consoleSetting;
	const std::unique_ptr<OSDSettingChecker> settingChecker;
	std::unique_ptr<EnumSetting<Placement>> consolePlacementSetting;
	std::unique_ptr<IntegerSetting> fontSizeSetting;
	std::unique_ptr<IntegerSetting> consoleRowsSetting;
	std::unique_ptr<IntegerSetting> consoleColumnsSetting;
	std::unique_ptr<FilenameSetting> backgroundSetting;
	std::unique_ptr<FilenameSetting> fontSetting;
	std::unique_ptr<TTFFont> font;
	std::unique_ptr<BaseImage> backgroundImage;
	TextCache textCache;
	TextCache::iterator cacheHint;

	uint64_t lastBlinkTime;
	uint64_t activeTime;
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

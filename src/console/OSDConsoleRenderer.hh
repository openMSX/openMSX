#ifndef OSDCONSOLERENDERER_HH
#define OSDCONSOLERENDERER_HH

#include "Layer.hh"
#include "TTFFont.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "FilenameSetting.hh"
#include "Observer.hh"
#include "string_ref.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <list>
#include <memory>

namespace openmsx {

class Reactor;
class CommandConsole;
class ConsoleLine;
class BaseImage;
class BooleanSetting;

class OSDConsoleRenderer final : public Layer, private Observer<Setting>
                               , private noncopyable
{
public:
	OSDConsoleRenderer(Reactor& reactor, CommandConsole& console,
	                   unsigned screenW, unsigned screenH, bool openGL);
	~OSDConsoleRenderer();

private:
	int initFontAndGetColumns();
	int getRows();

	// Layer
	void paint(OutputSurface& output) override;

	// Observer
	void update(const Setting& setting) override;

	void adjustColRow();
	void setActive(bool active);

	bool updateConsoleRect();
	void loadFont      (string_ref value);
	void loadBackground(string_ref value);
	byte getVisibility() const;
	void drawText(OutputSurface& output, const ConsoleLine& text,
	              int x, int y, byte alpha);
	void drawText2(OutputSurface& output, string_ref text,
                       int& x, int y, byte alpha, unsigned rgb);

	bool getFromCache(string_ref text, unsigned rgb,
	                  BaseImage*& image, unsigned& width);
	void insertInCache(const std::string& text, unsigned rgb,
	                   std::unique_ptr<BaseImage> image, unsigned width);
	void clearCache();

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
	const unsigned screenW;
	const unsigned screenH;
	const bool openGL;

	TTFFont font;
	TextCache textCache;
	TextCache::iterator cacheHint;

	EnumSetting<Placement> consolePlacementSetting;
	IntegerSetting fontSizeSetting;
	FilenameSetting fontSetting;
	IntegerSetting consoleColumnsSetting;
	IntegerSetting consoleRowsSetting;
	FilenameSetting backgroundSetting;
	std::unique_ptr<BaseImage> backgroundImage;

	uint64_t lastBlinkTime;
	uint64_t activeTime;
	unsigned destX;
	unsigned destY;
	unsigned destW;
	unsigned destH;
	unsigned lastCursorX;
	unsigned lastCursorY;
	bool blink;
	bool active;
};

} // namespace openmsx

#endif

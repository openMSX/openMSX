#ifndef OSDCONSOLERENDERER_HH
#define OSDCONSOLERENDERER_HH

#include "BaseImage.hh"
#include "Layer.hh"
#include "TTFFont.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "FilenameSetting.hh"
#include "Observer.hh"
#include "gl_vec.hh"
#include "openmsx.hh"
#include <list>
#include <memory>
#include <string_view>
#include <tuple>

namespace openmsx {

class BooleanSetting;
class CommandConsole;
class Display;
class Reactor;

class OSDConsoleRenderer final : public Layer, private Observer<Setting>
{
public:
	OSDConsoleRenderer(Reactor& reactor, CommandConsole& console,
	                   int screenW, int screenH, bool openGL);
	~OSDConsoleRenderer() override;

private:
	[[nodiscard]] int initFontAndGetColumns();
	[[nodiscard]] int getRows();

	// Layer
	void paint(OutputSurface& output) override;

	// Observer
	void update(const Setting& setting) noexcept override;

	void adjustColRow();
	void setActive(bool active);

	bool updateConsoleRect();
	void loadFont      (std::string_view value);
	void loadBackground(std::string_view value);
	byte getVisibility() const;
	void drawText(OutputSurface& output, std::string_view text,
	              int cx, int cy, byte alpha, uint32_t rgb);
	[[nodiscard]] gl::ivec2 getTextPos(int cursorX, int cursorY) const;
	void drawConsoleText(OutputSurface& output, byte visibility);

	[[nodiscard]] std::tuple<bool, BaseImage*, unsigned> getFromCache(
		std::string_view text, uint32_t rgb);
	void insertInCache(std::string text, uint32_t rgb,
	                   std::unique_ptr<BaseImage> image, unsigned width);
	void clearCache();

private:
	enum Placement {
		CP_TOP_LEFT,    CP_TOP,    CP_TOP_RIGHT,
		CP_LEFT,        CP_CENTER, CP_RIGHT,
		CP_BOTTOM_LEFT, CP_BOTTOM, CP_BOTTOM_RIGHT
	};

	struct TextCacheElement {
		TextCacheElement(std::string text_, uint32_t rgb_,
		                 std::unique_ptr<BaseImage> image_, unsigned width_)
			: text(std::move(text_)), image(std::move(image_))
			, rgb(rgb_), width(width_) {}

		std::string text;
		std::unique_ptr<BaseImage> image;
		uint32_t rgb;
		unsigned width; // in case of trailing whitespace width != image->getWidth()
	};
	using TextCache = std::list<TextCacheElement>;

	Reactor& reactor;
	Display& display;
	CommandConsole& console;
	BooleanSetting& consoleSetting;
	const int screenW;
	const int screenH;
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
	gl::ivec2 bgPos;
	gl::ivec2 bgSize;
	unsigned lastCursorX;
	unsigned lastCursorY;
	bool blink;
	bool active;
};

} // namespace openmsx

#endif

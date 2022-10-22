#include "OSDConsoleRenderer.hh"
#include "CommandConsole.hh"
#include "BooleanSetting.hh"
#include "SDLImage.hh"
#include "Display.hh"
#include "Timer.hh"
#include "FileContext.hh"
#include "CliComm.hh"
#include "Reactor.hh"
#include "MSXException.hh"
#include "openmsx.hh"
#include "one_of.hh"
#include "unreachable.hh"
#include "utf8_unchecked.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>
#include <memory>

#include "components.hh"
#if COMPONENT_GL
#include "GLImage.hh"
#endif

using namespace gl;

namespace openmsx {

/** How transparent is the console? (0=invisible, 255=opaque)
  * Note that when using a background image on the GLConsole,
  * that image's alpha channel is used instead.
  */
static constexpr int CONSOLE_ALPHA = 180;
static constexpr uint64_t BLINK_RATE = 500000; // us
static constexpr int CHAR_BORDER = 4;


// class OSDConsoleRenderer

static constexpr std::string_view defaultFont = "skins/VeraMono.ttf.gz";

OSDConsoleRenderer::OSDConsoleRenderer(
		Reactor& reactor_, CommandConsole& console_,
		unsigned screenW_, unsigned screenH_,
		bool openGL_)
	: Layer(COVER_NONE, Z_CONSOLE)
	, reactor(reactor_)
	, display(reactor.getDisplay()) // need to store because still needed during destructor
	, console(console_)
	, consoleSetting(console.getConsoleSetting())
	, screenW(screenW_)
	, screenH(screenH_)
	, openGL(openGL_)
	, consolePlacementSetting(
		reactor.getCommandController(), "consoleplacement",
		"position of the console within the emulator",
		// On Android, console must by default be placed on top, in
		// order to prevent that it overlaps with the virtual Android
		// keyboard, which is always placed at the bottom of the screen
		PLATFORM_ANDROID ? CP_TOP : CP_BOTTOM,
		EnumSetting<Placement>::Map{
			{"topleft",     CP_TOPLEFT},
			{"top",         CP_TOP},
			{"topright",    CP_TOPRIGHT},
			{"left",        CP_LEFT},
			{"center",      CP_CENTER},
			{"right",       CP_RIGHT},
			{"bottomleft",  CP_BOTTOMLEFT},
			{"bottom",      CP_BOTTOM},
			{"bottomright", CP_BOTTOMRIGHT}})
	, fontSizeSetting(reactor.getCommandController(),
		"consolefontsize", "Size of the console font", 12, 8, 32)
	, fontSetting(reactor.getCommandController(),
		"consolefont", "console font file", defaultFont)
	, consoleColumnsSetting(reactor.getCommandController(),
		"consolecolumns", "number of columns in the console",
		initFontAndGetColumns(), 32, 999)
	, consoleRowsSetting(reactor.getCommandController(),
		"consolerows", "number of rows in the console",
		getRows(), 1, 99)
	, backgroundSetting(reactor.getCommandController(),
		"consolebackground", "console background file",
		"skins/ConsoleBackgroundGrey.png")
{
#if !COMPONENT_GL
	assert(!openGL);
#endif
	bgPos = bgSize = ivec2(); // recalc on first paint()
	blink = false;
	lastBlinkTime = Timer::getTime();
	lastCursorX = lastCursorY = 0;

	active = false;
	activeTime = 0;
	setCoverage(COVER_PARTIAL);

	adjustColRow();

	// background (only load background on first paint())
	backgroundSetting.setChecker([this](TclObject& value) {
		loadBackground(value.getString());
	});
	// don't yet load background

	consoleSetting.attach(*this);
	fontSetting.attach(*this);
	fontSizeSetting.attach(*this);
	setActive(consoleSetting.getBoolean());
}

int OSDConsoleRenderer::initFontAndGetColumns()
{
	// init font
	fontSetting.setChecker([this](TclObject& value) {
		loadFont(value.getString());
	});
	try {
		loadFont(fontSetting.getString());
	} catch (MSXException&) {
		// This will happen when you upgrade from the old .png based
		// fonts to the new .ttf fonts. So provide a smooth upgrade path.
		reactor.getCliComm().printWarning(
			"Loading selected font (", fontSetting.getString(),
			") failed. Reverting to default font (", defaultFont, ").");
		fontSetting.setString(defaultFont);
		if (font.empty()) {
			// we can't continue without font
			throw FatalError("Couldn't load default console font.\n"
			                 "Please check your installation.");
		}
	}

	return (((screenW - CHAR_BORDER) / font.getWidth()) * 30) / 32;
}
int OSDConsoleRenderer::getRows()
{
	// initFontAndGetColumns() must already be called
	return ((screenH / font.getHeight()) * 6) / 15;
}
OSDConsoleRenderer::~OSDConsoleRenderer()
{
	fontSizeSetting.detach(*this);
	fontSetting.detach(*this);
	consoleSetting.detach(*this);
	setActive(false);
}

void OSDConsoleRenderer::adjustColRow()
{
	unsigned consoleColumns = std::min<unsigned>(
		consoleColumnsSetting.getInt(),
		(screenW - CHAR_BORDER) / font.getWidth());
	unsigned consoleRows = std::min<unsigned>(
		consoleRowsSetting.getInt(),
		screenH / font.getHeight());
	console.setColumns(consoleColumns);
	console.setRows(consoleRows);
}

void OSDConsoleRenderer::update(const Setting& setting) noexcept
{
	if (&setting == &consoleSetting) {
		setActive(consoleSetting.getBoolean());
	} else if (&setting == one_of(&fontSetting, &fontSizeSetting)) {
		loadFont(fontSetting.getString());
	} else {
		UNREACHABLE;
	}
}

void OSDConsoleRenderer::setActive(bool active_)
{
	if (active == active_) return;
	active = active_;

	display.repaintDelayed(40000); // 25 fps

	activeTime = Timer::getTime();
}

byte OSDConsoleRenderer::getVisibility() const
{
	const uint64_t FADE_IN_DURATION  = 100000;
	const uint64_t FADE_OUT_DURATION = 150000;

	auto now = Timer::getTime();
	auto dur = now - activeTime;
	if (active) {
		if (dur > FADE_IN_DURATION) {
			return 255;
		} else {
			display.repaintDelayed(40000); // 25 fps
			return byte((dur * 255) / FADE_IN_DURATION);
		}
	} else {
		if (dur > FADE_OUT_DURATION) {
			return 0;
		} else {
			display.repaintDelayed(40000); // 25 fps
			return byte(255 - ((dur * 255) / FADE_OUT_DURATION));
		}
	}
}

bool OSDConsoleRenderer::updateConsoleRect()
{
	adjustColRow();

	ivec2 size((font.getWidth()  * console.getColumns()) + CHAR_BORDER,
	            font.getHeight() * console.getRows());

	// TODO use setting listener in the future
	ivec2 pos;
	switch (consolePlacementSetting.getEnum()) {
		case CP_TOPLEFT:
		case CP_LEFT:
		case CP_BOTTOMLEFT:
			pos[0] = 0;
			break;
		case CP_TOPRIGHT:
		case CP_RIGHT:
		case CP_BOTTOMRIGHT:
			pos[0] = (screenW - size[0]);
			break;
		case CP_TOP:
		case CP_CENTER:
		case CP_BOTTOM:
		default:
			pos[0] = (screenW - size[0]) / 2;
			break;
	}
	switch (consolePlacementSetting.getEnum()) {
		case CP_TOPLEFT:
		case CP_TOP:
		case CP_TOPRIGHT:
			pos[1] = 0;
			break;
		case CP_LEFT:
		case CP_CENTER:
		case CP_RIGHT:
			pos[1] = (screenH - size[1]) / 2;
			break;
		case CP_BOTTOMLEFT:
		case CP_BOTTOM:
		case CP_BOTTOMRIGHT:
		default:
			pos[1] = (screenH - size[1]);
			break;
	}

	bool result = (pos != bgPos) || (size != bgSize);
	bgPos  = pos;
	bgSize = size;
	return result;
}

void OSDConsoleRenderer::loadFont(std::string_view value)
{
	auto filename = systemFileContext().resolve(value);
	auto newFont = TTFFont(filename, fontSizeSetting.getInt());
	if (!newFont.isFixedWidth()) {
		throw MSXException(value, " is not a monospaced font");
	}
	font = std::move(newFont);
	clearCache();
}

void OSDConsoleRenderer::loadBackground(std::string_view value)
{
	if (value.empty()) {
		backgroundImage.reset();
		return;
	}
	auto* output = display.getOutputSurface();
	if (!output) {
		backgroundImage.reset();
		return;
	}
	auto filename = systemFileContext().resolve(value);
	if (!openGL) {
		backgroundImage = std::make_unique<SDLImage>(*output, filename, bgSize);
	}
#if COMPONENT_GL
	else {
		backgroundImage = std::make_unique<GLImage>(*output, filename, bgSize);
	}
#endif
}

void OSDConsoleRenderer::drawText(OutputSurface& output, std::string_view text,
                                  int cx, int cy, byte alpha, uint32_t rgb)
{
	auto xy = getTextPos(cx, cy);
	auto [inCache, image, width] = getFromCache(text, rgb);
	if (!inCache) {
		std::string textStr(text);
		SDLSurfacePtr surf;
		uint32_t rgb2 = openGL ? 0xffffff : rgb; // openGL -> always render white
		try {
			width = font.getSize(textStr)[0];
			surf = font.render(textStr,
			                   (rgb2 >> 16) & 0xff,
			                   (rgb2 >>  8) & 0xff,
			                   (rgb2 >>  0) & 0xff);
		} catch (MSXException& e) {
			static bool alreadyPrinted = false;
			if (!alreadyPrinted) {
				alreadyPrinted = true;
				reactor.getCliComm().printWarning(
					"Invalid console text (invalid UTF-8): ",
					e.getMessage());
			}
			return; // don't cache negative results
		}
		std::unique_ptr<BaseImage> image2;
		if (!surf) {
			// nothing was rendered, so do nothing
		} else if (!openGL) {
			image2 = std::make_unique<SDLImage>(output, std::move(surf));
		}
#if COMPONENT_GL
		else {
			image2 = std::make_unique<GLImage>(output, std::move(surf));
		}
#endif
		image = image2.get();
		insertInCache(std::move(textStr), rgb, std::move(image2), width);
	}
	if (image) {
		if (openGL) {
			byte r = (rgb >> 16) & 0xff;
			byte g = (rgb >>  8) & 0xff;
			byte b = (rgb >>  0) & 0xff;
			image->draw(output, xy, r, g, b, alpha);
		} else {
			image->draw(output, xy, alpha);
		}
	}
}

std::tuple<bool, BaseImage*, unsigned> OSDConsoleRenderer::getFromCache(
	std::string_view text, uint32_t rgb)
{
	// Items are LRU sorted, so the next requested items will often be
	// located right in front of the previously found item. (Though
	// duplicate items (e.g. the command prompt '> ') degrade this
	// heuristic).
	auto it = cacheHint;
	// For openGL ignore rgb
	if ((it->text == text) && (openGL || (it->rgb  == rgb))) {
		goto found;
	}

	// Search the whole cache for a match. If the cache is big enough then
	// all N items used for rendering the previous frame should be located
	// in the N first positions in the cache (in approx reverse order).
	for (it = begin(textCache); it != end(textCache); ++it) {
		if (it->text != text) continue;
		if (!openGL && (it->rgb  != rgb)) continue;
found:		BaseImage* image = it->image.get();
		unsigned width = it->width;
		cacheHint = it;
		if (it != begin(textCache)) {
			--cacheHint; // likely candidate for next item
			// move to front (to keep in LRU order)
			textCache.splice(begin(textCache), textCache, it);
		}
		return {true, image, width};
	}
	return {false, nullptr, 0};
}

void OSDConsoleRenderer::insertInCache(
	std::string text, uint32_t rgb, std::unique_ptr<BaseImage> image,
	unsigned width)
{
	constexpr unsigned MAX_TEXT_CACHE_SIZE = 250;
	if (textCache.size() == MAX_TEXT_CACHE_SIZE) {
		// flush the least recently used entry
		if (auto it = std::prev(std::end(textCache)); it == cacheHint) {
			cacheHint = begin(textCache);
		}
		textCache.pop_back();
	}
	textCache.emplace_front(std::move(text), rgb, std::move(image), width);
}

void OSDConsoleRenderer::clearCache()
{
	// cacheHint must always point to a valid item, so insert a dummy entry
	textCache.clear();
	textCache.emplace_back(std::string{}, 0, nullptr, 0);
	cacheHint = begin(textCache);
}

gl::ivec2 OSDConsoleRenderer::getTextPos(int cursorX, int cursorY) const
{
	return bgPos + ivec2(CHAR_BORDER + cursorX * font.getWidth(),
	                     bgSize[1] - (font.getHeight() * (cursorY + 1)) - 1);
}

void OSDConsoleRenderer::paint(OutputSurface& output)
{
	byte visibility = getVisibility();
	if (!visibility) return;

	if (updateConsoleRect()) {
		try {
			loadBackground(backgroundSetting.getString());
		} catch (MSXException& e) {
			reactor.getCliComm().printWarning(e.getMessage());
		}
	}

	// draw the background image if there is one
	if (!backgroundImage) {
		// no background image, try to create an empty one
		try {
			if (!openGL) {
				backgroundImage = std::make_unique<SDLImage>(
					output, bgSize, CONSOLE_ALPHA);
			}
#if COMPONENT_GL
			else {
				backgroundImage = std::make_unique<GLImage>(
					output, bgSize, CONSOLE_ALPHA);
			}
#endif
		} catch (MSXException&) {
			// nothing
		}
	}
	if (backgroundImage) {
		backgroundImage->draw(output, bgPos, visibility);
	}

	drawConsoleText(output, visibility);

	// Check if the blink period is over
	auto now = Timer::getTime();
	if (lastBlinkTime < now) {
		lastBlinkTime = now + BLINK_RATE;
		blink = !blink;
	}

	auto [cursorX, cursorY] = console.getCursorPosition();
	if ((unsigned(cursorX) != lastCursorX) || (unsigned(cursorY) != lastCursorY)) {
		blink = true; // force cursor
		lastBlinkTime = now + BLINK_RATE; // maximum time
		lastCursorX = cursorX;
		lastCursorY = cursorY;
	}
	if (blink && (console.getScrollBack() == 0)) {
		drawText(output, "_", cursorX, cursorY, visibility, 0xffffff);
	}
}

void OSDConsoleRenderer::drawConsoleText(OutputSurface& output, byte visibility)
{
	const auto rows = console.getRows();
	const auto columns = console.getColumns();
	const auto scrollBack = console.getScrollBack();
	const auto& lines = console.getLines();

	// search first visible line
	auto [cursorY_, subLine, lineIdx] = [&] {
		size_t count = 0;
		size_t target = rows + scrollBack;
		for (auto idx : xrange(lines.size())) {
			count += std::max(size_t(1), (lines[idx].numChars() + columns - 1) / columns);
			if (count >= target) {
				return std::tuple(int(rows - 1), int(count - target), idx);
			}
		}
		int y = int(count - 1 - scrollBack);
		return std::tuple(y, 0, lines.size() - 1);
	}();
	int cursorY = cursorY_; // clang workaround

	// setup for first (partial) line
	std::string_view text = lines[lineIdx].str();
	auto it = begin(text);
	auto endIt = end(text);
	utf8::unchecked::advance(it, subLine * columns);
	std::string_view::size_type idx = it - begin(text);
	unsigned chunkIdx = 1;
	const auto& chunks0 = lines[lineIdx].getChunks();
	while ((chunkIdx < chunks0.size()) && (chunks0[chunkIdx].pos <= idx)) {
		++chunkIdx;
	}

	// draw each console-line (long text-lines are split over multiple console-lines)
	while (true) {
		auto remainingColumns = columns;
		const auto& chunks = lines[lineIdx].getChunks();
		if (!chunks.empty()) {
			// draw chunks of same color
			while (remainingColumns && (it < endIt)) {
				auto startColumn = remainingColumns;
				auto e = it;
				auto nextColorIt = (chunkIdx == chunks.size())
				                  ? endIt
				                  : begin(text) + chunks[chunkIdx].pos;
				auto maxIt = std::min(endIt, nextColorIt);
				while (remainingColumns && (e < maxIt)) {
					utf8::unchecked::next(e);
					--remainingColumns;
				}
				//std::string_view subText(it, e); // c++20
				std::string_view subText(&*it, e - it);
				auto rgb = chunks[chunkIdx - 1].rgb;
				auto cursorX = columns - startColumn;
				drawText(output, subText, cursorX, cursorY, visibility, rgb);

				// next chunk
				it = e;
				// move to next color?
				if (e == nextColorIt) ++chunkIdx;
			}
		}
		// all console-lines drawn?
		if (--cursorY < 0) break;
		// move to next text-line?
		if (it == endIt) {
			--lineIdx;
			text = lines[lineIdx].str();
			it = begin(text);
			endIt = end(text);
			chunkIdx = 1;
		}
	}
}

} // namespace openmsx

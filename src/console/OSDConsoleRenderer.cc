#include "OSDConsoleRenderer.hh"
#include "CommandConsole.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "FilenameSetting.hh"
#include "GlobalSettings.hh"
#include "SDLImage.hh"
#include "Display.hh"
#include "InputEventGenerator.hh"
#include "Timer.hh"
#include "FileContext.hh"
#include "CliComm.hh"
#include "Reactor.hh"
#include "MSXException.hh"
#include "openmsx.hh"
#include "memory.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <algorithm>
#include <cassert>

#include "components.hh"
#if COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;

namespace openmsx {

/** How transparent is the console? (0=invisible, 255=opaque)
  * Note that when using a background image on the GLConsole,
  * that image's alpha channel is used instead.
  */
static const int CONSOLE_ALPHA = 180;
static const uint64_t BLINK_RATE = 500000; // us
static const int CHAR_BORDER = 4;


// class OSDConsoleRenderer::TextCacheElement

OSDConsoleRenderer::TextCacheElement::TextCacheElement(
		const std::string& text_, unsigned rgb_,
		std::unique_ptr<BaseImage> image_, unsigned width_)
	: text(text_), image(std::move(image_)), rgb(rgb_), width(width_)
{
}

OSDConsoleRenderer::TextCacheElement::TextCacheElement(TextCacheElement&& rhs)
	: text(std::move(rhs.text)), image(std::move(rhs.image))
	, rgb(std::move(rhs.rgb)), width(std::move(rhs.width))
{
}


// class OSDConsoleRenderer

OSDConsoleRenderer::OSDConsoleRenderer(
		Reactor& reactor_, CommandConsole& console_,
		unsigned screenW_, unsigned screenH_,
		bool openGL_)
	: Layer(COVER_NONE, Z_CONSOLE)
	, reactor(reactor_)
	, console(console_)
	, consoleSetting(console.getConsoleSetting())
	, screenW(screenW_)
	, screenH(screenH_)
	, openGL(openGL_)
{
	// cacheHint must always point to a valid item, so insert a dummy entry
	textCache.push_back(TextCacheElement("", 0, nullptr, 0));
	cacheHint = textCache.begin();
#if !COMPONENT_GL
	assert(!openGL);
#endif
	destX = destY = destW = destH = 0; // recalc on first paint()
	blink = false;
	lastBlinkTime = Timer::getTime();
	lastCursorX = lastCursorY = 0;

	active = false;
	activeTime = 0;
	setCoverage(COVER_PARTIAL);

	// font size
	CommandController& commandController = reactor.getCommandController();
	fontSizeSetting = make_unique<IntegerSetting>(commandController,
		"consolefontsize", "Size of the console font", 12, 8, 32);

	// font
	const string& defaultFont = "skins/VeraMono.ttf.gz";
	fontSetting = make_unique<FilenameSetting>(commandController,
		"consolefont", "console font file", defaultFont);
	fontSetting->setChecker([this](TclObject& value) {
		loadFont(value.getString().str());
	});
	try {
		loadFont(fontSetting->getString());
	} catch (MSXException&) {
		// This will happen when you upgrade from the old .png based
		// fonts to the new .ttf fonts. So provide a smooth upgrade path.
		reactor.getCliComm().printWarning(
			"Loading selected font (" + fontSetting->getString() +
			") failed. Reverting to default font (" + defaultFont + ").");
		fontSetting->setString(defaultFont);
		if (font.empty()) {
			// we can't continue without font
			throw FatalError("Couldn't load default console font.\n"
			                 "Please check your installation.");
		}
	}

	// rows / columns
	int columns = (((screenW - CHAR_BORDER) / font.getWidth()) * 30) / 32;
	int rows = ((screenH / font.getHeight()) * 6) / 15;
	consoleColumnsSetting = make_unique<IntegerSetting>(commandController,
		"consolecolumns", "number of columns in the console", columns,
		32, 999);
	consoleRowsSetting = make_unique<IntegerSetting>(commandController,
		"consolerows", "number of rows in the console", rows, 1, 99);
	adjustColRow();

	// placement
	EnumSetting<Placement>::Map placeMap;
	placeMap["topleft"]     = CP_TOPLEFT;
	placeMap["top"]         = CP_TOP;
	placeMap["topright"]    = CP_TOPRIGHT;
	placeMap["left"]        = CP_LEFT;
	placeMap["center"]      = CP_CENTER;
	placeMap["right"]       = CP_RIGHT;
	placeMap["bottomleft"]  = CP_BOTTOMLEFT;
	placeMap["bottom"]      = CP_BOTTOM;
	placeMap["bottomright"] = CP_BOTTOMRIGHT;
	consolePlacementSetting = make_unique<EnumSetting<Placement>>(
		commandController, "consoleplacement",
		"position of the console within the emulator",
		CP_BOTTOM, placeMap);

	// background (only load backgound on first paint())
	backgroundSetting = make_unique<FilenameSetting>(commandController,
		"consolebackground", "console background file",
		"skins/ConsoleBackgroundGrey.png");
	backgroundSetting->setChecker([this](TclObject& value) {
		loadBackground(value.getString().str());
	});
	// don't yet load background

	consoleSetting.attach(*this);
	fontSizeSetting->attach(*this);
	setActive(consoleSetting.getBoolean());
}

OSDConsoleRenderer::~OSDConsoleRenderer()
{
	fontSizeSetting->detach(*this);
	consoleSetting.detach(*this);
	setActive(false);
}

void OSDConsoleRenderer::adjustColRow()
{
	unsigned consoleColumns = std::min<unsigned>(
		consoleColumnsSetting->getInt(),
		(screenW - CHAR_BORDER) / font.getWidth());
	unsigned consoleRows = std::min<unsigned>(
		consoleRowsSetting->getInt(),
		screenH / font.getHeight());
	console.setColumns(consoleColumns);
	console.setRows(consoleRows);
}

void OSDConsoleRenderer::update(const Setting& setting)
{
	if (&setting == &consoleSetting) {
		setActive(consoleSetting.getBoolean());
	} else if (&setting == fontSizeSetting.get()) {
		loadFont(fontSetting->getString());
	} else {
		UNREACHABLE;
	}
}

void OSDConsoleRenderer::setActive(bool active_)
{
	if (active == active_) return;
	active = active_;

	reactor.getDisplay().repaintDelayed(40000); // 25 fps

	activeTime = Timer::getTime();

	reactor.getInputEventGenerator().setKeyRepeat(active);
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
			reactor.getDisplay().repaintDelayed(40000); // 25 fps
			return byte((dur * 255) / FADE_IN_DURATION);
		}
	} else {
		if (dur > FADE_OUT_DURATION) {
			return 0;
		} else {
			reactor.getDisplay().repaintDelayed(40000); // 25 fps
			return byte(255 - ((dur * 255) / FADE_OUT_DURATION));
		}
	}
}

bool OSDConsoleRenderer::updateConsoleRect()
{
	adjustColRow();

	unsigned x, y, w, h;
	h = font.getHeight() * console.getRows();
	w = (font.getWidth() * console.getColumns()) + CHAR_BORDER;

	// TODO use setting listener in the future
	switch (consolePlacementSetting->getEnum()) {
		case CP_TOPLEFT:
		case CP_LEFT:
		case CP_BOTTOMLEFT:
			x = 0;
			break;
		case CP_TOPRIGHT:
		case CP_RIGHT:
		case CP_BOTTOMRIGHT:
			x = (screenW - w);
			break;
		case CP_TOP:
		case CP_CENTER:
		case CP_BOTTOM:
		default:
			x = (screenW - w) / 2;
			break;
	}
	switch (consolePlacementSetting->getEnum()) {
		case CP_TOPLEFT:
		case CP_TOP:
		case CP_TOPRIGHT:
			y = 0;
			break;
		case CP_LEFT:
		case CP_CENTER:
		case CP_RIGHT:
			y = (screenH - h) / 2;
			break;
		case CP_BOTTOMLEFT:
		case CP_BOTTOM:
		case CP_BOTTOMRIGHT:
		default:
			y = (screenH - h);
			break;
	}

	bool result = (x != destX) || (y != destY) ||
	              (w != destW) || (h != destH);
	destX = x; destY = y; destW = w; destH = h;
	return result;
}

void OSDConsoleRenderer::loadFont(const string& value)
{
	string filename = SystemFileContext().resolve(value);
	font = TTFFont(filename, fontSizeSetting->getInt());
}

void OSDConsoleRenderer::loadBackground(const string& value)
{
	if (value.empty()) {
		backgroundImage.reset();
		return;
	}
	string filename = SystemFileContext().resolve(value);
	if (!openGL) {
		backgroundImage = make_unique<SDLImage>(filename, destW, destH);
	}
#if COMPONENT_GL
	else {
		backgroundImage = make_unique<GLImage>(filename, destW, destH);
	}
#endif
}

void OSDConsoleRenderer::drawText(OutputSurface& output, const ConsoleLine& line,
                                  int x, int y, byte alpha)
{
	unsigned chunks = line.numChunks();
	for (unsigned i = 0; i < chunks; ++i) {
		unsigned rgb = line.chunkColor(i);
		string_ref text = line.chunkText(i);
		drawText2(output, text, x, y, alpha, rgb);
	}
}

void OSDConsoleRenderer::drawText2(OutputSurface& output, string_ref text,
                                   int& x, int y, byte alpha, unsigned rgb)
{
	unsigned width;
	BaseImage* image;
	if (!getFromCache(text, rgb, image, width)) {
		string textStr = text.str();
		SDLSurfacePtr surf;
		try {
			unsigned dummyHeight;
			font.getSize(textStr, width, dummyHeight);
			surf = font.render(textStr,
			                   (rgb >> 16) & 0xff,
			                   (rgb >>  8) & 0xff,
			                   (rgb >>  0) & 0xff);
		} catch (MSXException& e) {
			static bool alreadyPrinted = false;
			if (!alreadyPrinted) {
				alreadyPrinted = true;
				reactor.getCliComm().printWarning(
					"Invalid console text (invalid UTF-8): " +
					e.getMessage());
			}
			return; // don't cache negative results
		}
		std::unique_ptr<BaseImage> image2;
		if (!surf) {
			// nothing was rendered, so do nothing
		} else if (!openGL) {
			image2 = make_unique<SDLImage>(std::move(surf));
		}
#if COMPONENT_GL
		else {
			image2 = make_unique<GLImage>(std::move(surf));
		}
#endif
		image = image2.get();
		insertInCache(textStr, rgb, std::move(image2), width);
	}
	if (image) image->draw(output, x, y, alpha);
	x += width; // in case of trailing whitespace width != image->getWidth()
}

bool OSDConsoleRenderer::getFromCache(string_ref text, unsigned rgb,
                                      BaseImage*& image, unsigned& width)
{
	// Items are LRU sorted, so the next requested items will often be
	// located right in front of the previously found item. (Though
	// duplicate items (e.g. the command prompt '> ') degrade this
	// heuristic).
	auto it = cacheHint;
	if ((it->text == text) && (it->rgb  == rgb)) {
		goto found;
	}

	// Search the whole cache for a match. If the cache is big enough then
	// all N items used for rendering the previous frame should be located
	// in the N first positions in the cache (in approx reverse order).
	for (it = textCache.begin(); it != textCache.end(); ++it) {
		if (it->text != text) continue;
		if (it->rgb  != rgb ) continue;
found:		image = it->image.get();
		width = it->width;
		cacheHint = it;
		if (it != textCache.begin()) {
			--cacheHint; // likely candiate for next item
			// move to front (to keep in LRU order)
			textCache.splice(textCache.begin(), textCache, it);
		}
		return true;
	}
	return false;
}

void OSDConsoleRenderer::insertInCache(
	const string& text, unsigned rgb, std::unique_ptr<BaseImage> image,
	unsigned width)
{
	static const unsigned MAX_TEXT_CACHE_SIZE = 250;
	if (textCache.size() == MAX_TEXT_CACHE_SIZE) {
		// flush the least recently used entry
		auto it = textCache.end();
		--it;
		if (it == cacheHint) {
			cacheHint = textCache.begin();
		}
		textCache.pop_back();
	}
	textCache.push_front(TextCacheElement(
		text, rgb, std::move(image), width));
}

void OSDConsoleRenderer::paint(OutputSurface& output)
{
	byte visibility = getVisibility();
	if (!visibility) return;

	if (updateConsoleRect()) {
		try {
			loadBackground(backgroundSetting->getString());
		} catch (MSXException& e) {
			reactor.getCliComm().printWarning(e.getMessage());
		}
	}

	// draw the background image if there is one
	if (!backgroundImage) {
		// no background image, try to create an empty one
		try {
			if (!openGL) {
				backgroundImage = make_unique<SDLImage>(
					destW, destH, CONSOLE_ALPHA);
			}
#if COMPONENT_GL
			else {
				backgroundImage = make_unique<GLImage>(
					destW, destH, CONSOLE_ALPHA);
			}
#endif
		} catch (MSXException&) {
			// nothing
		}
	}
	if (backgroundImage) {
		backgroundImage->draw(output, destX, destY, visibility);
	}

	for (auto loop : xrange(destH / font.getHeight())) {
		drawText(output,
		         console.getLine(loop + console.getScrollBack()),
		         destX + CHAR_BORDER,
		         destY + destH - (1 + loop) * font.getHeight() - 1,
		         visibility);
	}

	// Check if the blink period is over
	auto now = Timer::getTime();
	if (lastBlinkTime < now) {
		lastBlinkTime = now + BLINK_RATE;
		blink = !blink;
	}

	unsigned cursorX, cursorY;
	console.getCursorPosition(cursorX, cursorY);
	if ((cursorX != lastCursorX) || (cursorY != lastCursorY)) {
		blink = true; // force cursor
		lastBlinkTime = now + BLINK_RATE; // maximum time
		lastCursorX = cursorX;
		lastCursorY = cursorY;
	}
	if (blink && (console.getScrollBack() == 0)) {
		drawText(output, ConsoleLine("_"),
		         destX + CHAR_BORDER + cursorX * font.getWidth(),
		         destY + destH - (font.getHeight() * (cursorY + 1)) - 1,
		         visibility);
	}
}

} // namespace openmsx

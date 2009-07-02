// $Id$

#include "OSDConsoleRenderer.hh"
#include "CommandConsole.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "FilenameSetting.hh"
#include "GlobalSettings.hh"
#include "TTFFont.hh"
#include "SDLImage.hh"
#include "Display.hh"
#include "InputEventGenerator.hh"
#include "Timer.hh"
#include "FileContext.hh"
#include "CliComm.hh"
#include "Reactor.hh"
#include "MSXException.hh"
#include "openmsx.hh"
#include <algorithm>
#include <cassert>

#include "components.hh"
#ifdef COMPONENT_GL
#include "GLImage.hh"
#endif

using std::string;

namespace openmsx {

/** How transparent is the console? (0=invisible, 255=opaque)
  * Note that when using a background image on the GLConsole,
  * that image's alpha channel is used instead.
  */
static const int CONSOLE_ALPHA = 180;
static const unsigned long long BLINK_RATE = 500000; // us
static const int CHAR_BORDER = 4;


class OSDSettingChecker : public SettingChecker<FilenameSetting::Policy>
{
public:
	OSDSettingChecker(OSDConsoleRenderer& renderer);
	virtual void check(SettingImpl<FilenameSetting::Policy>& setting,
	                   std::string& value);
private:
	OSDConsoleRenderer& renderer;
};


// class OSDConsoleRenderer

OSDConsoleRenderer::OSDConsoleRenderer(
		Reactor& reactor_, unsigned screenW_, unsigned screenH_,
		bool openGL_)
	: Layer(COVER_NONE, Z_CONSOLE)
	, reactor(reactor_)
	, console(reactor.getCommandConsole())
	, consoleSetting(reactor.getGlobalSettings().getConsoleSetting())
	, settingChecker(new OSDSettingChecker(*this))
	, screenW(screenW_)
	, screenH(screenH_)
	, openGL(openGL_)
{
#ifndef COMPONENT_GL
	assert(!openGL);
#endif
	destX = destY = destW = destH = 0; // avoid UMR
	blink = false;
	lastBlinkTime = Timer::getTime();
	lastCursorX = lastCursorY = 0;

	active = false;
	activeTime = 0;
	setCoverage(COVER_PARTIAL);

	// font size
	CommandController& commandController = reactor.getCommandController();
	fontSizeSetting.reset(new IntegerSetting(commandController,
		"consolefontsize", "Size of the console font", 12, 8, 32));

	// font
	const string& defaultFont = "skins/VeraMono.ttf.gz";
	fontSetting.reset(new FilenameSetting(commandController,
		"consolefont", "console font file", defaultFont));
	try {
		fontSetting->setChecker(settingChecker.get());
	} catch (MSXException&) {
		// This will happen when you upgrade from the old .png based
		// fonts to the new .ttf fonts. So provide a smooth upgrade path.
		reactor.getCliComm().printWarning(
			"Loading selected font (" + fontSetting->getValue() +
			") failed. Reverting to default font (" + defaultFont + ").");
		fontSetting->changeValue(defaultFont);
		if (!font.get()) {
			// we can't continue without font
			throw FatalError("Couldn't load default console font.\n"
			                 "Please check your installation.");
		}
	}

	// rows / columns
	int columns = (((screenW - CHAR_BORDER) / font->getWidth()) * 30) / 32;
	int rows = ((screenH / font->getHeight()) * 6) / 15;
	consoleColumnsSetting.reset(new IntegerSetting(commandController,
		"consolecolumns", "number of columns in the console", columns,
		32, 999));
	consoleRowsSetting.reset(new IntegerSetting(commandController,
		"consolerows", "number of rows in the console", rows, 1, 99));
	adjustColRow();

	// placement
	typedef EnumSetting<Placement>::Map PlaceMap;
	PlaceMap placeMap;
	placeMap["topleft"]     = CP_TOPLEFT;
	placeMap["top"]         = CP_TOP;
	placeMap["topright"]    = CP_TOPRIGHT;
	placeMap["left"]        = CP_LEFT;
	placeMap["center"]      = CP_CENTER;
	placeMap["right"]       = CP_RIGHT;
	placeMap["bottomleft"]  = CP_BOTTOMLEFT;
	placeMap["bottom"]      = CP_BOTTOM;
	placeMap["bottomright"] = CP_BOTTOMRIGHT;
	consolePlacementSetting.reset(new EnumSetting<Placement>(commandController,
		"consoleplacement", "position of the console within the emulator",
		CP_BOTTOM, placeMap));

	updateConsoleRect();

	// background
	backgroundSetting.reset(new FilenameSetting(commandController,
		"consolebackground", "console background file",
		"skins/ConsoleBackgroundGrey.png"));
	try {
		backgroundSetting->setChecker(settingChecker.get());
	} catch (MSXException& e) {
		reactor.getCliComm().printWarning(e.getMessage());
	}

	consoleSetting.attach(*this);
	fontSizeSetting->attach(*this);
	setActive(consoleSetting.getValue());
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
		consoleColumnsSetting->getValue(),
		(screenW - CHAR_BORDER) / font->getWidth());
	unsigned consoleRows = std::min<unsigned>(
		consoleRowsSetting->getValue(),
		screenH / font->getHeight());
	console.setColumns(consoleColumns);
	console.setRows(consoleRows);
}

void OSDConsoleRenderer::update(const Setting& setting)
{
	if (&setting == &consoleSetting) {
		setActive(consoleSetting.getValue());
	} else if (&setting == fontSizeSetting.get()) {
		loadFont(fontSetting->getValue());
	} else {
		assert(false);
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
	const unsigned long long FADE_IN_DURATION  = 100000;
	const unsigned long long FADE_OUT_DURATION = 150000;

	unsigned long long now = Timer::getTime();
	unsigned long long dur = now - activeTime;
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
	h = font->getHeight() * console.getRows();
	w = (font->getWidth() * console.getColumns()) + CHAR_BORDER;

	// TODO use setting listener in the future
	switch (consolePlacementSetting->getValue()) {
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
	switch (consolePlacementSetting->getValue()) {
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
	SystemFileContext context;
	CommandController* controller = NULL; // ok for SystemFileContext
	string filename = context.resolve(*controller, value);
	font.reset(new TTFFont(filename, fontSizeSetting->getValue()));
}

void OSDConsoleRenderer::loadBackground(const string& value)
{
	if (value.empty()) {
		backgroundImage.reset();
		return;
	}
	SystemFileContext context;
	CommandController* controller = NULL; // ok for SystemFileContext
	string filename = context.resolve(*controller, value);
	if (!openGL) {
		backgroundImage.reset(new SDLImage(filename, destW, destH));
	}
#ifdef COMPONENT_GL
	else {
		backgroundImage.reset(new GLImage(filename, destW, destH));
	}
#endif
}

void OSDConsoleRenderer::drawText(OutputSurface& output, const string& text,
                                  int x, int y, byte alpha)
{
	if (text.empty()) return;
	SDL_Surface* surf;
	try {
		surf = font->render(text, 255, 255, 255);
	} catch (MSXException& e) {
		static bool alreadyPrinted = false;
		if (!alreadyPrinted) {
			alreadyPrinted = true;
			reactor.getCliComm().printWarning(
				"Invalid console text (invalid UTF-8): " + e.getMessage());
		}
		return;
	}
	if (!openGL) {
		SDLImage image(surf);
		image.draw(output, x, y, alpha);
	}
#ifdef COMPONENT_GL
	else {
		GLImage image(surf);
		image.draw(output, x, y, alpha);
	}
#endif
}

void OSDConsoleRenderer::paint(OutputSurface& output)
{
	byte visibility = getVisibility();
	if (!visibility) return;

	if (updateConsoleRect()) {
		try {
			loadBackground(backgroundSetting->getValue());
		} catch (MSXException&) {
			// ignore
		}
	}

	// draw the background image if there is one
	if (!backgroundImage.get()) {
		// no background image, try to create an empty one
		try {
			if (!openGL) {
				backgroundImage.reset(new SDLImage(
					destW, destH, CONSOLE_ALPHA));
			}
#ifdef COMPONENT_GL
			else {
				backgroundImage.reset(new GLImage(
					destW, destH, CONSOLE_ALPHA));
			}
#endif
		} catch (MSXException&) {
			// nothing
		}
	}
	if (backgroundImage.get()) {
		backgroundImage->draw(output, destX, destY, visibility);
	}

	int screenlines = destH / font->getHeight();
	for (int loop = 0; loop < screenlines; ++loop) {
		drawText(output,
		         console.getLine(loop + console.getScrollBack()),
		         destX + CHAR_BORDER,
		         destY + destH - (1 + loop) * font->getHeight() - 1,
		         visibility);
	}

	// Check if the blink period is over
	unsigned long long now = Timer::getTime();
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
		drawText(output, "_",
		         destX + CHAR_BORDER + cursorX * font->getWidth(),
		         destY + destH - (font->getHeight() * (cursorY + 1)) - 1,
		         visibility);
	}
}

const string& OSDConsoleRenderer::getName()
{
	static const string NAME = "openMSX console";
	return NAME;
}


// class OSDSettingChecker

OSDSettingChecker::OSDSettingChecker(OSDConsoleRenderer& renderer_)
	: renderer(renderer_)
{
}

void OSDSettingChecker::check(SettingImpl<FilenameSetting::Policy>& setting,
	                      string& value)
{
	if (&setting == renderer.backgroundSetting.get()) {
		renderer.loadBackground(value);
	} else if (&setting == renderer.fontSetting.get()) {
		renderer.loadFont(value);
	} else {
		assert(false);
	}
}

} // namespace openmsx

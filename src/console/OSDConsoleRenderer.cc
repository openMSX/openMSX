// $Id$

#include "OSDConsoleRenderer.hh"
#include "CommandController.hh"
#include "CommandConsole.hh"
#include "EnumSetting.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "GlobalSettings.hh"
#include "Display.hh"
#include "Event.hh"
#include "InputEvents.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include "Timer.hh"
#include "DummyFont.hh"
#include "FileContext.hh"
#include "CliComm.hh"
#include "MSXMotherBoard.hh"
#include <algorithm>
#include <SDL.h>

using std::string;

namespace openmsx {

// class OSDConsoleRenderer

OSDConsoleRenderer::OSDConsoleRenderer(MSXMotherBoard& motherBoard_)
	: Layer(COVER_NONE, Z_CONSOLE)
	, motherBoard(motherBoard_)
	, consoleSetting(motherBoard.getCommandController().getGlobalSettings().
	                    getConsoleSetting())
{
	destX = destY = destW = destH = 0; // avoid UMR
	font.reset(new DummyFont());
	blink = false;
	lastBlinkTime = 0;
	lastCursorX = lastCursorY = 0;

	active = false;
	time = 0;
	setCoverage(COVER_PARTIAL);
	consoleSetting.attach(*this);
	setActive(consoleSetting.getValue());
}

OSDConsoleRenderer::~OSDConsoleRenderer()
{
	consoleSetting.detach(*this);
	setActive(false);
}

void OSDConsoleRenderer::initConsole()
{
	CommandController& commandController = motherBoard.getCommandController();
	// font
	fontSetting.reset(new FilenameSetting(commandController,
		"consolefont", "console font file",
		"skins/ConsoleFontRaveLShaded.png"));
	try {
		fontSetting->setChecker(this);
	} catch (MSXException& e) {
		// we really need a font
		throw FatalError(e.getMessage());
	}

	// rows / columns
	int columns = (((getScreenW() - CHAR_BORDER) / font->getWidth()) * 30) / 32;
	int rows = ((getScreenH() / font->getHeight()) * 6) / 15;
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
		backgroundSetting->setChecker(this);
	} catch (MSXException& e) {
		motherBoard.getCliComm().printWarning(e.getMessage());
	}
}

void OSDConsoleRenderer::adjustColRow()
{
	unsigned consoleColumns = std::min<unsigned>(
		consoleColumnsSetting->getValue(),
		(getScreenW() - CHAR_BORDER) / font->getWidth());
	unsigned consoleRows = std::min<unsigned>(
		consoleRowsSetting->getValue(),
		getScreenH() / font->getHeight());
	getConsole().setColumns(consoleColumns);
	getConsole().setRows(consoleRows);
}

void OSDConsoleRenderer::update(const Setting& setting)
{
	if (&setting); // avoid warning
	assert(&setting == &consoleSetting);
	setActive(consoleSetting.getValue());
}

void OSDConsoleRenderer::setActive(bool active_)
{
	if (active == active_) return;
	active = active_;

	getDisplay().repaintDelayed(40000); // 25 fps

	time = Timer::getTime();

	motherBoard.getInputEventGenerator().setKeyRepeat(active);
	if (active) {
		motherBoard.getEventDistributor().distributeEvent(
			new ConsoleEvent(OPENMSX_CONSOLE_ON_EVENT));
	} else {
		motherBoard.getEventDistributor().distributeEvent(
			new ConsoleEvent(OPENMSX_CONSOLE_OFF_EVENT));
	}
}

byte OSDConsoleRenderer::getVisibility() const
{
	const unsigned long long FADE_IN_DURATION  = 100000;
	const unsigned long long FADE_OUT_DURATION = 150000;

	unsigned long long now = Timer::getTime();
	unsigned long long dur = now - time;
	if (active) {
		if (dur > FADE_IN_DURATION) {
			return 255;
		} else {
			getDisplay().repaintDelayed(40000); // 25 fps
			return (dur * 255) / FADE_IN_DURATION;
		}
	} else {
		if (dur > FADE_OUT_DURATION) {
			return 0;
		} else {
			getDisplay().repaintDelayed(40000); // 25 fps
			return 255 - ((dur * 255) / FADE_OUT_DURATION);
		}
	}
}

bool OSDConsoleRenderer::updateConsoleRect()
{
	unsigned screenW = getScreenW();
	unsigned screenH = getScreenH();
	adjustColRow();

	unsigned x, y, w, h;
	h = font->getHeight() * getConsole().getRows();
	w = (font->getWidth() * getConsole().getColumns()) + CHAR_BORDER;

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

void OSDConsoleRenderer::check(SettingImpl<FilenameSetting::Policy>& setting,
                               string& value)
{
	SystemFileContext context;
	string filename = value.empty() ? value : context.resolve(value);
	if (&setting == backgroundSetting.get()) {
		loadBackground(filename);
	} else if (&setting == fontSetting.get()) {
		loadFont(filename);
	} else {
		assert(false);
	}
}

Display& OSDConsoleRenderer::getDisplay() const
{
	return motherBoard.getDisplay();
}

Console& OSDConsoleRenderer::getConsole() const
{
	return motherBoard.getCommandConsole();
}

} // namespace openmsx

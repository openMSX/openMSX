// $Id$

#include "OSDConsoleRenderer.hh"
#include "SettingsConfig.hh"
#include "CommandConsole.hh"
#include "File.hh"
#include "FileContext.hh"
#include "IntegerSetting.hh"
#include "GlobalSettings.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include <algorithm>
#include <SDL.h>


namespace openmsx {

// class BackgroundSetting

BackgroundSetting::BackgroundSetting(
	OSDConsoleRenderer& console_, const string& initialValue)
	: FilenameSettingBase("consolebackground", "console background file",
	                      initialValue)
	, console(console_)
{
	initSetting(SAVE_SETTING);
}

BackgroundSetting::~BackgroundSetting()
{
	exitSetting();
}

bool BackgroundSetting::checkFile(const string& filename)
{
	return console.loadBackground(filename);
}


// class FontSetting

FontSetting::FontSetting(
	OSDConsoleRenderer& console_, const string& initialValue)
	: FilenameSettingBase("consolefont", "console font file",
	                      initialValue)
	, console(console_)
{
	initSetting(SAVE_SETTING);
}

FontSetting::~FontSetting()
{
	exitSetting();
}

bool FontSetting::checkFile(const string& filename)
{
	return console.loadFont(filename);
}


// class OSDConsoleRenderer

OSDConsoleRenderer::OSDConsoleRenderer(Console& console_)
	: Layer(COVER_NONE, Z_CONSOLE)
	, console(console_)
	, eventDistributor(EventDistributor::instance())
	, inputEventGenerator(InputEventGenerator::instance())
	, consoleSetting(GlobalSettings::instance().getConsoleSetting())
{
	font.reset(new DummyFont());
	blink = false;
	lastBlinkTime = 0;
	unsigned cursorY;
	console.getCursorPosition(lastCursorPosition, cursorY);

	Display::INSTANCE->addLayer(this);
	active = false;
	consoleSetting.addListener(this);
	setActive(consoleSetting.getValue());
}

OSDConsoleRenderer::~OSDConsoleRenderer()
{
	consoleSetting.removeListener(this);
	setActive(false);
}

void OSDConsoleRenderer::initConsole()
{
	// font
	fontSetting.reset(new FontSetting(*this, "skins/ConsoleFont.png"));
	
	// rows / columns
	SDL_Surface* screen = SDL_GetVideoSurface();
	int columns = (((screen->w - CHAR_BORDER) / font->getWidth()) * 30) / 32;
	int rows = ((screen->h / font->getHeight()) * 6) / 15;
	consoleColumnsSetting.reset(new IntegerSetting("consolecolumns",
		"number of columns in the console", columns, 32, 999));
	consoleRowsSetting.reset(new IntegerSetting("consolerows",
		"number of rows in the console", rows, 1, 99));
	console.setColumns(consoleColumnsSetting->getValue());
	console.setRows(consoleRowsSetting->getValue());
	adjustColRow();
	console.setConsoleDimensions(consoleColumns, consoleRows);
	
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
	consolePlacementSetting.reset(new EnumSetting<Placement>(
		"consoleplacement", "position of the console within the emulator",
		CP_BOTTOM, placeMap));
	
	updateConsoleRect(destRect);
	
	// background
	backgroundSetting.reset(new BackgroundSetting(
		*this, "skins/ConsoleBackground.png"));
}

void OSDConsoleRenderer::adjustColRow()
{
	SDL_Surface* screen = SDL_GetVideoSurface();
	if (console.getColumns() > (unsigned)((screen->w - CHAR_BORDER) / font->getWidth())) {
		consoleColumns = (screen->w - CHAR_BORDER) / font->getWidth();
	} else {
		consoleColumns = console.getColumns();
	}
	if (console.getRows() > (unsigned)(screen->h / font->getHeight())) {
		consoleRows = screen->h / font->getHeight();
	} else {
		consoleRows = console.getRows();
	}
}

void OSDConsoleRenderer::update(const SettingLeafNode* setting)
{
	assert(setting == &consoleSetting);
	setActive(consoleSetting.getValue());
}

void OSDConsoleRenderer::setActive(bool active)
{
	if (this->active == active) return;
	this->active = active;
	inputEventGenerator.setKeyRepeat(active);
	setCoverage(active ? COVER_PARTIAL : COVER_NONE);
	if (active) {
		eventDistributor.registerEventListener(
			KEY_UP_EVENT,   console, EventDistributor::NATIVE );
		eventDistributor.registerEventListener(
			KEY_DOWN_EVENT, console, EventDistributor::NATIVE );
		eventDistributor.distributeEvent(
		  new SimpleEvent<CONSOLE_ON_EVENT>() );
	} else {
		eventDistributor.unregisterEventListener(
			KEY_DOWN_EVENT, console, EventDistributor::NATIVE );
		eventDistributor.unregisterEventListener(
			KEY_UP_EVENT,   console, EventDistributor::NATIVE );
		eventDistributor.distributeEvent(
		  new SimpleEvent<CONSOLE_OFF_EVENT>() );
	}
}

void OSDConsoleRenderer::updateConsoleRect(SDL_Rect& rect)
{
	SDL_Surface* screen = SDL_GetVideoSurface();
	
	// TODO use setting listener in the future
	console.setRows(consoleRowsSetting->getValue());
	console.setColumns(consoleColumnsSetting->getValue());
	adjustColRow();

	rect.h = font->getHeight() * consoleRows;
	rect.w = (font->getWidth() * consoleColumns) + CHAR_BORDER;
	console.setConsoleDimensions(consoleColumns, consoleRows);
	
	// TODO use setting listener in the future
	switch (consolePlacementSetting->getValue()) {
		case CP_TOPLEFT:
		case CP_LEFT:
		case CP_BOTTOMLEFT:
			rect.x = 0;
			break;
		case CP_TOPRIGHT:
		case CP_RIGHT:
		case CP_BOTTOMRIGHT:
			rect.x = (screen->w - rect.w);
			break;
		case CP_TOP:
		case CP_CENTER:
		case CP_BOTTOM:
		default:
			rect.x = (screen->w - rect.w) / 2;
			break;
	}
	switch (consolePlacementSetting->getValue()) {
		case CP_TOPLEFT:
		case CP_TOP:
		case CP_TOPRIGHT:
			rect.y = 0;
			break;
		case CP_LEFT:
		case CP_CENTER:
		case CP_RIGHT:
			rect.y = (screen->h - rect.h) / 2;
			break;
		case CP_BOTTOMLEFT:
		case CP_BOTTOM:
		case CP_BOTTOMRIGHT:
		default:
			rect.y = (screen->h - rect.h);
			break;
	}
}

} // namespace openmsx

// $Id$

#include "OSDConsoleRenderer.hh"
#include "SettingsConfig.hh"
#include "CommandConsole.hh"
#include "File.hh"
#include "FileContext.hh"
#include "IntegerSetting.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "InputEventGenerator.hh"
#include <algorithm>
#include <SDL.h>


namespace openmsx {

// class BackgroundSetting

BackgroundSetting::BackgroundSetting(
	OSDConsoleRenderer& console_, XMLElement& node)
	: FilenameSettingBase(node, "console background file")
	, console(console_)
{
	initSetting();
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
	OSDConsoleRenderer& console_, XMLElement& node)
	: FilenameSettingBase(node, "console font file")
	, console(console_)
{
	initSetting();
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

BooleanSetting OSDConsoleRenderer::consoleSetting(
	"console", "turns console display on/off", false);

OSDConsoleRenderer::OSDConsoleRenderer(Console& console_)
	: console(console_)
	, eventDistributor(EventDistributor::instance())
	, inputEventGenerator(InputEventGenerator::instance())
{
	font.reset(new DummyFont());
	blink = false;
	lastBlinkTime = 0;
	unsigned cursorY;
	console.getCursorPosition(lastCursorPosition, cursorY);

	Display::INSTANCE->addLayer(this, Display::Z_CONSOLE);
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
	XMLElement& config = SettingsConfig::instance().getCreateChild("console");

	// font
	XMLElement& fontElem = config.getCreateChild(
		"consolefont", "skins/ConsoleFont.png");
	fontSetting.reset(new FontSetting(*this, fontElem));
	
	// rows / columns
	SDL_Surface* screen = SDL_GetVideoSurface();
	int columns = (((screen->w - CHAR_BORDER) / font->getWidth()) * 30) / 32;
	int rows = ((screen->h / font->getHeight()) * 6) / 15;
	XMLElement& columnsElem = config.getCreateChild(
		"consolecolumns", StringOp::toString(columns));
	XMLElement& rowsElem    = config.getCreateChild(
		"consolerows",    StringOp::toString(rows));
	consoleColumnsSetting.reset(new IntegerSetting(
		columnsElem, "number of columns in the console", 32, 999));
	consoleRowsSetting.reset(new IntegerSetting(
		rowsElem, "number of rows in the console", 1, 99));
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
	XMLElement& placeElem   = config.getCreateChild("consoleplacement", "bottom");
	Placement placement = CP_BOTTOM;
	PlaceMap::const_iterator it = placeMap.find(placeElem.getData());
	if (it != placeMap.end()) {
		placement = it->second;
	}
	consolePlacementSetting.reset(new EnumSetting<Placement>(
		placeElem, "position of the console within the emulator",
		placement, placeMap));
	
	updateConsoleRect(destRect);
	
	// background
	XMLElement& backgroundElem = config.getCreateChild(
		"consolebackground", "skins/ConsoleBackground.png");
	backgroundSetting.reset(new BackgroundSetting(*this, backgroundElem));
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
	Display::INSTANCE->setCoverage(
		this, active ? Display::COVER_PARTIAL : Display::COVER_NONE );
	if (active) {
		eventDistributor.registerEventListener(
			KEY_UP_EVENT,   console, EventDistributor::NATIVE );
		eventDistributor.registerEventListener(
			KEY_DOWN_EVENT, console, EventDistributor::NATIVE );
	} else {
		eventDistributor.unregisterEventListener(
			KEY_DOWN_EVENT, console, EventDistributor::NATIVE );
		eventDistributor.unregisterEventListener(
			KEY_UP_EVENT,   console, EventDistributor::NATIVE );
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

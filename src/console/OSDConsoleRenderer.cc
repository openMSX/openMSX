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
	OSDConsoleRenderer& console_, const string& settingName,
	const string& filename, XMLElement* node)
	: FilenameSettingBase(settingName, "console background file", "", node)
	, console(console_)
{
	setValueString(filename);
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
	OSDConsoleRenderer& console_, const string& settingName,
	const string& filename, XMLElement* node)
	: FilenameSettingBase(settingName, "console font file", "", node)
	, console(console_)
{
	setValueString(filename);
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
	XMLElement& config = SettingsConfig::instance().getCreateChild("console");
	FileContext& context = config.getFileContext();

	// font
	XMLElement& fontElem = config.getCreateChild("font", "");
	string fontName;
	try {
		fontName = context.resolve(fontElem.getData());
	} catch (FileException& e) {
		// nothing
	}
	fontSetting.reset(new FontSetting(*this, "consolefont", fontName, &fontElem));
	
	// rows / columns
	SDL_Surface* screen = SDL_GetVideoSurface();
	int columns = (((screen->w - CHAR_BORDER) / font->getWidth()) * 30) / 32;
	int rows = ((screen->h / font->getHeight()) * 6) / 15;
	XMLElement& columnsElem = config.getCreateChild("columns", columns);
	XMLElement& rowsElem    = config.getCreateChild("rows",    rows);
	console.setColumns(columnsElem.getDataAsInt());
	console.setRows(rowsElem.getDataAsInt());
	adjustColRow();
	console.setConsoleDimensions(consoleColumns, consoleRows);
	consoleColumnsSetting.reset(new IntegerSetting(
		"consolecolumns", "number of columns in the console",
		console.getColumns(), 32, 999, &columnsElem));
	consoleRowsSetting.reset(new IntegerSetting(
		"consolerows", "number of rows in the console",
		console.getRows(), 1, 99, &rowsElem));
	
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
	XMLElement& placeElem   = config.getCreateChild("placement", "bottom");
	Placement placement = CP_BOTTOM;
	PlaceMap::const_iterator it = placeMap.find(placeElem.getData());
	if (it != placeMap.end()) {
		placement = it->second;
	}
	consolePlacementSetting.reset(new EnumSetting<Placement>(
		"consoleplacement", "position of the console within the emulator",
		placement, placeMap, &placeElem));
	
	updateConsoleRect(destRect);
	
	// background
	XMLElement& backgroundElem = config.getCreateChild("background", "");
	string backgroundName;
	try {
		backgroundName = context.resolve(backgroundElem.getData());
	} catch (FileException& e) {
		// nothing
	}
	backgroundSetting.reset(
		new BackgroundSetting(*this, "consolebackground",
		                      backgroundName, &backgroundElem));
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
	// TODO: Even if console alpha is 255, it doesn't cover the full screen,
	//       so underlying layers still need to be painted.
	//       Display interface design flaw?
	Display::INSTANCE->setAlpha(this, active ? 128 : 0);
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

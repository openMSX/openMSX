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
	OSDConsoleRenderer* console_, const string& settingName,
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
	bool ok = console->loadBackground(filename);
	if (ok) {
		console->setBackgroundName(filename);
	}
	return ok;
}


// class FontSetting

FontSetting::FontSetting(
	OSDConsoleRenderer* console_, const string& settingName,
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
	bool ok = console->loadFont(filename);
	if (ok) {
		console->setFontName(filename);
	}
	return ok;
}


// class OSDConsoleRenderer

BooleanSetting OSDConsoleRenderer::consoleSetting(
	"console", "turns console display on/off", false);

OSDConsoleRenderer::OSDConsoleRenderer(Console& console_)
	: console(console_)
	, eventDistributor(EventDistributor::instance())
	, inputEventGenerator(InputEventGenerator::instance())
{
	string tempconfig = console.getId();
	
	XMLElement& config = SettingsConfig::instance().getCreateChild(tempconfig);
	FileContext& context = config.getFileContext();

	XMLElement& fontElem = config.getCreateChild("font", console.getFont());
	try {
		string fontName = fontElem.getData();
		fontName = context.resolve(fontName);
		console.setFont(fontName);
	} catch (FileException& e) {
		// nothing
	}

	XMLElement& backgroundElem = config.getCreateChild("background", console.getBackground());
	try {
		string backgroundName = backgroundElem.getData();
		backgroundName = context.resolve(backgroundName);
		console.setBackground(backgroundName);
	} catch (FileException& e) {
		// nothing
	}

	font.reset(new DummyFont());
	if (!console.getFont().empty()) {
		console.registerConsole(this);
	}
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
	if (!console.getFont().empty()) {
		console.unregisterConsole(this);
	}
}

void OSDConsoleRenderer::setBackgroundName(const string& name)
{
	console.setBackground(name);
}

void OSDConsoleRenderer::setFontName(const string& name)
{
	console.setFont(name);
}

void OSDConsoleRenderer::initConsoleSize()
{
	// define all possible positions
	typedef EnumSetting<Console::Placement>::Map PlaceMap;
	PlaceMap placeMap;
	placeMap["topleft"]     = Console::CP_TOPLEFT;
	placeMap["top"]         = Console::CP_TOP;
	placeMap["topright"]    = Console::CP_TOPRIGHT;
	placeMap["left"]        = Console::CP_LEFT;
	placeMap["center"]      = Console::CP_CENTER;
	placeMap["right"]       = Console::CP_RIGHT;
	placeMap["bottomleft"]  = Console::CP_BOTTOMLEFT;
	placeMap["bottom"]      = Console::CP_BOTTOM;
	placeMap["bottomright"] = Console::CP_BOTTOMRIGHT;

	string tempconfig = console.getId();

	SDL_Surface* screen = SDL_GetVideoSurface();
	int columns = (((screen->w - CHAR_BORDER) / font->getWidth()) * 30) / 32;
	int rows = ((screen->h / font->getHeight()) * 6) / 15;
	string placementString = "bottom";

	SettingsConfig& settings = SettingsConfig::instance();
	XMLElement& config = settings.getCreateChild(tempconfig);
	XMLElement& columnsElem = config.getCreateChild("columns", columns);
	XMLElement& rowsElem    = config.getCreateChild("rows",    rows);
	XMLElement& placeElem   = config.getCreateChild("placement", placementString);
	
	columns = columnsElem.getDataAsInt();
	rows    = rowsElem.getDataAsInt();
	placementString = placeElem.getData();
	
	console.setColumns(columns);
	console.setRows(rows);
	
	PlaceMap::const_iterator it = placeMap.find(placementString);
	Console::Placement consolePlacement;
	if (it != placeMap.end()) {
		consolePlacement = it->second;
	} else {
		consolePlacement = Console::CP_BOTTOM; //not found, default
	}
	console.setPlacement(consolePlacement);
	
	adjustColRow();
	console.setConsoleDimensions(consoleColumns, consoleRows);
	string tempname = console.getId();
	consoleColumnsSetting.reset(new IntegerSetting(
		tempname + "columns", "number of columns in the console",
		console.getColumns(), 32, 999, &columnsElem));
	consoleRowsSetting.reset(new IntegerSetting(
		tempname + "rows", "number of rows in the console",
		console.getRows(), 1, 99, &rowsElem));
	consolePlacementSetting.reset(new EnumSetting<Console::Placement>(
		tempname + "placement", "position of the console within the emulator",
		console.getPlacement(), placeMap, &placeElem));
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
	console.setPlacement(consolePlacementSetting->getValue());
	switch (console.getPlacement()) {
		case Console::CP_TOPLEFT:
		case Console::CP_LEFT:
		case Console::CP_BOTTOMLEFT:
			rect.x = 0;
			break;
		case Console::CP_TOPRIGHT:
		case Console::CP_RIGHT:
		case Console::CP_BOTTOMRIGHT:
			rect.x = (screen->w - rect.w);
			break;
		case Console::CP_TOP:
		case Console::CP_CENTER:
		case Console::CP_BOTTOM:
		default:
			rect.x = (screen->w - rect.w) / 2;
			break;
	}
	switch (console.getPlacement()) {
		case Console::CP_TOPLEFT:
		case Console::CP_TOP:
		case Console::CP_TOPRIGHT:
			rect.y = 0;
			break;
		case Console::CP_LEFT:
		case Console::CP_CENTER:
		case Console::CP_RIGHT:
			rect.y = (screen->h - rect.h) / 2;
			break;
		case Console::CP_BOTTOMLEFT:
		case Console::CP_BOTTOM:
		case Console::CP_BOTTOMRIGHT:
		default:
			rect.y = (screen->h - rect.h);
			break;
	}
}

} // namespace openmsx

// $Id$

#include <algorithm>
#include "OSDConsoleRenderer.hh"
#include "MSXConfig.hh"
#include "Config.hh"
#include "CommandConsole.hh"
#include "File.hh"

namespace openmsx {


// class BackgroundSetting

BackgroundSetting::BackgroundSetting(OSDConsoleRenderer *console_, const string settingName,
                                     const string &filename)
	: FilenameSetting(settingName, "console background file", ""),
	  console(console_)
{
	setValueString(filename);
}

bool BackgroundSetting::checkFile(const string &filename)
{
	bool ok = console->loadBackground(filename);
	if (ok) {
		console->setBackgroundName(filename);
	}
	return ok;
}


// class FontSetting

FontSetting::FontSetting(OSDConsoleRenderer *console_, const string settingName,
                         const string &filename)
	: FilenameSetting(settingName, "console font file", ""),
	  console(console_)
{
	setValueString(filename);
}

bool FontSetting::checkFile(const string &filename)
{
	bool ok = console->loadFont(filename);
	if (ok) {
		console->setFontName(filename);
	}
	return ok;
}


// class OSDConsoleRenderer

OSDConsoleRenderer::OSDConsoleRenderer(Console& console_)
	: console(console_)
{
	bool initiated = true;
	static set<string> initsDone;
	string tempconfig = console.getId();
	tempconfig[0] = ::toupper(tempconfig[0]);
	try {
		Config* config = MSXConfig::instance().getConfigById(tempconfig);
		context = config->getContext().clone();
		if (initsDone.find(tempconfig) == initsDone.end()) {
			initsDone.insert(tempconfig);
			initiated = false;
		}
		try {
			if (!initiated && config->hasParameter("font")) {
				string fontName = config->getParameter("font");
				fontName = context->resolve(fontName);
				console.setFont(fontName);
			}
		} catch (FileException &e) {
			// nothing
		}

		try {
			if (!initiated && config->hasParameter("background")) {
				string backgroundName = config->getParameter("background");
				backgroundName = context->resolve(backgroundName);
				console.setBackground(backgroundName);
			}
		} catch (FileException &e) {
			// nothing
		}

	} catch (ConfigException &e) {
		// no Console section
		context = new SystemFileContext();
	}
	initiated = true;
	font = new DummyFont();
	if (!console.getFont().empty()) {
		console.registerConsole(this);
	}
	blink = false;
	lastBlinkTime = 0;
	unsigned cursorY;
	console.getCursorPosition(lastCursorPosition, cursorY);
	
	consolePlacementSetting = NULL;
	consoleRowsSetting = NULL;
	consoleColumnsSetting = NULL;
}

OSDConsoleRenderer::~OSDConsoleRenderer()
{
	if (!console.getFont().empty()) {
		console.unregisterConsole(this);
	}
	delete font;
	delete consolePlacementSetting;
 	delete consoleRowsSetting;
	delete consoleColumnsSetting;
	delete context;
}

void OSDConsoleRenderer::setBackgroundName(const string &name)
{
	console.setBackground(name);
}

void OSDConsoleRenderer::setFontName(const string &name)
{
	console.setFont(name);
}

void OSDConsoleRenderer::initConsoleSize()
{
	static set<string> initsDone;
	
	// define all possible positions
	map<string, Console::Placement> placeMap;
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
	tempconfig[0]=::toupper(tempconfig[0]);
	Config *config = MSXConfig::instance().getConfigById(tempconfig);
	// check if this console is already initiated
	if (initsDone.find(tempconfig) == initsDone.end()) {
		initsDone.insert(tempconfig);
		SDL_Surface *screen = SDL_GetVideoSurface();
		console.setColumns(config->getParameterAsInt("columns",
			(((screen->w - CHAR_BORDER) / font->getWidth()) * 30) / 32));
		console.setRows(config->getParameterAsInt("rows", 
			((screen->h / font->getHeight()) * 6) / 15));
		
		string placementString;
		placementString = config->getParameter("placement", "bottom");
		map<string, Console::Placement>::const_iterator it;
		it = placeMap.find(placementString);
		Console::Placement consolePlacement;
		if (it != placeMap.end()) {
			consolePlacement = it->second;
		} else {
			consolePlacement = Console::CP_BOTTOM; //not found, default
		}
		console.setPlacement(consolePlacement);
	}
	adjustColRow();
	console.setConsoleDimensions(consoleColumns, consoleRows);
	string tempname = console.getId();
	consoleColumnsSetting = new IntegerSetting(
		tempname + "columns", "number of columns in the console",
		console.getColumns(), 32, 999
		);
	consoleRowsSetting = new IntegerSetting(
		tempname + "rows", "number of rows in the console",
		console.getRows(), 1, 99
		);
	consolePlacementSetting = new EnumSetting<Console::Placement>(
		tempname + "placement", "position of the console within the emulator",
		console.getPlacement(), placeMap
		);
}

void OSDConsoleRenderer::adjustColRow()
{
	SDL_Surface *screen = SDL_GetVideoSurface();
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

void OSDConsoleRenderer::updateConsoleRect(SDL_Rect & rect)
{
	SDL_Surface *screen = SDL_GetVideoSurface();
	
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

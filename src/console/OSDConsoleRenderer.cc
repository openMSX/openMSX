// $Id$

#include <algorithm>
#include "OSDConsoleRenderer.hh"
#include "MSXConfig.hh"
#include "CommandConsole.hh"
#include "File.hh"

int OSDConsoleRenderer::wantedColumns;
int OSDConsoleRenderer::wantedRows;
string OSDConsoleRenderer::fontName;
string OSDConsoleRenderer::backgroundName;
OSDConsoleRenderer::Placement OSDConsoleRenderer::consolePlacement;


// class BackgroundSetting

BackgroundSetting::BackgroundSetting(OSDConsoleRenderer *console_, const std::string settingName,
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

FontSetting::FontSetting(OSDConsoleRenderer *console_, const std::string settingName,
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

OSDConsoleRenderer::OSDConsoleRenderer(Console * console_)
{
	console = console_;
	bool initiated = true;
	static set<string> initsDone;
	string tempconfig = console->getId();
	tempconfig[0] = ::toupper (tempconfig[0]);
	try {
		Config *config = MSXConfig::instance()->getConfigById(tempconfig);
		context = config->getContext();
		if (initsDone.find(tempconfig)==initsDone.end()){
			initsDone.insert(tempconfig);
			initiated = false;
		}
		try {
			if (!initiated && config->hasParameter("font")) {
				fontName = config->getParameter("font");
				fontName = context->resolve(fontName);
			}
		} catch (FileException &e) {}

		try {
			if (!initiated && config->hasParameter("background")) {
				backgroundName = config->getParameter("background");
				backgroundName = context->resolve(backgroundName);
			}
		} catch (FileException &e) {}

	} catch (MSXException &e) {
		// no Console section
		context = new SystemFileContext();	// TODO memory leak
	}
	initiated = true;
	font = new DummyFont();
	if (!fontName.empty()) {
		console->registerConsole(this);
	}
	blink = false;
	lastBlinkTime = 0;
	int cursorY;
	console->getCursorPosition(&lastCursorPosition, &cursorY);
	
	consolePlacementSetting = NULL;
	consoleRowsSetting = NULL;
	consoleColumnsSetting = NULL;
}

OSDConsoleRenderer::~OSDConsoleRenderer()
{
	if (!fontName.empty()) {
		console->unregisterConsole(this);
	}
	delete font;
	delete consolePlacementSetting;
 	delete consoleRowsSetting;
	delete consoleColumnsSetting;
}

void OSDConsoleRenderer::setBackgroundName(const string &name)
{
	backgroundName = name;
}

void OSDConsoleRenderer::setFontName(const string &name)
{
	fontName = name;
}

void OSDConsoleRenderer::initConsoleSize()
{
	static set<string> initsDone;
	
	// define all possible positions
	map<string, Placement> placeMap;
	placeMap["topleft"]     = CP_TOPLEFT;
	placeMap["top"]         = CP_TOP;
	placeMap["topright"]    = CP_TOPRIGHT;
	placeMap["left"]        = CP_LEFT;
	placeMap["center"]      = CP_CENTER;
	placeMap["right"]       = CP_RIGHT;
	placeMap["bottomleft"]  = CP_BOTTOMLEFT;
	placeMap["bottom"]      = CP_BOTTOM;
	placeMap["bottomright"] = CP_BOTTOMRIGHT;

	string tempconfig = console->getId();
	tempconfig[0]=::toupper(tempconfig[0]);
	Config *config = MSXConfig::instance()->getConfigById(tempconfig);
	// check if this console is allready initiated
	if (initsDone.find(tempconfig)==initsDone.end()){
		initsDone.insert(tempconfig);
		SDL_Surface *screen = SDL_GetVideoSurface();
		wantedColumns = config->hasParameter("columns") ?
		                config->getParameterAsInt("columns") :
		                (((screen->w - CHAR_BORDER) / font->getWidth()) * 30) / 32;
		wantedRows = config->hasParameter("rows") ?
		             config->getParameterAsInt("rows") :
		             ((screen->h / font->getHeight()) * 6) / 15;
		string placementString;
		placementString = config->hasParameter("placement") ?
		                  config->getParameter("placement") :
		                  "bottom";
		map<string, Placement>::const_iterator it;
		it = placeMap.find(placementString);
		if (it != placeMap.end()) {
			consolePlacement = it->second;
		} else {
			consolePlacement = CP_BOTTOM; //not found, default
		}
	}
	adjustColRow();
	console->setConsoleDimensions(consoleColumns, consoleRows);
	string tempname = console->getId();
	consoleColumnsSetting = new IntegerSetting(
		tempname + "columns", "number of columns in the console",
		wantedColumns, 32, 999
		);
	consoleRowsSetting = new IntegerSetting(
		tempname + "rows", "number of rows in the console",
		wantedRows, 1, 99
		);
	consolePlacementSetting = new EnumSetting<OSDConsoleRenderer::Placement>(
		tempname + "placement", "position of the console within the emulator",
		consolePlacement, placeMap
		);
}

void OSDConsoleRenderer::adjustColRow()
{
	SDL_Surface *screen = SDL_GetVideoSurface();
	if (wantedColumns > ((screen->w - CHAR_BORDER) / font->getWidth())) {
		consoleColumns = (screen->w - CHAR_BORDER) / font->getWidth();
	} else {
		consoleColumns = wantedColumns;
	}
	if (wantedRows > (screen->h / font->getHeight())) {
		consoleRows = screen->h / font->getHeight();
	} else {
		consoleRows = wantedRows;
	}
}

void OSDConsoleRenderer::updateConsoleRect(SDL_Rect & rect)
{
	SDL_Surface *screen = SDL_GetVideoSurface();
	
	// TODO use setting listener in the future
	wantedRows = consoleRowsSetting->getValue();
	wantedColumns = consoleColumnsSetting->getValue();
	adjustColRow();

	rect.h = font->getHeight() * consoleRows;
	rect.w = (font->getWidth() * consoleColumns) + CHAR_BORDER;
	console->setConsoleDimensions(consoleColumns, consoleRows);
	
	// TODO use setting listener in the future
	consolePlacement = consolePlacementSetting->getValue();
	switch (consolePlacement) {
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
	switch (consolePlacement) {
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

// $Id$

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

BackgroundSetting::BackgroundSetting(OSDConsoleRenderer *console_,
                                     const string &filename)
	: FilenameSetting("consolebackground", "console background file", ""),
	  console(console_)
{
	setValueString(filename);
}

void BackgroundSetting::setValue(const string &newValue)
{
	string resolved;
	try {
		UserFileContext context;
		resolved = context.resolve(newValue);
	} catch (FileException &e) {
		// file not found
		PRT_INFO("Warning: Couldn't read background image: \"" <<
		         newValue << "\"");
		return;
	}
	bool ok = console->loadBackground(resolved);
	if (ok) {
		console->setBackgroundName(resolved);
		FilenameSetting::setValue(newValue);
	}
}


// class FontSetting

FontSetting::FontSetting(OSDConsoleRenderer *console_,
                         const string &filename)
	: FilenameSetting("consolefont", "console font file", ""),
	  console(console_)
{
	setValueString(filename);
}

void FontSetting::setValue(const string &newValue)
{
	string resolved;
	try {
		UserFileContext context;
		resolved = context.resolve(newValue);
	} catch (FileException &e) {
		// file not found
		PRT_INFO("Warning: Couldn't read font image: \"" <<
		         newValue << "\"");
		return;
	}
	bool ok = console->loadFont(resolved);
	if (ok) {
		console->setFontName(resolved);
		FilenameSetting::setValue(newValue);
	}
}


// class OSDConsoleRenderer

OSDConsoleRenderer::OSDConsoleRenderer(Console * console_)
{
	console = console_;
	static bool initiated = false;
	try {
		Config *config = MSXConfig::instance()->getConfigById("Console");
		context = config->getContext();

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

		initiated = true;
		font = new DummyFont();
	} catch (MSXException &e) {
		// no Console section
		context = new SystemFileContext();	// TODO memory leak
	}

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
	static bool placementInitDone = false;

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

	Config *config = MSXConfig::instance()->getConfigById("Console");

	if (!placementInitDone) {
		// first time here ?
		placementInitDone = true;

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

	consoleColumnsSetting = new IntegerSetting(
		"consolecolumns", "number of columns in the console",
		wantedColumns, 32, 999
		);
	consoleRowsSetting = new IntegerSetting(
		"consolerows", "number of rows in the console",
		wantedRows, 1, 99
		);
	consolePlacementSetting = new EnumSetting<OSDConsoleRenderer::Placement>(
		"consoleplacement", "position of the console within the emulator",
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

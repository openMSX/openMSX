// $Id$

#include "OSDConsoleRenderer.hh"
#include "MSXConfig.hh"
#include "Console.hh"
#include "File.hh"

int OSDConsoleRenderer::consoleColumns;
int OSDConsoleRenderer::consoleLines;
OSDConsoleRenderer::Placement OSDConsoleRenderer::consolePlacement;

// class BackgroundSetting

BackgroundSetting::BackgroundSetting(OSDConsoleRenderer *console_,
                                     const std::string &filename)
	: FilenameSetting("consolebackground", "console background file",
	                  filename),
	  console(console_)
{
	setValueString(filename);
}

bool BackgroundSetting::checkUpdate(const std::string &newValue)
{
	bool result;
	try {
		UserFileContext context;
		result = console->loadBackground(context.resolve(newValue));
	} catch (FileException &e) {
		// file not found
		result = false;
	}
	return result;
}

// class FontSetting

FontSetting::FontSetting(OSDConsoleRenderer *console_,
                         const std::string &filename)
	: FilenameSetting("consolefont", "console font file", filename),
	  console(console_)
{
	setValueString(filename);
}

bool FontSetting::checkUpdate(const std::string &newValue)
{
	bool result;
	try {
		UserFileContext context;
		result = console->loadFont(context.resolve(newValue));
	} catch (FileException &e) {
		// file not found
		result = false;
	}
	return result;
}


// class OSDConsoleRenderer

OSDConsoleRenderer::OSDConsoleRenderer()
{
	try {
		Config *config = MSXConfig::instance()->getConfigById("Console");
		context = config->getContext();
		
		try
		{			
			if (config->hasParameter("font")) {
				fontName = config->getParameter("font");
				fontName = context->resolve(fontName);
			}
		}catch(FileException &e) {}
		
		try
		{
			if (config->hasParameter("background")) {
				backgroundName = config->getParameter("background");
				backgroundName = context->resolve(backgroundName);
			}
		}catch(FileException &e) {}
	
		font = new DummyFont();
				
	} catch (MSXException &e) {
		// no Console section
		context = new SystemFileContext();	// TODO memory leak
	}

	if (!fontName.empty()) {
		Console::instance()->registerConsole(this);
	}
	blink = false;
	lastBlinkTime = 0;
	lastCursorPosition=Console::instance()->getCursorPosition();
	
	consolePlacementSetting=NULL;
	consoleLinesSetting=NULL;
	consoleColumnsSetting=NULL;
}

OSDConsoleRenderer::~OSDConsoleRenderer()
{
	if (!fontName.empty()) {
		Console::instance()->unregisterConsole(this);
	}
	delete font;
	if (consolePlacementSetting) delete consolePlacementSetting;
 	if (consoleLinesSetting) delete consoleLinesSetting;
	if (consoleColumnsSetting) delete consoleColumnsSetting;
}

void OSDConsoleRenderer::initConsoleSize()
{
	static bool placementInitDone=false;
	
	// define all possible positions
	placeMap["topleft"]		= CP_TOPLEFT;
	placeMap["top"]			= CP_TOP;
	placeMap["topright"]	= CP_TOPRIGHT;
	placeMap["left"] 		= CP_LEFT;
	placeMap["center"] 		= CP_CENTER;
	placeMap["right"] 		= CP_RIGHT;
	placeMap["bottomleft"] 	= CP_BOTTOMLEFT;
	placeMap["bottom"] 		= CP_BOTTOM;
	placeMap["bottomright"]	= CP_BOTTOMRIGHT;
	
	SDL_Surface *screen = SDL_GetVideoSurface();
	
	Config *config = MSXConfig::instance()->getConfigById("Console");
	
	if (!placementInitDone) // first time here ?
		{	
			std::string placementString;
			consoleLines = (config->hasParameter("height") ? config->getParameterAsInt("height") : 0);
			consoleColumns = (config->hasParameter("width") ? config->getParameterAsInt("width") : 0);
			// check if lines and columns of the console are valid
			if ((consoleLines<1) || (consoleLines > (screen->h / font->getHeight())))
				consoleLines=((screen->h / font->getHeight())*6)/15;
			if ((consoleColumns<32) || (consoleColumns > ((screen->w - CHAR_BORDER) / font->getWidth())))
				consoleColumns=((screen->w / font->getWidth())*30)/32; 
			placementString = (config->hasParameter("placement") ? config->getParameter("placement") : "bottom");
			
			std::map<std::string, Placement>::const_iterator it;
			it = placeMap.find(placementString);
			if (it != placeMap.end()) 
				consolePlacement=it->second;
			else
				consolePlacement=CP_BOTTOM; //not found, default
			placementInitDone=true;
		}	
		Console::instance()->setConsoleColumns(consoleColumns);
			
		consoleLinesSetting = new IntegerSetting("consolelines","number of lines in the console",consoleLines,1,screen->h / font->getHeight());
		consoleColumnsSetting = new IntegerSetting("consolecolumns","number of columns in the console",consoleColumns,32,(screen->w - CHAR_BORDER) / font->getWidth());
		consolePlacementSetting = new EnumSetting<OSDConsoleRenderer::Placement>(
			"consoleplacement", "position of the console within the emulator", consolePlacement, placeMap);
}



void OSDConsoleRenderer::updateConsoleRect(SDL_Rect & rect)
{
	SDL_Surface *screen = SDL_GetVideoSurface();
		
	consoleLinesSetting->setRange(1,screen->h / font->getHeight());
	consoleLines = consoleLinesSetting->getValue();
	rect.h = font->getHeight() * consoleLines;
		
	consoleColumnsSetting->setRange(32,(screen->w - CHAR_BORDER) / font->getWidth());
	consoleColumns= consoleColumnsSetting->getValue();
	rect.w = (font->getWidth() * consoleColumns) + CHAR_BORDER;
	
	Console::instance()->setConsoleColumns(consoleColumns);
	
	consolePlacement=consolePlacementSetting->getValue();
	
	switch(consolePlacement)
		{
		case CP_TOPLEFT:
		case CP_LEFT:
		case CP_BOTTOMLEFT:
			rect.x = 0;
			break;
		case CP_TOPRIGHT:
		case CP_RIGHT:
		case CP_BOTTOMRIGHT:
			rect.x = (screen->w - rect.w) ;
			break;
		case CP_TOP:
		case CP_CENTER:
		case CP_BOTTOM:
		default:
			rect.x = (screen->w - rect.w) /2;
			break;
		}
		
		switch(consolePlacement)
		{
		case CP_TOPLEFT:
		case CP_TOP:
		case CP_TOPRIGHT:
			rect.y = 0;
			break;
		case CP_LEFT:
		case CP_CENTER:
		case CP_RIGHT:
			rect.y = (screen->h - rect.h) /2;
			break;
		case CP_BOTTOMLEFT:
		case CP_BOTTOM:
		case CP_BOTTOMRIGHT:
		default:
			rect.y = (screen->h - rect.h) ;
			break;
		}
}

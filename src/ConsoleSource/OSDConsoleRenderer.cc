// $Id$

#include "OSDConsoleRenderer.hh"
#include "MSXConfig.hh"
#include "Console.hh"
#include "File.hh"


// class BackgroundSetting

BackgroundSetting::BackgroundSetting(OSDConsoleRenderer *console_,
                                     const std::string &filename)
	: FilenameSetting("console_background", "console background file",
	                  filename),
	  console(console_)
{
	EmuTime dummy;
	setValueString(filename, dummy);
}

bool BackgroundSetting::checkUpdate(const std::string &newValue,
                                    const EmuTime &time)
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
	: FilenameSetting("console_font", "console font file", filename),
	  console(console_)
{
	EmuTime dummy;
	setValueString(filename, dummy);
}

bool FontSetting::checkUpdate(const std::string &newValue, const EmuTime &time)
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
		if (config->hasParameter("font")) {
			fontName = config->getParameter("font");
			fontName = context->resolve(fontName);
		}
		if (config->hasParameter("background")) {
			backgroundName = config->getParameter("background");
			backgroundName = context->resolve(backgroundName);
		}
	} catch (MSXException &e) {
		// no Console section
		context = new SystemFileContext();	// TODO memory leak
	}

	if (!fontName.empty()) {
		Console::instance()->registerConsole(this);
	}
}

OSDConsoleRenderer::~OSDConsoleRenderer()
{
	if (!fontName.empty()) {
		Console::instance()->unregisterConsole(this);
	}
}

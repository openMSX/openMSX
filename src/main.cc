// $Id$

/*
 *  openmsx - the MSX emulator that aims for perfection
 *
 */

#include <memory> // for auto_ptr
#include <iostream>
#include <SDL/SDL.h>
#include <exception>
#include "config.h"
#include "MSXMotherBoard.hh"
#include "CommandLineParser.hh"
#include "CliCommInput.hh"
#include "HotKey.hh"
#include "AfterCommand.hh"
#include "AliasCommands.hh"

using std::auto_ptr;
using std::cerr;
using std::endl;

namespace openmsx {

static void initializeSDL()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) { 
		throw FatalError(string("Couldn't init SDL: ") + SDL_GetError());
	}
}

static void unexpectedExceptionHandler()
{
	cerr << "Unexpected exception." << endl;
}

static int main(int argc, char **argv)
{
	std::set_unexpected(unexpectedExceptionHandler);
	
	int err = 0;
	try {
		initializeSDL();
		CommandLineParser& parser = CommandLineParser::instance();
		parser.parse(argc, argv);
		CommandLineParser::ParseStatus parseStatus = parser.getParseStatus();
		if (parseStatus != CommandLineParser::EXIT) {
			auto_ptr<CliCommInput> cliCommInput;
			if (parseStatus) {
				CommandLineParser::ControlType type;
				string argument;
				parser.getControlParameters (& type, argument);				
				cliCommInput.reset(new CliCommInput(type, argument));
			}
			HotKey hotkey;
			AfterCommand afterCommand;
			AliasCommands aliasCommands;
			MSXMotherBoard motherboard;
			motherboard.run(parseStatus == CommandLineParser::RUN);
		}
	} catch (FatalError& e) {
		cerr << "Fatal error: " << e.getMessage() << endl;
		err = 1;
	} catch (MSXException& e) {
		cerr << "Uncaught exception: " << e.getMessage() << endl;
		err = 1;
	} catch (...) {
		cerr << "Uncaught exception of unexpected type." << endl;
		err = 1;
	}
	// Clean up.
	if (SDL_WasInit(SDL_INIT_EVERYTHING)) {
		SDL_Quit();
	}
	return err;
}

} // namespace openmsx

// Enter the openMSX namespace.
int main(int argc, char **argv)
{
	return openmsx::main(argc, argv);
}

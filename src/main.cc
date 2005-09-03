// $Id$

/*
 *  openmsx - the MSX emulator that aims for perfection
 *
 */

#include <memory>
#include <iostream>
#include <exception>
#include <SDL.h>
#include "Reactor.hh"
#include "MSXMotherBoard.hh"
#include "CommandLineParser.hh"
#include "CartridgeSlotManager.hh"
#include "CliComm.hh"
#include "CliServer.hh"
#include "AfterCommand.hh"
#include "CommandController.hh"
#include "Interpreter.hh"
#include "Display.hh"
#include "RendererFactory.hh"
#include "MSXException.hh"
#include "HotKey.hh"
#include "SettingsConfig.hh"
#include "CommandConsole.hh"

using std::auto_ptr;
using std::cerr;
using std::endl;
using std::string;

namespace openmsx {

static void initializeSDL()
{
	int flags = SDL_INIT_TIMER;
#ifndef NDEBUG
	flags |= SDL_INIT_NOPARACHUTE;
#endif
	if (SDL_Init(flags) < 0) {
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
		MSXMotherBoard motherBoard;
		motherBoard.getCommandController().getInterpreter().init(argv[0]);

		// TODO cleanup once singleton mess is cleaned up
		HotKey hotKey(motherBoard.getCommandController(),
		              motherBoard.getUserInputEventDistributor());
		motherBoard.getCommandController().getSettingsConfig().setHotKey(&hotKey);

		CommandLineParser parser(motherBoard);
		parser.parse(argc, argv);
		CommandLineParser::ParseStatus parseStatus = parser.getParseStatus();
		if (parseStatus != CommandLineParser::EXIT) {
			initializeSDL();
			AfterCommand afterCommand(
				motherBoard.getScheduler(),
				motherBoard.getEventDistributor(),
				motherBoard.getCommandController());                  
			RendererFactory::createVideoSystem(motherBoard);
			motherBoard.readConfig();
			Reactor reactor(motherBoard);
			// TODO disabled for security reasons
			//CliServer cliServer(motherBoard.getScheduler(),
			//                    motherBoard.getCommandController()); // disabled for security reasons
			reactor.run(parseStatus == CommandLineParser::RUN);
		}

	} catch (FatalError& e) {
		cerr << "Fatal error: " << e.getMessage() << endl;
		err = 1;
	} catch (MSXException& e) {
		cerr << "Uncaught exception: " << e.getMessage() << endl;
		err = 1;
	} catch (std::exception& e) {
		cerr << "Uncaught std::exception: " << e.what() << endl;
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

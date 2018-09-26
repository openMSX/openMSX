/*
 *  openmsx - the MSX emulator that aims for perfection
 *
 */

#include "openmsx.hh"
#include "Date.hh"
#include "Reactor.hh"
#include "CommandLineParser.hh"
#include "CliServer.hh"
#include "Display.hh"
#include "EventDistributor.hh"
#include "RenderSettings.hh"
#include "EnumSetting.hh"
#include "MSXException.hh"
#include "Thread.hh"
#include "build-info.hh"
#include "random.hh"
#include <iostream>
#include <exception>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <SDL.h>
#ifdef _WIN32
#include "win32-arggen.hh"
#endif

// Set LOG_TO_FILE to 1 for any platform on which stdout and stderr must
// be redirected to a file
// Also, specify the appropriate file names, depending on the platform conventions
#if PLATFORM_ANDROID
#define LOG_TO_FILE 1
#define STDOUT_LOG_FILE_NAME "openmsx_system/openmsx.stdout"
#define STDERR_LOG_FILE_NAME "openmsx_system/openmsx.stderr"
#else
#define LOG_TO_FILE 0
#endif

namespace openmsx {

static void initializeSDL()
{
	int flags = 0;
#ifndef SDL_JOYSTICK_DISABLED
	flags |= SDL_INIT_JOYSTICK;
#endif
#ifndef NDEBUG
	flags |= SDL_INIT_NOPARACHUTE;
#endif
	if (SDL_Init(flags) < 0) {
		throw FatalError("Couldn't init SDL: ", SDL_GetError());
	}

// In SDL 1.2.9 and before SDL_putenv has different semantics and is not
// guaranteed to exist on all platforms.
#if SDL_VERSION_ATLEAST(1, 2, 10)
	// On Mac OS X, send key combos like Cmd+H and Cmd+M to Cocoa, so it can
	// perform the corresponding actions.
#if defined(__APPLE__)
	SDL_putenv(const_cast<char*>("SDL_ENABLEAPPEVENTS=1"));
#endif
#endif
}

static int main(int argc, char **argv)
{
#if LOG_TO_FILE
	ad_printf("Redirecting stdout to %s and stderr to %s\n",
	          STDOUT_LOG_FILE_NAME, STDERR_LOG_FILE_NAME);

	if (!freopen(STDOUT_LOG_FILE_NAME, "a", stdout)) {
		ad_printf("Couldn't redirect stdout to logfile, aborting\n");
		std::cerr << "Couldn't redirect stdout to "
		             STDOUT_LOG_FILE_NAME "\n";
		exit(1);
	}
	if (!freopen(STDERR_LOG_FILE_NAME, "a", stderr)) {
		ad_printf("Couldn't redirect stderr to logfile, aborting\n");
		std::cout << "Couldn't redirect stderr to "
		             STDERR_LOG_FILE_NAME "\n";
		exit(1);
	}

	std::string msg = Date::toString(time(nullptr)) + ": starting openMSX";
	std::cout << msg << '\n';
	std::cerr << msg << '\n';
#endif

	int err = 0;
	try {
		// Constructing Reactor already causes parts of SDL to be used
		// and initialized. If we want to set environment variables
		// before this, we have to do it here...
		//
		// This is to make sure we get no annoying behaviour from SDL
		// with regards to CAPS lock. This only works in SDL 1.2.14 or
		// later, but it can't hurt to always set it (if we can rely on
		// SDL_putenv, so on 1.2.10+).
		//
		// On Mac OS X, Cocoa does not report CAPS lock release events.
		// The input driver inside SDL works around that by sending a
		// pressed;released combo when CAPS status changes. However,
		// because there is no time inbetween this does not give the
		// MSX BIOS a chance to see the CAPS key in a pressed state.

#if SDL_VERSION_ATLEAST(1, 2, 10)
		SDL_putenv(const_cast<char*>("SDL_DISABLE_LOCK_KEYS="
#if defined(__APPLE__)
			"0"
#else
			"1"
#endif
			));
#endif
		randomize(); // seed global random generator
		initializeSDL();

		Thread::setMainThread();
		Reactor reactor;
#ifdef _WIN32
		ArgumentGenerator arggen;
		argv = arggen.GetArguments(argc);
#endif
		CommandLineParser parser(reactor);
		parser.parse(argc, argv);
		CommandLineParser::ParseStatus parseStatus = parser.getParseStatus();

		if (parseStatus != CommandLineParser::EXIT) {
			if (!parser.isHiddenStartup()) {
				auto& render = reactor.getDisplay().getRenderSettings().getRendererSetting();
				render.setValue(render.getRestoreValue());
				// Switching renderer requires events, handle
				// these events before continuing with the rest
				// of initialization. This fixes a bug where
				// you have a '-script bla.tcl' command line
				// argument where bla.tcl contains a line like
				// 'ext gfx9000'.
				reactor.getEventDistributor().deliverEvents();
			}
			if (parseStatus != CommandLineParser::TEST) {
				CliServer cliServer(reactor.getCommandController(),
				                    reactor.getEventDistributor(),
				                    reactor.getGlobalCliComm());
				reactor.run(parser);
			}
		}
	} catch (FatalError& e) {
		std::cerr << "Fatal error: " << e.getMessage() << '\n';
		err = 1;
	} catch (MSXException& e) {
		std::cerr << "Uncaught exception: " << e.getMessage() << '\n';
		err = 1;
	} catch (std::exception& e) {
		std::cerr << "Uncaught std::exception: " << e.what() << '\n';
		err = 1;
	} catch (...) {
		std::cerr << "Uncaught exception of unexpected type." << '\n';
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
	exit(openmsx::main(argc, argv)); // need exit() iso return on win32/SDL
}

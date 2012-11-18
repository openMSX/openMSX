// $Id$

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
#include "StringOp.hh"
#include "Thread.hh"
#include "HostCPU.hh"
#ifdef _WIN32
#include "win32-arggen.hh"
#endif

// Set LOG_TO_FILE to 1 for any platform on which stdout and stderr must
// be redirected to a file
// Also, specify the appropriate file names, depending on the platform conventions
#ifdef ANDROID
#define LOG_TO_FILE 1
#define STDOUT_LOG_FILE_NAME "openmsx_system/openmsx.stdout"
#define STDERR_LOG_FILE_NAME "openmsx_system/openmsx.stderr"
#else
#define LOG_TO_FILE 0
#endif

#if LOG_TO_FILE
#include <ctime>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#endif

#include <memory>
#include <iostream>
#include <exception>
#include <cstdlib>
#include <SDL.h>

using std::cerr;
using std::cout;
using std::endl;
using std::string;

namespace openmsx {

static void initializeSDL()
{
	int flags = 0;
#ifndef NDEBUG
	flags |= SDL_INIT_NOPARACHUTE;
#endif
	if (SDL_Init(flags) < 0) {
		throw FatalError(StringOp::Builder() <<
			"Couldn't init SDL: " << SDL_GetError());
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

static void unexpectedExceptionHandler()
{
	cerr << "Unexpected exception." << endl;
}

static int main(int argc, char **argv)
{
#if LOG_TO_FILE
	ad_printf("Redirecting stdout to %s and stderr to %s\n", STDOUT_LOG_FILE_NAME, STDERR_LOG_FILE_NAME);

	int stdoutlogfile = open(STDOUT_LOG_FILE_NAME,O_WRONLY|O_APPEND|O_CREAT);
	close(1);
	dup2(stdoutlogfile, 1);
	close(stdoutlogfile);

	int stderrlogfile=open(STDERR_LOG_FILE_NAME,O_WRONLY|O_APPEND|O_CREAT);
	close(2);
	dup2(stderrlogfile, 2);
	close(stderrlogfile);

	char msg[255];
	snprintf(msg, sizeof(msg), "%s: starting openMSX", Date::toString(time(0)).c_str());
	cout << msg << endl;
	cerr << msg << endl;
#endif
	std::set_unexpected(unexpectedExceptionHandler);

	int err = 0;
	try {
		HostCPU::init(); // as early as possible

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
			initializeSDL();
			if (!parser.isHiddenStartup()) {
				reactor.getDisplay().getRenderSettings().
					getRenderer().restoreDefault();
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
	exit(openmsx::main(argc, argv)); // need exit() iso return on win32/SDL
}

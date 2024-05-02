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
#include "FileContext.hh"
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
static constexpr const char* STDOUT_LOG_FILE_NAME = "openmsx_system/openmsx.stdout";
static constexpr const char* STDERR_LOG_FILE_NAME = "openmsx_system/openmsx.stderr";
#else
#define LOG_TO_FILE 0
#endif

namespace openmsx {

#ifdef _WIN32
// wrapper for Windows, as the MS runtime doesn't provide setenv!?
static int setenv(const char* name, const char* value, int overwrite)
{
	if (!overwrite && getenv(name)) {
		return 0;
	}
	return _putenv_s(name, value);
}
#endif

#ifdef _WIN32
// enable console output on Windows
static void EnableConsoleOutput()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
}
#endif

static void initializeSDL()
{
	int flags = 0;
	flags |= SDL_INIT_JOYSTICK;
#ifndef NDEBUG
	flags |= SDL_INIT_NOPARACHUTE;
#endif
	if (SDL_Init(flags) < 0) {
		throw FatalError("Couldn't init SDL: ", SDL_GetError());
	}

	// for now: instruct FreeType to use the v35 TTF engine
	// this is the rendering we had before FreeType implemented and
	// switched over to the v40 engine. To keep this the same for now, we
	// select the old engine explicitly, until we decide how to continue
	// (e.g. just use v40 or use a font that renders better on v40
	setenv("FREETYPE_PROPERTIES", "truetype:interpreter-version=35", 0);
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
		return 1;
	}
	if (!freopen(STDERR_LOG_FILE_NAME, "a", stderr)) {
		ad_printf("Couldn't redirect stderr to logfile, aborting\n");
		std::cout << "Couldn't redirect stderr to "
		             STDERR_LOG_FILE_NAME "\n";
		return 1;
	}

	std::string msg = Date::toString(time(nullptr)) + ": starting openMSX";
	std::cout << msg << '\n';
	std::cerr << msg << '\n';
#endif

#ifdef _WIN32
    EnableConsoleOutput();
#endif

	try {
		randomize(); // seed global random generator
		initializeSDL();

		Thread::setMainThread();
		Reactor reactor;
#ifdef _WIN32
		(void)argc; (void)argv;
		ArgumentGenerator argGen;
		auto args = argGen.getArgs();
#else
		std::span<char*> args{argv, size_t(argc)};
#endif
		CommandLineParser parser(reactor);
		parser.parse(args);
		CommandLineParser::ParseStatus parseStatus = parser.getParseStatus();

		if (parseStatus != CommandLineParser::EXIT) {
			auto& display = reactor.getDisplay();
			if (!parser.isHiddenStartup()) {
				auto& render = display.getRenderSettings().getRendererSetting();
				render.setValue(render.getDefaultValue());
				// Switching renderer requires events, handle
				// these events before continuing with the rest
				// of initialization. This fixes a bug where
				// you have a '-script bla.tcl' command line
				// argument where bla.tcl contains a line like
				// 'ext gfx9000'.
				reactor.getEventDistributor().deliverEvents();
			}
			if (parseStatus != CommandLineParser::TEST) {
				display.repaint();

				CliServer cliServer(reactor.getCommandController(),
				                    reactor.getEventDistributor(),
				                    reactor.getGlobalCliComm());
				reactor.run(parser);
			}
		}
	} catch (FatalError& e) {
		std::cerr << "Fatal error: " << e.getMessage() << '\n';
		exitCode = 1;
	} catch (MSXException& e) {
		std::cerr << "Uncaught exception: " << e.getMessage() << '\n';
		exitCode = 1;
	} catch (std::exception& e) {
		std::cerr << "Uncaught std::exception: " << e.what() << '\n';
		exitCode = 1;
	} catch (...) {
		std::cerr << "Uncaught exception of unexpected type." << '\n';
		exitCode = 1;
	}
	// Clean up.
	if (SDL_WasInit(SDL_INIT_EVERYTHING)) {
		SDL_Quit();
	}

	return exitCode;
}

} // namespace openmsx

// Enter the openMSX namespace.
int main(int argc, char **argv)
{
	// TODO with SDL1 we had the comment:
	//   need exit() iso return on win32/SDL
	// Is that still the case with SDL2? Because for Android we need to
	// return from main instead of exit().
	return openmsx::main(argc, argv);
}

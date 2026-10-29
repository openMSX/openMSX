/*
 *  openmsx - the MSX emulator that aims for perfection
 *
 */

#include "CliServer.hh"
#include "CommandLineParser.hh"
#include "Display.hh"
#include "EnumSetting.hh"
#include "EventDistributor.hh"
#include "MSXException.hh"
#include "Reactor.hh"
#include "RenderSettings.hh"
#include "Thread.hh"

#include "one_of.hh"
#include "random.hh"
#include "setenv.hh" // setenv() for windows

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#ifdef _WIN32
#include "win32-arggen.hh"
#endif

namespace openmsx {

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
	// Deliberately NOT like SDL2: use SDL3's default keycode options. Set
	// them explicitly, so this doesn't change when SDL changes its default
	// (the SDL_KEYCODE_OPTIONS environment variable still overrides it). So
	// the letter keys of non-Latin layouts (e.g. Russian) report Latin keycodes,
	// and the number row of French layouts reports digits. With the
	// keycodes of the actual layout (like SDL2), everything based on
	// keycodes works worse on such keyboards: Ctrl+key on the MSX, the KEY
	// mapping mode, layout independent bindings and GUI shortcuts (e.g.
	// Ctrl+C). Text input, and thus typing on the MSX in CHARACTER mode, is
	// not affected. Only bindings on e.g. 'й' or '&' must now use 'Q' or '1'.
	SDL_SetHint(SDL_HINT_KEYCODE_OPTIONS, "french_numbers,latin_letters");
	// KEEP-SDL2: We always keep text input started (see
	// InputEventGenerator::startTextInput()). Without this hint, SDL3 then
	// shows an on-screen keyboard when there's no physical keyboard (e.g.
	// on a Steam Deck). SDL2 didn't do that.
	SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
	if (!SDL_Init(flags)) {
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
		auto parseStatus = parser.getParseStatus();

		if (parseStatus != one_of(CommandLineParser::Status::EXIT, CommandLineParser::Status::TEST)) {
			reactor.runStartupScripts(parser);

			auto& display = reactor.getDisplay();
			auto& render = display.getRenderSettings().getRendererSetting();
			if ((render.getEnum() == RenderSettings::RendererID::UNINITIALIZED) &&
			    (parseStatus != CommandLineParser::Status::CONTROL)) {
				render.setValue(render.getDefaultValue());
				// Switching renderer requires events, handle
				// these events before continuing with the rest
				// of initialization. This fixes a bug where
				// you have a '-script bla.tcl' command line
				// argument where bla.tcl contains a line like
				// 'ext gfx9000'.
				reactor.getEventDistributor().deliverEvents();
			}

			CliServer cliServer(reactor.getCommandController(),
			                    reactor.getEventDistributor(),
			                    reactor.getGlobalCliComm());

			if (parser.getParseStatus() == CommandLineParser::Status::RUN) {
				reactor.powerOn();
			}
			display.repaint();
			reactor.run();
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
	if (SDL_WasInit(0)) {
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

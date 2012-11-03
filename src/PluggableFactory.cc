// $Id$

#include "PluggableFactory.hh"
#include "PluggingController.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "Joystick.hh"
#include "JoyMega.hh"
#include "ArkanoidPad.hh"
#include "JoyTap.hh"
#include "NinjaTap.hh"
#include "SETetrisDongle.hh"
#include "MagicKey.hh"
#include "KeyJoystick.hh"
#include "MidiInReader.hh"
#include "MidiOutLogger.hh"
#include "Mouse.hh"
#include "Trackball.hh"
#include "PrinterPortLogger.hh"
#include "PrinterPortSimpl.hh"
#include "Printer.hh"
#include "RS232Tester.hh"
#include "WavAudioInput.hh"
#if	defined(_WIN32)
#include "MidiInWindows.hh"
#include "MidiOutWindows.hh"
#endif
#if defined(__APPLE__)
#include "MidiOutCoreMIDI.hh"
#endif

namespace openmsx {

using std::unique_ptr;

void PluggableFactory::createAll(PluggingController& controller,
                                 MSXMotherBoard& motherBoard)
{
	EventDistributor& eventDistributor =
		motherBoard.getReactor().getEventDistributor();
	Scheduler& scheduler = motherBoard.getScheduler();
	CommandController& commandController = motherBoard.getCommandController();
	MSXEventDistributor& msxEventDistributor =
		motherBoard.getMSXEventDistributor();
	StateChangeDistributor& stateChangeDistributor =
		motherBoard.getStateChangeDistributor();
	// Input devices:
	// TODO: Support hot-plugging of input devices:
	// - additional key joysticks can be created by the user
	// - real joysticks and mice can be hotplugged (USB)
	controller.registerPluggable(unique_ptr<Pluggable>(new ArkanoidPad(
		msxEventDistributor, stateChangeDistributor)));
	controller.registerPluggable(unique_ptr<Pluggable>(new Mouse(
		msxEventDistributor, stateChangeDistributor)));
	controller.registerPluggable(unique_ptr<Pluggable>(new Trackball(
		msxEventDistributor, stateChangeDistributor)));
	controller.registerPluggable(unique_ptr<Pluggable>(new JoyTap(
		controller, "joytap")));
	controller.registerPluggable(unique_ptr<Pluggable>(new NinjaTap(
		controller, "ninjatap")));
	controller.registerPluggable(unique_ptr<Pluggable>(new KeyJoystick(
		commandController, msxEventDistributor,
		stateChangeDistributor, "keyjoystick1")));
	controller.registerPluggable(unique_ptr<Pluggable>(new KeyJoystick(
		commandController, msxEventDistributor,
		stateChangeDistributor, "keyjoystick2")));
	Joystick::registerAll(msxEventDistributor, stateChangeDistributor,
	                      controller);
	JoyMega::registerAll(msxEventDistributor, stateChangeDistributor,
	                      controller);

	// Dongles
	controller.registerPluggable(unique_ptr<Pluggable>(new SETetrisDongle()));
	controller.registerPluggable(unique_ptr<Pluggable>(new MagicKey()));

	// Logging:
	controller.registerPluggable(unique_ptr<Pluggable>(new PrinterPortLogger(
		commandController)));
	controller.registerPluggable(unique_ptr<Pluggable>(new MidiOutLogger(
		commandController)));

	// Serial communication:
	controller.registerPluggable(unique_ptr<Pluggable>(new RS232Tester(
		eventDistributor, scheduler, commandController)));

	// Sampled audio:
	controller.registerPluggable(unique_ptr<Pluggable>(new PrinterPortSimpl(
		*motherBoard.getMachineConfig())));
	controller.registerPluggable(unique_ptr<Pluggable>(new WavAudioInput(
		commandController)));

	// MIDI:
	controller.registerPluggable(unique_ptr<Pluggable>(new MidiInReader(
		eventDistributor, scheduler, commandController)));
#if defined(_WIN32)
	MidiInWindows::registerAll(eventDistributor, scheduler, controller);
	MidiOutWindows::registerAll(controller);
#endif
#if defined(__APPLE__)
	controller.registerPluggable(unique_ptr<Pluggable>(new MidiOutCoreMIDIVirtual()));
	MidiOutCoreMIDI::registerAll(controller);
#endif

	// Printers
	controller.registerPluggable(unique_ptr<Pluggable>(new ImagePrinterMSX(
		motherBoard)));
	controller.registerPluggable(unique_ptr<Pluggable>(new ImagePrinterEpson(
		motherBoard)));
}

} // namespace openmsx

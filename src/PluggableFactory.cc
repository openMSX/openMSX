// $Id$

#include "PluggableFactory.hh"
#include "PluggingController.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "Joystick.hh"
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


namespace openmsx {

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
	controller.registerPluggable(new ArkanoidPad(msxEventDistributor,
	                                             stateChangeDistributor));
	controller.registerPluggable(new Mouse(msxEventDistributor,
	                                       stateChangeDistributor));
	controller.registerPluggable(new Trackball(msxEventDistributor,
	                                           stateChangeDistributor));
	controller.registerPluggable(new JoyTap(controller, "joytap"));
	controller.registerPluggable(new NinjaTap(controller, "ninjatap"));
	controller.registerPluggable(new KeyJoystick(
		commandController, msxEventDistributor,
		stateChangeDistributor, "keyjoystick1"));
	controller.registerPluggable(new KeyJoystick(
		commandController, msxEventDistributor,
		stateChangeDistributor, "keyjoystick2"));
	Joystick::registerAll(msxEventDistributor, stateChangeDistributor,
	                      controller);

	// Dongles
	controller.registerPluggable(new SETetrisDongle());
	controller.registerPluggable(new MagicKey());

	// Logging:
	controller.registerPluggable(new PrinterPortLogger(commandController));
	controller.registerPluggable(new MidiOutLogger(commandController));

	// Serial communication:
	controller.registerPluggable(new RS232Tester(
		eventDistributor, scheduler, commandController));

	// Sampled audio:
	controller.registerPluggable(new PrinterPortSimpl(motherBoard.getMSXMixer()));
	controller.registerPluggable(new WavAudioInput(commandController));

	// MIDI:
	controller.registerPluggable(new MidiInReader(
		eventDistributor, scheduler, commandController));
#if defined(_WIN32)
	MidiInWindows::registerAll(eventDistributor, scheduler, controller);
	MidiOutWindows::registerAll(controller);
#endif

	// Printers
	controller.registerPluggable(new ImagePrinterMSX(motherBoard));
	controller.registerPluggable(new ImagePrinterEpson(motherBoard));
}

} // namespace openmsx

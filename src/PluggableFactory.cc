// $Id$

#include "PluggableFactory.hh"
#include "PluggingController.hh"
#include "MSXMotherBoard.hh"
#include "Joystick.hh"
#include "JoyTap.hh"
#include "NinjaTap.hh"
#include "SETetrisDongle.hh"
#include "MagicKey.hh"
#include "KeyJoystick.hh"
#include "MidiInReader.hh"
#include "MidiOutLogger.hh"
#include "Mouse.hh"
#include "PrinterPortLogger.hh"
#include "PrinterPortSimpl.hh"
#include "Printer.hh"
#include "RS232Tester.hh"
#include "WavAudioInput.hh"
#if	defined(_WIN32)
#include "MidiInNative.hh"
#include "MidiOutNative.hh"
#endif


namespace openmsx {

void PluggableFactory::createAll(PluggingController& controller,
                                 MSXMotherBoard& motherBoard)
{
	EventDistributor& eventDistributor = motherBoard.getEventDistributor();
	Scheduler& scheduler = motherBoard.getScheduler();
	CommandController& commandController = motherBoard.getCommandController();
	MSXEventDistributor& msxEventDistributor =
		motherBoard.getMSXEventDistributor();
	// Input devices:
	// TODO: Support hot-plugging of input devices:
	// - additional key joysticks can be created by the user
	// - real joysticks and mice can be hotplugged (USB)
	controller.registerPluggable(new Mouse(motherBoard.getMSXEventDistributor()));
	controller.registerPluggable(new JoyTap(controller, "joytap"));
	controller.registerPluggable(new NinjaTap(controller, "ninjatap"));
	controller.registerPluggable(new KeyJoystick(
		commandController, msxEventDistributor, "keyjoystick1"));
	controller.registerPluggable(new KeyJoystick(
		commandController, msxEventDistributor, "keyjoystick2"));
	Joystick::registerAll(msxEventDistributor, controller);

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
	MidiInNative::registerAll(eventDistributor, scheduler, controller);
	MidiOutNative::registerAll(controller);
#endif

	// Printers
	controller.registerPluggable(new ImagePrinterMSX(motherBoard));
	controller.registerPluggable(new ImagePrinterEpson(motherBoard));
}

} // namespace openmsx

// $Id$

#include "probed_defs.hh"

#include "PluggableFactory.hh"
#include "PluggingController.hh"
#include "MSXMotherBoard.hh"

#include "JoyNet.hh"
#include "Joystick.hh"
#include "SETetrisDongle.hh"
#include "KeyJoystick.hh"
#include "MidiInReader.hh"
#include "MidiOutLogger.hh"
#include "Mouse.hh"
#include "PrinterPortLogger.hh"
#include "PrinterPortSimpl.hh"
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
	Scheduler& scheduler = motherBoard.getScheduler();
	CommandController& commandController = motherBoard.getCommandController();
	EventDistributor& eventDistributor = motherBoard.getEventDistributor();
	UserInputEventDistributor& userInputEventDistributor =
		motherBoard.getUserInputEventDistributor();
	// Input devices:
	// TODO: Support hot-plugging of input devices:
	// - additional key joysticks can be created by the user
	// - real joysticks and mice can be hotplugged (USB)
	controller.registerPluggable(new Mouse(motherBoard.getEventDistributor()));
#ifdef	HAVE_SYS_SOCKET_H
	controller.registerPluggable(new JoyNet(commandController));
#endif
	controller.registerPluggable(new KeyJoystick(
		commandController, userInputEventDistributor, "keyjoystick1"));
	controller.registerPluggable(new KeyJoystick(
		commandController, userInputEventDistributor, "keyjoystick2"));
	Joystick::registerAll(eventDistributor, controller);

	// SE tris II copy protection
	controller.registerPluggable(new SETetrisDongle());

	// Logging:
	controller.registerPluggable(new PrinterPortLogger(
		motherBoard.getCommandController()));
	controller.registerPluggable(new MidiOutLogger(commandController));

	// Serial communication:
	controller.registerPluggable(new RS232Tester(scheduler, commandController));

	// Sampled audio:
	controller.registerPluggable(new PrinterPortSimpl(motherBoard.getMixer()));
	controller.registerPluggable(new WavAudioInput(commandController));

	// MIDI:
	controller.registerPluggable(new MidiInReader(scheduler, commandController));
#if defined(_WIN32)
	MidiInNative::registerAll(scheduler, controller);
	MidiOutNative::registerAll(controller);
#endif
}

} // namespace openmsx

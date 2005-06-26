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
	// Input devices:
	// TODO: Support hot-plugging of input devices:
	// - additional key joysticks can be created by the user
	// - real joysticks and mice can be hotplugged (USB)
	controller.registerPluggable(new Mouse());
#ifdef	HAVE_SYS_SOCKET_H
	controller.registerPluggable(new JoyNet());
#endif
	controller.registerPluggable(new KeyJoystick());
	Joystick::registerAll(controller);

	// SE tris II copy protection
	controller.registerPluggable(new SETetrisDongle());

	// Logging:
	controller.registerPluggable(new PrinterPortLogger());
	controller.registerPluggable(new MidiOutLogger());

	// Serial communication:
	controller.registerPluggable(new RS232Tester());

	// Sampled audio:
	controller.registerPluggable(new PrinterPortSimpl(motherBoard.getMixer()));
	controller.registerPluggable(new WavAudioInput());

	// MIDI:
	controller.registerPluggable(new MidiInReader());
#if defined(_WIN32)
	MidiInNative::registerAll(controller);
	MidiOutNative::registerAll(controller);
#endif
}

} // namespace openmsx

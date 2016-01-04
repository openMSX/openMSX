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
#include "Touchpad.hh"
#include "PrinterPortLogger.hh"
#include "PrinterPortSimpl.hh"
#include "Printer.hh"
#include "RS232Tester.hh"
#include "WavAudioInput.hh"
#include "components.hh"
#if	defined(_WIN32)
#include "MidiInWindows.hh"
#include "MidiOutWindows.hh"
#endif
#if defined(__APPLE__)
#include "MidiInCoreMIDI.hh"
#include "MidiOutCoreMIDI.hh"
#endif
#if COMPONENT_ALSAMIDI
#include "MidiSessionALSA.hh"
#endif
#include "memory.hh"

namespace openmsx {

using std::unique_ptr;

void PluggableFactory::createAll(PluggingController& controller,
                                 MSXMotherBoard& motherBoard)
{
	auto& reactor                = motherBoard.getReactor();
	auto& scheduler              = motherBoard.getScheduler();
	auto& commandController      = motherBoard.getCommandController();
	auto& msxEventDistributor    = motherBoard.getMSXEventDistributor();
	auto& stateChangeDistributor = motherBoard.getStateChangeDistributor();
	auto& eventDistributor       = reactor.getEventDistributor();
	auto& globalSettings         = reactor.getGlobalSettings();
	// Input devices:
	// TODO: Support hot-plugging of input devices:
	// - additional key joysticks can be created by the user
	// - real joysticks and mice can be hotplugged (USB)
	controller.registerPluggable(make_unique<ArkanoidPad>(
		msxEventDistributor, stateChangeDistributor));
	controller.registerPluggable(make_unique<Mouse>(
		msxEventDistributor, stateChangeDistributor));
	controller.registerPluggable(make_unique<Trackball>(
		msxEventDistributor, stateChangeDistributor));
	controller.registerPluggable(make_unique<Touchpad>(
		msxEventDistributor, stateChangeDistributor,
		commandController));
	controller.registerPluggable(make_unique<JoyTap>(
		controller, "joytap"));
	controller.registerPluggable(make_unique<NinjaTap>(
		controller, "ninjatap"));
	controller.registerPluggable(make_unique<KeyJoystick>(
		commandController, msxEventDistributor,
		stateChangeDistributor, "keyjoystick1"));
	controller.registerPluggable(make_unique<KeyJoystick>(
		commandController, msxEventDistributor,
		stateChangeDistributor, "keyjoystick2"));
	Joystick::registerAll(msxEventDistributor, stateChangeDistributor,
	                      commandController, globalSettings, controller);
	JoyMega::registerAll(msxEventDistributor, stateChangeDistributor,
	                      controller);

	// Dongles
	controller.registerPluggable(make_unique<SETetrisDongle>());
	controller.registerPluggable(make_unique<MagicKey>());

	// Logging:
	controller.registerPluggable(make_unique<PrinterPortLogger>(
		commandController));
	controller.registerPluggable(make_unique<MidiOutLogger>(
		commandController));

	// Serial communication:
	controller.registerPluggable(make_unique<RS232Tester>(
		eventDistributor, scheduler, commandController));

	// Sampled audio:
	controller.registerPluggable(make_unique<PrinterPortSimpl>(
		*motherBoard.getMachineConfig()));
	controller.registerPluggable(make_unique<WavAudioInput>(
		commandController));

	// MIDI:
	controller.registerPluggable(make_unique<MidiInReader>(
		eventDistributor, scheduler, commandController));
#if defined(_WIN32)
	MidiInWindows::registerAll(eventDistributor, scheduler, controller);
	MidiOutWindows::registerAll(controller);
#endif
#if defined(__APPLE__)
	controller.registerPluggable(make_unique<MidiInCoreMIDIVirtual>(
		eventDistributor, scheduler));
	MidiInCoreMIDI::registerAll(eventDistributor, scheduler, controller);
	controller.registerPluggable(make_unique<MidiOutCoreMIDIVirtual>());
	MidiOutCoreMIDI::registerAll(controller);
#endif
#if COMPONENT_ALSAMIDI
	MidiSessionALSA::registerAll(controller, reactor.getCliComm());
#endif

	// Printers
	controller.registerPluggable(make_unique<ImagePrinterMSX>(
		motherBoard));
	controller.registerPluggable(make_unique<ImagePrinterEpson>(
		motherBoard));
}

} // namespace openmsx

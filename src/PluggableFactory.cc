#include "PluggableFactory.hh"

#include "ArkanoidPad.hh"
#include "CircuitDesignerRDDongle.hh"
#include "InputEventGenerator.hh"
#include "JoyMega.hh"
#include "JoyTap.hh"
#include "MSXJoystick.hh"
#include "MSXMotherBoard.hh"
#include "MagicKey.hh"
#include "MidiInReader.hh"
#include "MidiOutLogger.hh"
#include "Mouse.hh"
#include "NinjaTap.hh"
#include "Paddle.hh"
#include "PluggingController.hh"
#include "Plotter.hh"
#include "Printer.hh"
#include "PrinterPortLogger.hh"
#include "PrinterPortSimpl.hh"
#include "RS232Net.hh"
#include "RS232Tester.hh"
#include "Reactor.hh"
#include "SETetrisDongle.hh"
#include "Touchpad.hh"
#include "Trackball.hh"
#include "WavAudioInput.hh"

#include "components.hh"
#if	defined(_WIN32)
#include "MidiInWindows.hh"
#include "MidiOutWindows.hh"
#endif
#ifdef __APPLE__
#include "MidiInCoreMIDI.hh"
#include "MidiOutCoreMIDI.hh"
#endif
#if COMPONENT_ALSAMIDI
#include "MidiSessionALSA.hh"
#endif

#include <memory>

namespace openmsx {

void PluggableFactory::createAll(PluggingController& controller,
                                 MSXMotherBoard& motherBoard)
{
	auto& reactor                = motherBoard.getReactor();
	auto& scheduler              = motherBoard.getScheduler();
	auto& commandController      = motherBoard.getCommandController();
	auto& msxEventDistributor    = motherBoard.getMSXEventDistributor();
	auto& stateChangeDistributor = motherBoard.getStateChangeDistributor();
	auto& eventDistributor       = reactor.getEventDistributor();
	auto& joystickManager         = reactor.getInputEventGenerator().getJoystickManager();
	auto& display                = reactor.getDisplay();
	// Input devices:
	// TODO: Support hot-plugging of input devices:
	// - additional key joysticks can be created by the user
	// - real joysticks and mice can be hot-plugged (USB)
	controller.registerPluggable(std::make_unique<ArkanoidPad>(
		msxEventDistributor, stateChangeDistributor));
	controller.registerPluggable(std::make_unique<Mouse>(
		msxEventDistributor, stateChangeDistributor));
	controller.registerPluggable(std::make_unique<Paddle>(
		msxEventDistributor, stateChangeDistributor));
	controller.registerPluggable(std::make_unique<Trackball>(
		msxEventDistributor, stateChangeDistributor));
	controller.registerPluggable(std::make_unique<Touchpad>(
		msxEventDistributor, stateChangeDistributor,
		display, commandController));
	controller.registerPluggable(std::make_unique<JoyTap>(
		controller, "joytap"));
	controller.registerPluggable(std::make_unique<NinjaTap>(
		controller, "ninjatap"));
	controller.registerPluggable(std::make_unique<MSXJoystick>(
		commandController, msxEventDistributor,
		stateChangeDistributor, joystickManager, 1)); // msxjoystick1
	controller.registerPluggable(std::make_unique<MSXJoystick>(
		commandController, msxEventDistributor,
		stateChangeDistributor, joystickManager, 2)); // msxjoystick2
	controller.registerPluggable(std::make_unique<JoyMega>(
		commandController, msxEventDistributor,
		stateChangeDistributor, joystickManager, 1)); // joymega1
	controller.registerPluggable(std::make_unique<JoyMega>(
		commandController, msxEventDistributor,
		stateChangeDistributor, joystickManager, 2)); // joymega2

	// Dongles
	controller.registerPluggable(std::make_unique<SETetrisDongle>());
	controller.registerPluggable(std::make_unique<MagicKey>());
	controller.registerPluggable(std::make_unique<CircuitDesignerRDDongle>());

	// Logging:
	controller.registerPluggable(std::make_unique<PrinterPortLogger>(
		commandController));
	controller.registerPluggable(std::make_unique<MidiOutLogger>(
		commandController));

	// Serial communication:
	controller.registerPluggable(std::make_unique<RS232Tester>(
		eventDistributor, scheduler, commandController));
	controller.registerPluggable(std::make_unique<RS232Net>(
		eventDistributor, scheduler, commandController));

	// Sampled audio:
	controller.registerPluggable(std::make_unique<PrinterPortSimpl>(
		*motherBoard.getMachineConfig()));
	controller.registerPluggable(std::make_unique<WavAudioInput>(
		commandController));

	// MIDI:
#ifndef _WIN32
	// Devices and pipes are not usable as files on Windows, and MidiInReader
	// reads all data as soon as it becomes available, so this pluggable is
	// not useful on Windows.
	controller.registerPluggable(std::make_unique<MidiInReader>(
		eventDistributor, scheduler, commandController));
#endif
#ifdef _WIN32
	MidiInWindows::registerAll(eventDistributor, scheduler, controller);
	MidiOutWindows::registerAll(controller);
#endif
#ifdef __APPLE__
	controller.registerPluggable(std::make_unique<MidiInCoreMIDIVirtual>(
		eventDistributor, scheduler));
	MidiInCoreMIDI::registerAll(eventDistributor, scheduler, controller);
	controller.registerPluggable(std::make_unique<MidiOutCoreMIDIVirtual>());
	MidiOutCoreMIDI::registerAll(controller);
#endif
#if COMPONENT_ALSAMIDI
	MidiSessionALSA::registerAll(controller, reactor.getCliComm(), eventDistributor, scheduler);
#endif

	// Printers
	controller.registerPluggable(std::make_unique<ImagePrinterMSX>(
		motherBoard));
	controller.registerPluggable(std::make_unique<ImagePrinterEpson>(
		motherBoard));
	controller.registerPluggable(std::make_unique<MSXPlotter>(
		motherBoard));
}

} // namespace openmsx

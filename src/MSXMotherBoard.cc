// $Id$

#include "MSXMotherBoard.hh"
#include "Leds.hh"
#include "MSXDevice.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "EmuTime.hh"
#include "PluggingController.hh"
#include "Mixer.hh"
#include "MSXConfig.hh"
#include "DeviceFactory.hh"
#include "CommandController.hh"
#include "KeyEventInserter.hh"
#include "MSXCPUInterface.hh"


namespace openmsx {

MSXMotherBoard::MSXMotherBoard()
{
	CommandController::instance()->registerCommand(&resetCmd, "reset");
}

MSXMotherBoard::~MSXMotherBoard()
{
	CommandController::instance()->unregisterCommand(&resetCmd, "reset");
}

MSXMotherBoard *MSXMotherBoard::instance()
{
	static MSXMotherBoard* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new MSXMotherBoard();
	return oneInstance;
}


void MSXMotherBoard::addDevice(MSXDevice *device)
{
	availableDevices.push_back(device);
}

void MSXMotherBoard::removeDevice(MSXDevice *device)
{
	availableDevices.remove(device);
}


void MSXMotherBoard::resetMSX()
{
	const EmuTime &time = MSXCPU::instance()->getCurrentTime();
	MSXCPUInterface::instance()->reset();
	list<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		(*i)->reset(time);
	}
}

void MSXMotherBoard::run()
{
	// Initialise devices.
	MSXConfig* config = MSXConfig::instance();
	config->initDeviceIterator();
	Device* d;
	while ((d = config->getNextDevice()) != 0) {
		PRT_DEBUG("Instantiating: " << d->getType());
		MSXDevice *device = DeviceFactory::create(d, EmuTime::zero);
		addDevice(device);
	}
	// Register all postponed slots.
	MSXCPUInterface::instance()->registerPostSlots();

	// First execute auto commands.
	CommandController::instance()->autoCommands();

	// Schedule key insertions.
	// TODO move this somewhere else
	KeyEventInserter keyEvents(EmuTime::zero);

	// Initialize.
	MSXCPUInterface::instance()->reset();
	Leds::instance()->setLed(Leds::POWER_ON);

	// Run.
	EmuTime time(Scheduler::instance()->scheduleEmulation());

	// Shut down mixing because it depends on MSXCPU,
	// which will be destroyed in the next step.
	// TODO: Get rid of this dependency.
	Mixer::instance()->shutDown();

	// Destroy emulated MSX machine.
	for (list<MSXDevice*>::iterator i = availableDevices.begin();
	     i != availableDevices.end(); ++i) {
		(*i)->powerDown(time);
		delete (*i);
	}
}


string MSXMotherBoard::ResetCmd::execute(const vector<string> &tokens)
	throw()
{
	MSXMotherBoard::instance()->resetMSX();
	return "";
}
string MSXMotherBoard::ResetCmd::help(const vector<string> &tokens) const
	throw()
{
	return "Resets the MSX.\n";
}

} // namespace openmsx


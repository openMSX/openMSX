// $Id$

#include "MSXMotherBoard.hh"
#include "MSXDevice.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "EmuTime.hh"
#include "PluggingController.hh"
#include "Mixer.hh"
#include "MSXConfig.hh"
#include "Device.hh"
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
	const EmuTime& time = MSXCPU::instance()->getCurrentTime();
	MSXCPUInterface::instance()->reset();
	MSXCPU::instance()->reset(time);
	for (list<MSXDevice*>::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reset(time);
	}
}

void MSXMotherBoard::reInitMSX()
{
	const EmuTime& time = MSXCPU::instance()->getCurrentTime();
	MSXCPUInterface::instance()->reset();
	MSXCPU::instance()->reset(time);
	for (list<MSXDevice*>::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reInit(time);
	}
}

void MSXMotherBoard::run(bool powerOn)
{
	// Initialise devices.
	MSXConfig* config = MSXConfig::instance();
	config->initDeviceIterator();
	Device* d;
	while ((d = config->getNextDevice()) != 0) {
		PRT_DEBUG("Instantiating: " << d->getType());
		MSXDevice* device = DeviceFactory::create(d, EmuTime::zero);
		if (device) {
			addDevice(device);
		}
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

	// Run.
	if (powerOn) {
		Scheduler::instance()->powerOn();
	}
	EmuTime time(Scheduler::instance()->scheduleEmulation());
	Scheduler::instance()->powerOff();

	// Shut down mixing because it depends on MSXCPU,
	// which will be destroyed in the next step.
	// TODO: Get rid of this dependency.
	Mixer::instance()->shutDown();

	// Destroy emulated MSX machine.
	for (list<MSXDevice*>::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		MSXDevice* device = *it;
		delete device;
	}
	availableDevices.clear();
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


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

namespace openmsx {

MSXMotherBoard::MSXMotherBoard()
	: powerSetting(Scheduler::instance().getPowerSetting())
{
	Scheduler::instance().setMotherBoard(this);
	powerSetting.addListener(this);
	
}

MSXMotherBoard::~MSXMotherBoard()
{
	// Destroy emulated MSX machine.
	for (list<MSXDevice*>::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		MSXDevice* device = *it;
		delete device;
	}
	availableDevices.clear();

	powerSetting.removeListener(this);
	Scheduler::instance().setMotherBoard(NULL);
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
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	PRT_DEBUG("MSXMotherBoard::reset() @ " << time);
	MSXCPUInterface::instance().reset();
	for (list<MSXDevice*>::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reset(time);
	}
	MSXCPU::instance().reset(time);
}

void MSXMotherBoard::reInitMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	PRT_DEBUG("MSXMotherBoard::reinit() @ " << time);
	MSXCPUInterface::instance().reset();
	for (list<MSXDevice*>::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reInit(time);
	}
	MSXCPU::instance().reset(time);
}

void MSXMotherBoard::run(bool powerOn)
{
	// Initialise devices.
	const MSXConfig::Devices& devices = MSXConfig::instance().getDevices();
	for (MSXConfig::Devices::const_iterator it = devices.begin();
	     it != devices.end(); ++it) {
		PRT_DEBUG("Instantiating: " << (*it)->getType());
		MSXDevice* device = DeviceFactory::create(*it, EmuTime::zero);
		if (device) {
			addDevice(device);
		}
	}
	// Register all postponed slots.
	MSXCPUInterface::instance().registerPostSlots();

	// First execute auto commands.
	CommandController::instance().autoCommands();

	// Initialize.
	MSXCPUInterface::instance().reset();

	// Run.
	if (powerOn) {
		Scheduler::instance().powerOn();
	}
	Scheduler::instance().schedule(EmuTime::zero, EmuTime::infinity);
	Scheduler::instance().powerOff();
}

void MSXMotherBoard::update(const SettingLeafNode* setting) throw()
{
	assert(setting == &powerSetting);
	if (!powerSetting.getValue()) {
		reInitMSX();
	}
}

} // namespace openmsx


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
#include "HardwareConfig.hh"
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
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		delete *it;
	}
	availableDevices.clear();

	powerSetting.removeListener(this);
	Scheduler::instance().setMotherBoard(NULL);
}

void MSXMotherBoard::addDevice(auto_ptr<MSXDevice> device)
{
	availableDevices.push_back(device.release());
}


void MSXMotherBoard::resetMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	PRT_DEBUG("MSXMotherBoard::reset() @ " << time);
	MSXCPUInterface::instance().reset();
	for (Devices::iterator it = availableDevices.begin();
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
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reInit(time);
	}
	MSXCPU::instance().reset(time);
}

void MSXMotherBoard::run(bool powerOn)
{
	// Initialise devices.
	const XMLElement::Children& configs =
		HardwareConfig::instance().getChildren();
	for (XMLElement::Children::const_iterator it = configs.begin();
	     it != configs.end(); ++it) {
		if ((*it)->getName() != "device") {
			continue;
		}
		PRT_DEBUG("Instantiating: " << (*it)->getChildData("type"));
		auto_ptr<MSXDevice> device(DeviceFactory::create(**it, EmuTime::zero));
		addDevice(device);
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
	Scheduler::instance().schedule();
	Scheduler::instance().powerOff();
}

void MSXMotherBoard::update(const SettingLeafNode* setting)
{
	assert(setting == &powerSetting);
	if (!powerSetting.getValue()) {
		reInitMSX();
	}
}

} // namespace openmsx


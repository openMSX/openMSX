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
#include "KeyEventInserter.hh"


namespace openmsx {

MSXMotherBoard::MSXMotherBoard()
	: resetCmd(*this),
	  powerSetting(Scheduler::instance().getPowerSetting())
{
	CommandController::instance().registerCommand(&resetCmd, "reset");
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
	CommandController::instance().unregisterCommand(&resetCmd, "reset");
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
	MSXConfig& config = MSXConfig::instance();
	config.initDeviceIterator();
	Device* d;
	while ((d = config.getNextDevice()) != 0) {
		PRT_DEBUG("Instantiating: " << d->getType());
		MSXDevice* device = DeviceFactory::create(d, EmuTime::zero);
		if (device) {
			addDevice(device);
		}
	}
	// Register all postponed slots.
	MSXCPUInterface::instance().registerPostSlots();

	// First execute auto commands.
	CommandController::instance().autoCommands();

	// Schedule key insertions.
	// TODO move this somewhere else
	KeyEventInserter keyEvents(EmuTime::zero);

	// Initialize.
	MSXCPUInterface::instance().reset();

	// Run.
	if (powerOn) {
		Scheduler::instance().powerOn();
	}
	Scheduler::instance().schedule(EmuTime::infinity);
	Scheduler::instance().powerOff();
}

void MSXMotherBoard::update(const SettingLeafNode* setting) throw()
{
	assert(setting == &powerSetting);
	if (!powerSetting.getValue()) {
		reInitMSX();
	}
}


MSXMotherBoard::ResetCmd::ResetCmd(MSXMotherBoard& parent_)
	: parent(parent_)
{
}

string MSXMotherBoard::ResetCmd::execute(const vector<string> &tokens)
	throw()
{
	parent.resetMSX();
	return "";
}
string MSXMotherBoard::ResetCmd::help(const vector<string> &tokens) const
	throw()
{
	return "Resets the MSX.\n";
}

} // namespace openmsx


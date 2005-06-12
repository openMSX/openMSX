// $Id$

#include "MSXMotherBoard.hh"
#include "MSXDevice.hh"
#include "Scheduler.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "EmuTime.hh"
#include "HardwareConfig.hh"
#include "DeviceFactory.hh"
#include "CliComm.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

MSXMotherBoard::MSXMotherBoard()
	: blockedCounter(0)
	, output(CliComm::instance())
{
	MSXCPU::instance().setMotherboard(this);

	// Initialise devices.
	//PRT_DEBUG(HardwareConfig::instance().getChild("devices").dump());
	createDevices(HardwareConfig::instance().getChild("devices"));
}

MSXMotherBoard::~MSXMotherBoard()
{
	// Destroy emulated MSX machine.
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		delete *it;
	}
	availableDevices.clear();
}

void MSXMotherBoard::execute()
{
	MSXCPU::instance().execute();
}

void MSXMotherBoard::block()
{
	++blockedCounter;
	MSXCPU::instance().exitCPULoop();
}

void MSXMotherBoard::unblock()
{
	--blockedCounter;
	assert(blockedCounter >= 0);
}

void MSXMotherBoard::addDevice(std::auto_ptr<MSXDevice> device)
{
	availableDevices.push_back(device.release());
}

void MSXMotherBoard::resetMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	MSXCPUInterface::instance().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reset(time);
	}
	MSXCPU::instance().reset(time);
}

void MSXMotherBoard::powerDownMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerDown(time);
	}
}

void MSXMotherBoard::powerUpMSX()
{
	const EmuTime& time = Scheduler::instance().getCurrentTime();
	MSXCPUInterface::instance().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerUp(time);
	}
	MSXCPU::instance().reset(time);
}

void MSXMotherBoard::createDevices(const XMLElement& elem)
{
	const XMLElement::Children& children = elem.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		const XMLElement& sub = **it;
		const string& name = sub.getName();
		if ((name == "primary") || (name == "secondary")) {
			createDevices(sub);
		} else {
			PRT_DEBUG("Instantiating: " << name);
			std::auto_ptr<MSXDevice> device(
				DeviceFactory::create(sub, EmuTime::zero));
			if (device.get()) {
				device->setMotherboard(*this);
				addDevice(device);
			} else {
				output.printWarning("Deprecated device: \"" +
					name + "\", please upgrade your "
					"machine descriptions.");
			}
		}
	}
}

} // namespace openmsx

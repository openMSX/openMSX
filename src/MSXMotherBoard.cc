// $Id$

#include "MSXMotherBoard.hh"
#include "Leds.hh"
#include "MSXDevice.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "MSXCPUInterface.hh"


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


void MSXMotherBoard::resetMSX(const EmuTime &time)
{
	MSXCPUInterface::instance()->reset();
	std::list<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		(*i)->reset(time);
	}
}

void MSXMotherBoard::run()
{
	// initialize
	MSXCPUInterface::instance()->reset();
	Leds::instance()->setLed(Leds::POWER_ON);

	// run
	EmuTime time(Scheduler::instance()->scheduleEmulation());

	// destroy
	std::list<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		(*i)->powerDown(time);
		delete (*i);
	}
}


void MSXMotherBoard::ResetCmd::execute(const std::vector<std::string> &tokens,
                                       const EmuTime &time)
{
	MSXMotherBoard::instance()->resetMSX(time);
}
void MSXMotherBoard::ResetCmd::help(const std::vector<std::string> &tokens) const
{
	print("Resets the MSX.");
}


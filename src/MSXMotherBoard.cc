// $Id$

#include "MSXMotherBoard.hh"
#include "RealTime.hh"
#include "Leds.hh"
#include "MSXDevice.hh"
#include "ConsoleSource/ConsoleManager.hh"
#include "ConsoleSource/CommandController.hh"
#include "Scheduler.hh"


MSXMotherBoard::MSXMotherBoard(MSXConfig::Config *config) : MSXCPUInterface(config)
{
	PRT_DEBUG("Creating an MSXMotherBoard object");
	CommandController::instance()->registerCommand(resetCmd, "reset");
}

MSXMotherBoard::~MSXMotherBoard()
{
	PRT_DEBUG("Destructing an MSXMotherBoard object");
	CommandController::instance()->unregisterCommand("reset");
}

MSXMotherBoard *MSXMotherBoard::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new MSXMotherBoard(
			MSXConfig::Backend::instance()->getConfigById("MotherBoard"));
	}
	return oneInstance;
}
MSXMotherBoard *MSXMotherBoard::oneInstance = NULL;


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
	setPrimarySlots(0);
	std::list<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		(*i)->reset(time);
	}
}

void MSXMotherBoard::startMSX()
{
	setPrimarySlots(0);
	Leds::instance()->setLed(Leds::POWER_ON);
	RealTime::instance();
	Scheduler::instance()->scheduleEmulation();
}

void MSXMotherBoard::destroyMSX()
{
	std::list<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		delete (*i);
	}
}

void MSXMotherBoard::executeUntilEmuTime(const EmuTime &time, int userData)
{
	resetMSX(time);
}


void MSXMotherBoard::ResetCmd::execute(const std::vector<std::string> &tokens)
{
	Scheduler::instance()->setSyncPoint(Scheduler::ASAP, MSXMotherBoard::instance());
}
void MSXMotherBoard::ResetCmd::help   (const std::vector<std::string> &tokens)
{
	ConsoleManager::instance()->print("Resets the MSX.");
}


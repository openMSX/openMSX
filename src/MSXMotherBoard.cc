// $Id$

#include "MSXMotherBoard.hh"
#include "RealTime.hh"
#include "Leds.hh"
#include "MSXCPU.hh"
#include "MSXDevice.hh"
#include "ConsoleSource/Console.hh"


MSXMotherBoard::MSXMotherBoard(MSXConfig::Config *config) : MSXCPUInterface(config)
{
	PRT_DEBUG("Creating an MSXMotherBoard object");
	Console::instance()->registerCommand(resetCmd, "reset");
}

MSXMotherBoard::~MSXMotherBoard()
{
	PRT_DEBUG("Destructing an MSXMotherBoard object");
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

void MSXMotherBoard::resetMSX(const EmuTime &time)
{
	setPrimarySlots(0);
	std::vector<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		(*i)->reset(time);
	}
}

void MSXMotherBoard::startMSX()
{
	resetIRQLine();
	setPrimarySlots(0);
	Leds::instance()->setLed(Leds::POWER_ON);
	RealTime::instance();
	Scheduler::instance()->scheduleEmulation();
}

void MSXMotherBoard::destroyMSX()
{
	std::vector<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		delete (*i);
	}
}

void MSXMotherBoard::saveStateMSX(std::ofstream &savestream)
{
	std::vector<MSXDevice*>::iterator i;
	for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
		(*i)->saveState(savestream);
	}
}


void MSXMotherBoard::ResetCmd::execute(const std::vector<std::string> &tokens)
{
	MSXMotherBoard::instance()->resetMSX(MSXCPU::instance()->getCurrentTime());
}
void MSXMotherBoard::ResetCmd::help   (const std::vector<std::string> &tokens)
{
	Console::instance()->print("Resets the MSX.");
}


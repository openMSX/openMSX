// $Id$

#include "MSXMotherBoard.hh"
#include "Leds.hh"
#include "MSXDevice.hh"
#include "CommandController.hh"
#include "Scheduler.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "EmuTime.hh"


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
	try {
		// Initialize.
		MSXCPUInterface::instance()->reset();
		Leds::instance()->setLed(Leds::POWER_ON);

		// Run.
		EmuTime time(Scheduler::instance()->scheduleEmulation());

		// Destroy.
		list<MSXDevice*>::iterator i;
		for (i = availableDevices.begin(); i != availableDevices.end(); i++) {
			(*i)->powerDown(time);
			delete (*i);
		}
	} catch (MSXException &e) {
		PRT_ERROR("Uncaught exception: " << e.getMessage());
	} catch (...) {
		PRT_ERROR("Uncaught exception of unexpected type.");
	}
}


void MSXMotherBoard::ResetCmd::execute(const vector<string> &tokens)
{
	MSXMotherBoard::instance()->resetMSX();
}
void MSXMotherBoard::ResetCmd::help(const vector<string> &tokens) const
{
	print("Resets the MSX.");
}


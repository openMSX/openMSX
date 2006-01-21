// $Id$

#include "MSXMotherBoard.hh"
#include "MSXDevice.hh"
#include "MachineConfig.hh"
#include "ExtensionConfig.hh"
#include "Scheduler.hh"
#include "CartridgeSlotManager.hh"
#include "Debugger.hh"
#include "Mixer.hh"
#include "PluggingController.hh"
#include "DummyDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "PanasonicMemory.hh"
#include "MSXDeviceSwitch.hh"
#include "CassettePort.hh"
#include "RenShaTurbo.hh"
#include "CommandConsole.hh"
#include "Display.hh"
#include "IconStatus.hh"
#include "FileManipulator.hh"
#include "FilePool.hh"
#include "EmuTime.hh"
#include "LedEvent.hh"
#include "EventDistributor.hh"
#include "UserInputEventDistributor.hh"
#include "InputEventGenerator.hh"
#include "CliComm.hh"
#include "CommandController.hh"
#include "RealTime.hh"
#include "BooleanSetting.hh"
#include "FileContext.hh"
#include "GlobalSettings.hh"
#include "Command.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class ResetCmd : public SimpleCommand
{
public:
	ResetCmd(CommandController& commandController,
	         MSXMotherBoard& motherBoard);
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	MSXMotherBoard& motherBoard;
};


MSXMotherBoard::MSXMotherBoard()
	: powered(false)
	, needReset(false)
	, needPowerDown(false)
	, blockedCounter(0)
	, resetCommand(new ResetCmd(getCommandController(), *this))
	, powerSetting(getCommandController().getGlobalSettings().getPowerSetting())
{
	getMixer().mute(); // powered down
	powerSetting.attach(*this);
}

MSXMotherBoard::~MSXMotherBoard()
{
	powerSetting.detach(*this);
	deleteMachine();

	assert(availableDevices.empty());
	assert(extensions.empty());
	assert(!machineConfig.get());
}

void MSXMotherBoard::deleteMachine()
{
	for (Extensions::reverse_iterator it = extensions.rbegin();
	     it != extensions.rend(); ++it) {
		delete *it;
	}
	extensions.clear();

	machineConfig.reset();
}

MachineConfig& MSXMotherBoard::getMachineConfig()
{
	if (!machineConfig.get()) {
		machineConfig.reset(new MachineConfig(*this));
	}
	return *machineConfig;
}

void MSXMotherBoard::addExtension(std::auto_ptr<ExtensionConfig>& extension)
{
	extensions.push_back(extension.release());
}


Scheduler& MSXMotherBoard::getScheduler()
{
	if (!scheduler.get()) {
		scheduler.reset(new Scheduler());
	}
	return *scheduler;
}

CartridgeSlotManager& MSXMotherBoard::getSlotManager()
{
	if (!slotManager.get()) {
		slotManager.reset(new CartridgeSlotManager(*this));
	}
	return *slotManager;
}

CommandController& MSXMotherBoard::getCommandController()
{
	if (!commandController.get()) {
		commandController.reset(new CommandController(getScheduler()));

		// TODO remove this hack
		//   needed to properly initialize circular dependency between
		//   CommandController and CliComm
		cliComm.reset(new CliComm(getScheduler(),
			*commandController, getEventDistributor()));
	}
	return *commandController;
}

EventDistributor& MSXMotherBoard::getEventDistributor()
{
	if (!eventDistributor.get()) {
		eventDistributor.reset(new EventDistributor(
			getScheduler(), getCommandController()));
	}
	return *eventDistributor;
}

UserInputEventDistributor& MSXMotherBoard::getUserInputEventDistributor()
{
	if (!userInputEventDistributor.get()) {
		userInputEventDistributor.reset(
			new UserInputEventDistributor(getEventDistributor()));
	}
	return *userInputEventDistributor;
}

InputEventGenerator& MSXMotherBoard::getInputEventGenerator()
{
	if (!inputEventGenerator.get()) {
		inputEventGenerator.reset(new InputEventGenerator(
			getScheduler(), getCommandController(),
			getEventDistributor()));
	}
	return *inputEventGenerator;
}

CliComm& MSXMotherBoard::getCliComm()
{
	if (!cliComm.get()) {
		cliComm.reset(new CliComm(getScheduler(),
			getCommandController(), getEventDistributor()));
	}
	return *cliComm;
}

RealTime& MSXMotherBoard::getRealTime()
{
	if (!realTime.get()) {
		realTime.reset(new RealTime(
			getScheduler(), getEventDistributor(),
			getCommandController().getGlobalSettings()));
	}
	return *realTime;
}

Debugger& MSXMotherBoard::getDebugger()
{
	if (!debugger.get()) {
		debugger.reset(new Debugger(getCommandController()));
	}
	return *debugger;
}

Mixer& MSXMotherBoard::getMixer()
{
	if (!mixer.get()) {
		mixer.reset(new Mixer(getScheduler(), getCommandController()));
	}
	return *mixer;
}

PluggingController& MSXMotherBoard::getPluggingController()
{
	if (!pluggingController.get()) {
		pluggingController.reset(new PluggingController(*this));
	}
	return *pluggingController;
}

DummyDevice& MSXMotherBoard::getDummyDevice()
{
	if (!dummyDevice.get()) {
		dummyDeviceConfig.reset(new XMLElement("Dummy"));
		dummyDeviceConfig->addAttribute("id", "empty");
		dummyDeviceConfig->setFileContext(std::auto_ptr<FileContext>(
			new SystemFileContext()));
		dummyDevice.reset(
			new DummyDevice(*this, *dummyDeviceConfig, EmuTime::zero));
	}
	return *dummyDevice;
}

MSXCPU& MSXMotherBoard::getCPU()
{
	if (!msxCpu.get()) {
		msxCpu.reset(new MSXCPU(*this));
	}
	return *msxCpu;
}

MSXCPUInterface& MSXMotherBoard::getCPUInterface()
{
	if (!msxCpuInterface.get()) {
		// TODO assert hw config already loaded
		msxCpuInterface = MSXCPUInterface::create(
			*this, getMachineConfig().getConfig());
	}
	return *msxCpuInterface;
}

PanasonicMemory& MSXMotherBoard::getPanasonicMemory()
{
	if (!panasonicMemory.get()) {
		panasonicMemory.reset(new PanasonicMemory(*this));
	}
	return *panasonicMemory;
}

MSXDeviceSwitch& MSXMotherBoard::getDeviceSwitch()
{
	if (!deviceSwitch.get()) {
		devSwitchConfig.reset(new XMLElement("DeviceSwitch"));
		devSwitchConfig->addAttribute("id", "DeviceSwitch");
		devSwitchConfig->setFileContext(std::auto_ptr<FileContext>(
			new SystemFileContext()));
		deviceSwitch.reset(
			new MSXDeviceSwitch(*this, *devSwitchConfig, EmuTime::zero));
	}
	return *deviceSwitch;
}

CassettePortInterface& MSXMotherBoard::getCassettePort()
{
	if (!cassettePort.get()) {
		if (getMachineConfig().getConfig().findChild("CassettePort")) {
			cassettePort.reset(new CassettePort(*this));
		} else {
			cassettePort.reset(new DummyCassettePort());
		}
	}
	return *cassettePort;
}

RenShaTurbo& MSXMotherBoard::getRenShaTurbo()
{
	if (!renShaTurbo.get()) {
		renShaTurbo.reset(new RenShaTurbo(
			getCommandController(), getMachineConfig().getConfig()));
	}
	return *renShaTurbo;
}

CommandConsole& MSXMotherBoard::getCommandConsole()
{
	if (!commandConsole.get()) {
		commandConsole.reset(new CommandConsole(
			getCommandController(),
			getEventDistributor()));
	}
	return *commandConsole;
}

Display& MSXMotherBoard::getDisplay()
{
	if (!display.get()) {
		display.reset(new Display(*this));
	}
	return *display;
}

IconStatus& MSXMotherBoard::getIconStatus()
{
	if (!iconStatus.get()) {
		iconStatus.reset(new IconStatus(
			getEventDistributor(), getDisplay()));
	}
	return *iconStatus;
}

FileManipulator& MSXMotherBoard::getFileManipulator()
{
	if (!fileManipulator.get()) {
		fileManipulator.reset(new FileManipulator(
			getCommandController()));
	}
	return *fileManipulator;
}

FilePool& MSXMotherBoard::getFilePool()
{
	if (!filePool.get()) {
		filePool.reset(new FilePool(
			getCommandController().getSettingsConfig()));
	}
	return *filePool;
}

void MSXMotherBoard::readConfig()
{
	getMachineConfig().parseSlots();
	for (Extensions::const_iterator it = extensions.begin();
	     it != extensions.end(); ++it) {
		(*it)->parseSlots();
	}

	getMachineConfig().createDevices();
	for (Extensions::const_iterator it = extensions.begin();
	     it != extensions.end(); ++it) {
		(*it)->createDevices();
	}
}

bool MSXMotherBoard::execute()
{
	if (needReset) {
		needReset = false;
		doReset(getScheduler().getCurrentTime());
	}
	if (needPowerDown) {
		needPowerDown = false;
		doPowerDown(getScheduler().getCurrentTime());
	}

	if (!powered || blockedCounter) {
		return false;
	}

	getCPU().execute();
	return true;
}

void MSXMotherBoard::block()
{
	++blockedCounter;
	getCPU().exitCPULoop();
	getMixer().mute();
}

void MSXMotherBoard::unblock()
{
	--blockedCounter;
	assert(blockedCounter >= 0);
	getMixer().unmute();
}

void MSXMotherBoard::pause()
{
	getCPU().setPaused(true);
	getMixer().mute();
}

void MSXMotherBoard::unpause()
{
	getCPU().setPaused(false);
	getMixer().unmute();
}

void MSXMotherBoard::addDevice(MSXDevice& device)
{
	availableDevices.push_back(&device);
}

void MSXMotherBoard::removeDevice(MSXDevice& device)
{
	Devices::iterator it = find(availableDevices.begin(),
	                            availableDevices.end(), &device);
	assert(it != availableDevices.end());
	availableDevices.erase(it);
}

void MSXMotherBoard::scheduleReset()
{
	needReset = true;
	getCPU().exitCPULoop();
}

void MSXMotherBoard::doReset(const EmuTime& time)
{
	getCPUInterface().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reset(time);
	}
	getCPU().doReset(time);
}

void MSXMotherBoard::powerUp()
{
	if (powered) return;

	powered = true;
	// TODO: If our "powered" field is always equal to the power setting,
	//       there is not really a point in keeping it.
	// TODO: assert disabled see note in Reactor::run() where this method
	//       is called
	//assert(powerSetting.getValue() == powered);
	powerSetting.setValue(true);
	// TODO: We could make the power LED a device, so we don't have to handle
	//       it separately here.
	getEventDistributor().distributeEvent(
		new LedEvent(LedEvent::POWER, true));

	const EmuTime& time = getScheduler().getCurrentTime();
	getCPUInterface().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerUp(time);
	}
	getCPU().doReset(time);
	getMixer().unmute();
}

void MSXMotherBoard::schedulePowerDown()
{
	needPowerDown = true;
	getCPU().exitCPULoop();
}

void MSXMotherBoard::doPowerDown(const EmuTime& time)
{
	if (!powered) return;

	powered = false;
	// TODO: This assertion fails in 1 case: when quitting with a running MSX.
	//       How do we want the Reactor to shutdown: immediately or after
	//       handling all pending commands/events/updates?
	//assert(powerSetting.getValue() == powered);
	powerSetting.setValue(false);
	getEventDistributor().distributeEvent(
		new LedEvent(LedEvent::POWER, false));

	getMixer().mute();

	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerDown(time);
	}
}

// Observer<Setting>
void MSXMotherBoard::update(const Setting& setting)
{
	if (&setting == &powerSetting) {
		if (powerSetting.getValue()) {
			powerUp();
		} else {
			schedulePowerDown();
		}
	} else {
		assert(false);
	}
}

MSXDevice* MSXMotherBoard::findDevice(const string& name)
{
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}



ResetCmd::ResetCmd(CommandController& commandController,
                                   MSXMotherBoard& motherBoard_)
	: SimpleCommand(commandController, "reset")
	, motherBoard(motherBoard_)
{
}

string ResetCmd::execute(const vector<string>& /*tokens*/)
{
	motherBoard.scheduleReset();
	return "";
}

string ResetCmd::help(const vector<string>& /*tokens*/) const
{
	return "Resets the MSX.\n";
}

} // namespace openmsx

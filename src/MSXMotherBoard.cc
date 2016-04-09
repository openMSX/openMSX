#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXDevice.hh"
#include "ReverseManager.hh"
#include "HardwareConfig.hh"
#include "ConfigException.hh"
#include "XMLElement.hh"
#include "MSXCliComm.hh"
#include "GlobalCliComm.hh"
#include "MSXCommandController.hh"
#include "Scheduler.hh"
#include "Schedulable.hh"
#include "CartridgeSlotManager.hh"
#include "EventDistributor.hh"
#include "Debugger.hh"
#include "SimpleDebuggable.hh"
#include "MSXMixer.hh"
#include "PluggingController.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "PanasonicMemory.hh"
#include "MSXDeviceSwitch.hh"
#include "MSXMapperIO.hh"
#include "CassettePort.hh"
#include "JoystickPort.hh"
#include "RenShaTurbo.hh"
#include "LedStatus.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "EventDelay.hh"
#include "RealTime.hh"
#include "DeviceFactory.hh"
#include "BooleanSetting.hh"
#include "GlobalSettings.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "InfoTopic.hh"
#include "FileException.hh"
#include "TclObject.hh"
#include "Observer.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "ScopedAssign.hh"
#include "memory.hh"
#include "stl.hh"
#include "unreachable.hh"
#include <cassert>
#include <functional>
#include <iostream>

using std::string;
using std::vector;
using std::unique_ptr;

namespace openmsx {

class AddRemoveUpdate
{
public:
	AddRemoveUpdate(MSXMotherBoard& motherBoard);
	~AddRemoveUpdate();
private:
	MSXMotherBoard& motherBoard;
};

class ResetCmd final : public RecordedCommand
{
public:
	ResetCmd(MSXMotherBoard& motherBoard);
	void execute(array_ref<TclObject> tokens, TclObject& result,
	             EmuTime::param time) override;
	string help(const vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class LoadMachineCmd final : public Command
{
public:
	LoadMachineCmd(MSXMotherBoard& motherBoard);
	void execute(array_ref<TclObject> tokens, TclObject& result) override;
	string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class ListExtCmd final : public Command
{
public:
	ListExtCmd(MSXMotherBoard& motherBoard);
	void execute(array_ref<TclObject> tokens, TclObject& result) override;
	string help(const vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class RemoveExtCmd final : public RecordedCommand
{
public:
	RemoveExtCmd(MSXMotherBoard& motherBoard);
	void execute(array_ref<TclObject> tokens, TclObject& result,
	             EmuTime::param time) override;
	string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineNameInfo final : public InfoTopic
{
public:
	MachineNameInfo(MSXMotherBoard& motherBoard);
	void execute(array_ref<TclObject> tokens,
	             TclObject& result) const override;
	string help(const vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class DeviceInfo final : public InfoTopic
{
public:
	DeviceInfo(MSXMotherBoard& motherBoard);
	void execute(array_ref<TclObject> tokens,
	             TclObject& result) const override;
	string help(const vector<string>& tokens) const override;
	void tabCompletion(vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class FastForwardHelper final : private Schedulable
{
public:
	FastForwardHelper(MSXMotherBoard& msxMotherBoardImpl);
	void setTarget(EmuTime::param targetTime);
private:
	void executeUntil(EmuTime::param time) override;
	MSXMotherBoard& motherBoard;
};

class JoyPortDebuggable final : public SimpleDebuggable
{
public:
	JoyPortDebuggable(MSXMotherBoard& motherBoard);
	byte read(unsigned address, EmuTime::param time) override;
	void write(unsigned address, byte value) override;
};

class SettingObserver final : public Observer<Setting>
{
public:
	SettingObserver(MSXMotherBoard& motherBoard);
	void update(const Setting& setting) override;
private:
	MSXMotherBoard& motherBoard;
};


static unsigned machineIDCounter = 0;

MSXMotherBoard::MSXMotherBoard(Reactor& reactor_)
	: reactor(reactor_)
	, machineID(StringOp::Builder() << "machine" << ++machineIDCounter)
	, mapperIOCounter(0)
	, machineConfig(nullptr)
	, msxCliComm(make_unique<MSXCliComm>(*this, reactor.getGlobalCliComm()))
	, msxEventDistributor(make_unique<MSXEventDistributor>())
	, stateChangeDistributor(make_unique<StateChangeDistributor>())
	, msxCommandController(make_unique<MSXCommandController>(
		reactor.getGlobalCommandController(), reactor,
		*this, *msxEventDistributor, machineID))
	, scheduler(make_unique<Scheduler>())
	, msxMixer(make_unique<MSXMixer>(
		reactor.getMixer(), *this,
		reactor.getGlobalSettings()))
	, videoSourceSetting(*msxCommandController)
	, fastForwardHelper(make_unique<FastForwardHelper>(*this))
	, settingObserver(make_unique<SettingObserver>(*this))
	, powerSetting(reactor.getGlobalSettings().getPowerSetting())
	, powered(false)
	, active(false)
	, fastForwarding(false)
{
	slotManager = make_unique<CartridgeSlotManager>(*this);
	reverseManager = make_unique<ReverseManager>(*this);
	resetCommand = make_unique<ResetCmd>(*this);
	loadMachineCommand = make_unique<LoadMachineCmd>(*this);
	listExtCommand = make_unique<ListExtCmd>(*this);
	extCommand = make_unique<ExtCmd>(*this, "ext");
	removeExtCommand = make_unique<RemoveExtCmd>(*this);
	machineNameInfo = make_unique<MachineNameInfo>(*this);
	deviceInfo = make_unique<DeviceInfo>(*this);
	debugger = make_unique<Debugger>(*this);

	msxMixer->mute(); // powered down

	// Do this before machine-specific settings are created, otherwise
	// a setting-info clicomm message is send with a machine id that hasn't
	// been announced yet over clicomm.
	addRemoveUpdate = make_unique<AddRemoveUpdate>(*this);

	// TODO: Initialization of this field cannot be done much earlier because
	//       EventDelay creates a setting, calling getMSXCliComm()
	//       on MSXMotherBoard, so "pimpl" has to be set up already.
	eventDelay = make_unique<EventDelay>(
		*scheduler, *msxCommandController,
		reactor.getEventDistributor(), *msxEventDistributor,
		*reverseManager);
	realTime = make_unique<RealTime>(
		*this, reactor.getGlobalSettings(), *eventDelay);

	powerSetting.attach(*settingObserver);
}

MSXMotherBoard::~MSXMotherBoard()
{
	powerSetting.detach(*settingObserver);
	deleteMachine();

	assert(mapperIOCounter == 0);
	assert(availableDevices.empty());
	assert(extensions.empty());
	assert(!machineConfig2);
	assert(!getMachineConfig());
}

void MSXMotherBoard::deleteMachine()
{
	while (!extensions.empty()) {
		try {
			removeExtension(*extensions.back());
		} catch (MSXException& e) {
			std::cerr << "Internal error: failed to remove "
			             "extension while deleting a machine: "
			          << e.getMessage() << std::endl;
			UNREACHABLE;
		}
	}

	machineConfig2.reset();
	machineConfig = nullptr;
}

void MSXMotherBoard::setMachineConfig(HardwareConfig* machineConfig_)
{
	assert(!getMachineConfig());
	machineConfig = machineConfig_;

	// make sure the CPU gets instantiated from the main thread
	assert(!msxCpu);
	msxCpu = make_unique<MSXCPU>(*this);
	msxCpuInterface = make_unique<MSXCPUInterface>(*this);
}

bool MSXMotherBoard::isTurboR() const
{
	const HardwareConfig* config = getMachineConfig();
	assert(config);
	return config->getConfig().getChild("devices").findChild("S1990") != nullptr;
}

string MSXMotherBoard::loadMachine(const string& machine)
{
	assert(machineName.empty());
	assert(extensions.empty());
	assert(!machineConfig2);
	assert(!getMachineConfig());

	try {
		machineConfig2 = HardwareConfig::createMachineConfig(*this, machine);
		setMachineConfig(machineConfig2.get());
	} catch (FileException& e) {
		throw MSXException("Machine \"" + machine + "\" not found: " +
		                   e.getMessage());
	} catch (MSXException& e) {
		throw MSXException("Error in \"" + machine + "\" machine: " +
		                   e.getMessage());
	}
	try {
		machineConfig->parseSlots();
		machineConfig->createDevices();
	} catch (MSXException& e) {
		throw MSXException("Error in \"" + machine + "\" machine: " +
		                   e.getMessage());
	}
	if (powerSetting.getBoolean()) {
		powerUp();
	}
	machineName = machine;
	return machineName;
}

string MSXMotherBoard::loadExtension(string_ref name, string_ref slotname)
{
	unique_ptr<HardwareConfig> extension;
	try {
		extension = HardwareConfig::createExtensionConfig(*this, name, slotname);
	} catch (FileException& e) {
		throw MSXException(
			"Extension \"" + name + "\" not found: " + e.getMessage());
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"" + name + "\" extension: " + e.getMessage());
	}
	return insertExtension(name, std::move(extension));
}

string MSXMotherBoard::insertExtension(
	string_ref name, unique_ptr<HardwareConfig> extension)
{
	try {
		extension->parseSlots();
		extension->createDevices();
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"" + name + "\" extension: " + e.getMessage());
	}
	string result = extension->getName();
	extensions.push_back(std::move(extension));
	getMSXCliComm().update(CliComm::EXTENSION, result, "add");
	return result;
}

HardwareConfig* MSXMotherBoard::findExtension(string_ref extensionName)
{
	auto it = std::find_if(begin(extensions), end(extensions),
		[&](Extensions::value_type& v) {
			return v->getName() == extensionName; });
	return (it != end(extensions)) ? it->get() : nullptr;
}

void MSXMotherBoard::removeExtension(const HardwareConfig& extension)
{
	extension.testRemove();
	getMSXCliComm().update(CliComm::EXTENSION, extension.getName(), "remove");
	auto it = rfind_if_unguarded(extensions,
		[&](Extensions::value_type& v) { return v.get() == &extension; });
	extensions.erase(it);
}

CliComm& MSXMotherBoard::getMSXCliComm()
{
	return *msxCliComm;
}

PluggingController& MSXMotherBoard::getPluggingController()
{
	assert(getMachineConfig()); // needed for PluggableFactory::createAll()
	if (!pluggingController) {
		pluggingController = make_unique<PluggingController>(*this);
	}
	return *pluggingController;
}

MSXCPU& MSXMotherBoard::getCPU()
{
	assert(getMachineConfig()); // because CPU needs to know if we're
	                            // emulating turbor or not
	return *msxCpu;
}

MSXCPUInterface& MSXMotherBoard::getCPUInterface()
{
	assert(getMachineConfig());
	return *msxCpuInterface;
}

PanasonicMemory& MSXMotherBoard::getPanasonicMemory()
{
	if (!panasonicMemory) {
		panasonicMemory = make_unique<PanasonicMemory>(*this);
	}
	return *panasonicMemory;
}

MSXDeviceSwitch& MSXMotherBoard::getDeviceSwitch()
{
	if (!deviceSwitch) {
		deviceSwitch = DeviceFactory::createDeviceSwitch(*getMachineConfig());
	}
	return *deviceSwitch;
}

CassettePortInterface& MSXMotherBoard::getCassettePort()
{
	if (!cassettePort) {
		assert(getMachineConfig());
		if (getMachineConfig()->getConfig().findChild("CassettePort")) {
			cassettePort = make_unique<CassettePort>(*getMachineConfig());
		} else {
			cassettePort = make_unique<DummyCassettePort>();
		}
	}
	return *cassettePort;
}

JoystickPortIf& MSXMotherBoard::getJoystickPort(unsigned port)
{
	assert(port < 2);
	if (!joystickPort[0]) {
		assert(getMachineConfig());
		// some MSX machines only have 1 instead of 2 joystick ports
		string_ref ports = getMachineConfig()->getConfig().getChildData(
			"JoystickPorts", "AB");
		if ((ports != "AB") && (ports != "") &&
		    (ports != "A") && (ports != "B")) {
			throw ConfigException(
				"Invalid JoystickPorts specification, "
				"should be one of '', 'A', 'B' or 'AB'.");
		}
		PluggingController& ctrl = getPluggingController();
		if ((ports == "AB") || (ports == "A")) {
			joystickPort[0] = make_unique<JoystickPort>(
				ctrl, "joyporta", "MSX Joystick port A");
		} else {
			joystickPort[0] = make_unique<DummyJoystickPort>();
		}
		if ((ports == "AB") || (ports == "B")) {
			joystickPort[1] = make_unique<JoystickPort>(
				ctrl, "joyportb", "MSX Joystick port B");
		} else {
			joystickPort[1] = make_unique<DummyJoystickPort>();
		}
		joyPortDebuggable = make_unique<JoyPortDebuggable>(*this);
	}
	return *joystickPort[port];
}

RenShaTurbo& MSXMotherBoard::getRenShaTurbo()
{
	if (!renShaTurbo) {
		assert(getMachineConfig());
		renShaTurbo = make_unique<RenShaTurbo>(
			*msxCommandController,
			getMachineConfig()->getConfig());
	}
	return *renShaTurbo;
}

LedStatus& MSXMotherBoard::getLedStatus()
{
	if (!ledStatus) {
		getMSXCliComm(); // force init, to be on the safe side
		ledStatus = make_unique<LedStatus>(
			reactor.getRTScheduler(),
			*msxCommandController,
			*msxCliComm);
	}
	return *ledStatus;
}

CommandController& MSXMotherBoard::getCommandController()
{
	return *msxCommandController;
}

InfoCommand& MSXMotherBoard::getMachineInfoCommand()
{
	return msxCommandController->getMachineInfoCommand();
}

EmuTime::param MSXMotherBoard::getCurrentTime()
{
	return scheduler->getCurrentTime();
}

bool MSXMotherBoard::execute()
{
	if (!powered) {
		return false;
	}
	assert(getMachineConfig()); // otherwise powered cannot be true

	getCPU().execute(false);
	return true;
}

void MSXMotherBoard::fastForward(EmuTime::param time, bool fast)
{
	assert(powered);
	assert(getMachineConfig());

	if (time <= getCurrentTime()) return;

	ScopedAssign<bool> sa(fastForwarding, fast);
	realTime->disable();
	msxMixer->mute();
	fastForwardHelper->setTarget(time);
	while (time > getCurrentTime()) {
		// note: this can run (slightly) past the requested time
		getCPU().execute(true); // fast-forward mode
	}
	realTime->enable();
	msxMixer->unmute();
}

void MSXMotherBoard::pause()
{
	if (getMachineConfig()) {
		getCPU().setPaused(true);
	}
	msxMixer->mute();
}

void MSXMotherBoard::unpause()
{
	if (getMachineConfig()) {
		getCPU().setPaused(false);
	}
	msxMixer->unmute();
}

void MSXMotherBoard::addDevice(MSXDevice& device)
{
	availableDevices.push_back(&device);
}

void MSXMotherBoard::removeDevice(MSXDevice& device)
{
	move_pop_back(availableDevices, rfind_unguarded(availableDevices, &device));
}

void MSXMotherBoard::doReset()
{
	if (!powered) return;
	assert(getMachineConfig());

	EmuTime::param time = getCurrentTime();
	getCPUInterface().reset();
	for (auto& d : availableDevices) {
		d->reset(time);
	}
	getCPU().doReset(time);
	// let everyone know we're booting, note that the fact that this is
	// done after the reset call to the devices is arbitrary here
	reactor.getEventDistributor().distributeEvent(
		std::make_shared<SimpleEvent>(OPENMSX_BOOT_EVENT));
}

byte MSXMotherBoard::readIRQVector()
{
	byte result = 0xff;
	for (auto& d : availableDevices) {
		result &= d->readIRQVector();
	}
	return result;
}

void MSXMotherBoard::powerUp()
{
	if (powered) return;
	if (!getMachineConfig()) return;

	powered = true;
	// TODO: If our "powered" field is always equal to the power setting,
	//       there is not really a point in keeping it.
	// TODO: assert disabled see note in Reactor::run() where this method
	//       is called
	//assert(powerSetting.getBoolean() == powered);
	powerSetting.setBoolean(true);
	// TODO: We could make the power LED a device, so we don't have to handle
	//       it separately here.
	getLedStatus().setLed(LedStatus::POWER, true);

	EmuTime::param time = getCurrentTime();
	getCPUInterface().reset();
	for (auto& d : availableDevices) {
		d->powerUp(time);
	}
	getCPU().doReset(time);
	msxMixer->unmute();
	// let everyone know we're booting, note that the fact that this is
	// done after the reset call to the devices is arbitrary here
	reactor.getEventDistributor().distributeEvent(
		std::make_shared<SimpleEvent>(OPENMSX_BOOT_EVENT));
}

void MSXMotherBoard::powerDown()
{
	if (!powered) return;

	powered = false;
	// TODO: This assertion fails in 1 case: when quitting with a running MSX.
	//       How do we want the Reactor to shutdown: immediately or after
	//       handling all pending commands/events/updates?
	//assert(powerSetting.getBoolean() == powered);
	powerSetting.setBoolean(false);
	getLedStatus().setLed(LedStatus::POWER, false);

	msxMixer->mute();

	EmuTime::param time = getCurrentTime();
	for (auto& d : availableDevices) {
		d->powerDown(time);
	}
}

void MSXMotherBoard::activate(bool active_)
{
	active = active_;
	auto event = std::make_shared<SimpleEvent>(
		active ? OPENMSX_MACHINE_ACTIVATED : OPENMSX_MACHINE_DEACTIVATED);
	msxEventDistributor->distributeEvent(event, scheduler->getCurrentTime());
	if (active) {
		realTime->resync();
	}
}

void MSXMotherBoard::exitCPULoopAsync()
{
	if (getMachineConfig()) {
		getCPU().exitCPULoopAsync();
	}
}

void MSXMotherBoard::exitCPULoopSync()
{
	getCPU().exitCPULoopSync();
}

MSXDevice* MSXMotherBoard::findDevice(string_ref name)
{
	for (auto& d : availableDevices) {
		if (d->getName() == name) {
			return d;
		}
	}
	return nullptr;
}

MSXMapperIO* MSXMotherBoard::createMapperIO()
{
	if (mapperIOCounter == 0) {
		mapperIO = DeviceFactory::createMapperIO(*getMachineConfig());

		MSXCPUInterface& cpuInterface = getCPUInterface();
		cpuInterface.register_IO_Out(0xFC, mapperIO.get());
		cpuInterface.register_IO_Out(0xFD, mapperIO.get());
		cpuInterface.register_IO_Out(0xFE, mapperIO.get());
		cpuInterface.register_IO_Out(0xFF, mapperIO.get());
		cpuInterface.register_IO_In (0xFC, mapperIO.get());
		cpuInterface.register_IO_In (0xFD, mapperIO.get());
		cpuInterface.register_IO_In (0xFE, mapperIO.get());
		cpuInterface.register_IO_In (0xFF, mapperIO.get());
	}
	++mapperIOCounter;
	return mapperIO.get();
}

void MSXMotherBoard::destroyMapperIO()
{
	assert(mapperIO);
	assert(mapperIOCounter);
	--mapperIOCounter;
	if (mapperIOCounter == 0) {
		MSXCPUInterface& cpuInterface = getCPUInterface();
		cpuInterface.unregister_IO_Out(0xFC, mapperIO.get());
		cpuInterface.unregister_IO_Out(0xFD, mapperIO.get());
		cpuInterface.unregister_IO_Out(0xFE, mapperIO.get());
		cpuInterface.unregister_IO_Out(0xFF, mapperIO.get());
		cpuInterface.unregister_IO_In (0xFC, mapperIO.get());
		cpuInterface.unregister_IO_In (0xFD, mapperIO.get());
		cpuInterface.unregister_IO_In (0xFE, mapperIO.get());
		cpuInterface.unregister_IO_In (0xFF, mapperIO.get());

		mapperIO.reset();
	}
}

string MSXMotherBoard::getUserName(const string& hwName)
{
	auto& s = userNames[hwName];
	unsigned n = 0;
	string userName;
	do {
		userName = StringOp::Builder() << "untitled" << ++n;
	} while (find(begin(s), end(s), userName) != end(s));
	s.push_back(userName);
	return userName;
}

void MSXMotherBoard::freeUserName(const string& hwName, const string& userName)
{
	auto& s = userNames[hwName];
	move_pop_back(s, rfind_unguarded(s, userName));
}

// AddRemoveUpdate

AddRemoveUpdate::AddRemoveUpdate(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
	motherBoard.getReactor().getGlobalCliComm().update(
		CliComm::HARDWARE, motherBoard.getMachineID(), "add");
}

AddRemoveUpdate::~AddRemoveUpdate()
{
	motherBoard.getReactor().getGlobalCliComm().update(
		CliComm::HARDWARE, motherBoard.getMachineID(), "remove");
}


// ResetCmd
ResetCmd::ResetCmd(MSXMotherBoard& motherBoard_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  "reset")
	, motherBoard(motherBoard_)
{
}

void ResetCmd::execute(array_ref<TclObject> /*tokens*/, TclObject& /*result*/,
                       EmuTime::param /*time*/)
{
	motherBoard.doReset();
}

string ResetCmd::help(const vector<string>& /*tokens*/) const
{
	return "Resets the MSX.";
}


// LoadMachineCmd
LoadMachineCmd::LoadMachineCmd(MSXMotherBoard& motherBoard_)
	: Command(motherBoard_.getCommandController(), "load_machine")
	, motherBoard(motherBoard_)
{
	// The load_machine command should always directly follow a
	// create_machine command:
	// - It's not allowed to use load_machine on a machine that has
	//   already a machine configuration loaded earlier.
	// - We also disallow executing most machine-specifc commands on an
	//   'empty machine' (an 'empty machine', is a machine returned by
	//   create_machine before the load_machine command is executed, so a
	//   machine without a machine configuration). The only exception is
	//   this load_machine command and machine_info.
	//
	// So if the only allowed command on an empty machine is
	// 'load_machine', (and an empty machine by itself isn't very useful),
	// then why isn't create_machine and load_machine merged into a single
	// command? The only reason for this is that load_machine sends out
	// events (machine specific) and maybe you already want to know the ID
	// of the new machine (this is returned by create_machine) before those
	// events will be send.
	//
	// Why not allow all commands on an empty machine? In the past we did
	// allow this, though it often was the source of bugs. We could in each
	// command (when needed) check for an empty machine and then return
	// some dummy/empty result or some error. But because I can't think of
	// any really useful command for an empty machine, it seemed easier to
	// just disallow most commands.
	setAllowedInEmptyMachine(true);
}

void LoadMachineCmd::execute(array_ref<TclObject> tokens, TclObject& result)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	if (motherBoard.getMachineConfig()) {
		throw CommandException("Already loaded a config in this machine.");
	}
	result.setString(motherBoard.loadMachine(tokens[1].getString().str()));
}

string LoadMachineCmd::help(const vector<string>& /*tokens*/) const
{
	return "Load a msx machine configuration into an empty machine.";
}

void LoadMachineCmd::tabCompletion(vector<string>& tokens) const
{
	auto machines = Reactor::getHwConfigs("machines");
	completeString(tokens, machines);
}


// ListExtCmd
ListExtCmd::ListExtCmd(MSXMotherBoard& motherBoard_)
	: Command(motherBoard_.getCommandController(), "list_extensions")
	, motherBoard(motherBoard_)
{
}

void ListExtCmd::execute(array_ref<TclObject> /*tokens*/, TclObject& result)
{
	for (auto& e : motherBoard.getExtensions()) {
		result.addListElement(e->getName());
	}
}

string ListExtCmd::help(const vector<string>& /*tokens*/) const
{
	return "Return a list of all inserted extensions.";
}


// ExtCmd
ExtCmd::ExtCmd(MSXMotherBoard& motherBoard_, string_ref commandName_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  commandName_)
	, motherBoard(motherBoard_)
	, commandName(commandName_.str())
{
}

void ExtCmd::execute(array_ref<TclObject> tokens, TclObject& result,
                     EmuTime::param /*time*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	try {
		auto slotname = (commandName.size() == 4)
			? string(1, commandName[3])
			: "any";
		result.setString(motherBoard.loadExtension(
			tokens[1].getString(), slotname));
	} catch (MSXException& e) {
		throw CommandException(e.getMessage());
	}
}

string ExtCmd::help(const vector<string>& /*tokens*/) const
{
	return "Insert a hardware extension.";
}

void ExtCmd::tabCompletion(vector<string>& tokens) const
{
	auto extensions = Reactor::getHwConfigs("extensions");
	completeString(tokens, extensions);
}


// RemoveExtCmd
RemoveExtCmd::RemoveExtCmd(MSXMotherBoard& motherBoard_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  "remove_extension")
	, motherBoard(motherBoard_)
{
}

void RemoveExtCmd::execute(array_ref<TclObject> tokens, TclObject& /*result*/,
                           EmuTime::param /*time*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	string_ref extName = tokens[1].getString();
	HardwareConfig* extension = motherBoard.findExtension(extName);
	if (!extension) {
		throw CommandException("No such extension: " + extName);
	}
	try {
		motherBoard.removeExtension(*extension);
	} catch (MSXException& e) {
		throw CommandException("Can't remove extension '" + extName +
		                       "': " + e.getMessage());
	}
}

string RemoveExtCmd::help(const vector<string>& /*tokens*/) const
{
	return "Remove an extension from the MSX machine.";
}

void RemoveExtCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		vector<string_ref> names;
		for (auto& e : motherBoard.getExtensions()) {
			names.push_back(e->getName());
		}
		completeString(tokens, names);
	}
}


// MachineNameInfo

MachineNameInfo::MachineNameInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "config_name")
	, motherBoard(motherBoard_)
{
}

void MachineNameInfo::execute(array_ref<TclObject> /*tokens*/,
                              TclObject& result) const
{
	result.setString(motherBoard.getMachineName());
}

string MachineNameInfo::help(const vector<string>& /*tokens*/) const
{
	return "Returns the configuration name for this machine.";
}


// DeviceInfo

DeviceInfo::DeviceInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "device")
	, motherBoard(motherBoard_)
{
}

void DeviceInfo::execute(array_ref<TclObject> tokens, TclObject& result) const
{
	switch (tokens.size()) {
	case 2:
		for (auto& d : motherBoard.availableDevices) {
			result.addListElement(d->getName());
		}
		break;
	case 3: {
		string_ref deviceName = tokens[2].getString();
		MSXDevice* device = motherBoard.findDevice(deviceName);
		if (!device) {
			throw CommandException("No such device: " + deviceName);
		}
		device->getDeviceInfo(result);
		break;
	}
	default:
		throw SyntaxError();
	}
}

string DeviceInfo::help(const vector<string>& /*tokens*/) const
{
	return "Without any arguments, returns the list of used device names.\n"
	       "With a device name as argument, returns the type (and for some "
	       "devices the subtype) of the given device.";
}

void DeviceInfo::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		vector<string> names;
		for (auto& d : motherBoard.availableDevices) {
			names.push_back(d->getName());
		}
		completeString(tokens, names);
	}
}


// FastForwardHelper

FastForwardHelper::FastForwardHelper(MSXMotherBoard& motherBoard_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
{
}

void FastForwardHelper::setTarget(EmuTime::param targetTime)
{
	setSyncPoint(targetTime);
}

void FastForwardHelper::executeUntil(EmuTime::param /*time*/)
{
	motherBoard.exitCPULoopSync();
}


// class JoyPortDebuggable

JoyPortDebuggable::JoyPortDebuggable(MSXMotherBoard& motherBoard_)
	: SimpleDebuggable(motherBoard_, "joystickports", "MSX Joystick Ports", 2)
{
}

byte JoyPortDebuggable::read(unsigned address, EmuTime::param time)
{
	return getMotherBoard().getJoystickPort(address).read(time);
}

void JoyPortDebuggable::write(unsigned /*address*/, byte /*value*/)
{
	// ignore
}


// class SettingObserver

SettingObserver::SettingObserver(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
}

void SettingObserver::update(const Setting& setting)
{
	if (&setting == &motherBoard.powerSetting) {
		if (motherBoard.powerSetting.getBoolean()) {
			motherBoard.powerUp();
		} else {
			motherBoard.powerDown();
		}
	} else {
		UNREACHABLE;
	}
}


// serialize
// version 1: initial version
// version 2: added reRecordCount
// version 3: removed reRecordCount (moved to ReverseManager)
// version 4: moved joystickportA/B from MSXPSG to here
template<typename Archive>
void MSXMotherBoard::serialize(Archive& ar, unsigned version)
{
	// don't serialize:
	//    machineID, userNames, availableDevices, addRemoveUpdate,
	//    sharedStuffMap, msxCliComm, msxEventDistributor,
	//    msxCommandController, slotManager, eventDelay,
	//    debugger, msxMixer, panasonicMemory, renShaTurbo,
	//    ledStatus

	// Scheduler must come early so that devices can query current time
	ar.serialize("scheduler", *scheduler);
	// MSXMixer has already set syncpoints, those are invalid now
	// the following call will fix this
	if (ar.isLoader()) {
		msxMixer->reInit();
	}

	ar.serialize("name", machineName);
	ar.serializeWithID("config", machineConfig2, std::ref(*this));
	assert(getMachineConfig() == machineConfig2.get());
	ar.serializeWithID("extensions", extensions, std::ref(*this));

	if (mapperIO) ar.serialize("mapperIO", *mapperIO);

	MSXDeviceSwitch& devSwitch = getDeviceSwitch();
	if (devSwitch.hasRegisteredDevices()) {
		ar.serialize("deviceSwitch", devSwitch);
	}

	if (getMachineConfig()) {
		ar.serialize("cpu", getCPU());
	}
	ar.serialize("cpuInterface", getCPUInterface());

	if (auto port = dynamic_cast<CassettePort*>(&getCassettePort())) {
		ar.serialize("cassetteport", *port);
	}
	if (ar.versionAtLeast(version, 4)) {
		if (auto port = dynamic_cast<JoystickPort*>(
				joystickPort[0].get())) {
			ar.serialize("joystickportA", *port);
		}
		if (auto port = dynamic_cast<JoystickPort*>(
				joystickPort[1].get())) {
			ar.serialize("joystickportB", *port);
		}
	}

	if (ar.isLoader()) {
		powered = true; // must come before changing power setting
		powerSetting.setBoolean(true);
		getLedStatus().setLed(LedStatus::POWER, true);
		msxMixer->unmute();
	}

	if (version == 2) {
		assert(ar.isLoader());
		unsigned reRecordCount = 0; // silence warning
		ar.serialize("reRecordCount", reRecordCount);
		getReverseManager().setReRecordCount(reRecordCount);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMotherBoard)

} // namespace openmsx

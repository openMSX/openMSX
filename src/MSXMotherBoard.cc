#include "MSXMotherBoard.hh"

#include "BooleanSetting.hh"
#include "CartridgeSlotManager.hh"
#include "CassettePort.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "ConfigException.hh"
#include "Debugger.hh"
#include "DeviceFactory.hh"
#include "EventDelay.hh"
#include "EventDistributor.hh"
#include "FileException.hh"
#include "GlobalCliComm.hh"
#include "GlobalSettings.hh"
#include "HardwareConfig.hh"
#include "InfoTopic.hh"
#include "JoystickPort.hh"
#include "LedStatus.hh"
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXCliComm.hh"
#include "MSXCommandController.hh"
#include "MSXDevice.hh"
#include "MSXDeviceSwitch.hh"
#include "MSXEventDistributor.hh"
#include "MSXMapperIO.hh"
#include "MSXMixer.hh"
#include "Observer.hh"
#include "PanasonicMemory.hh"
#include "PluggingController.hh"
#include "Reactor.hh"
#include "RealTime.hh"
#include "RenShaTurbo.hh"
#include "ReverseManager.hh"
#include "Schedulable.hh"
#include "Scheduler.hh"
#include "SimpleDebuggable.hh"
#include "StateChangeDistributor.hh"
#include "TclObject.hh"
#include "XMLElement.hh"
#include "serialize.hh"
#include "serialize_stl.hh"

#include "ScopedAssign.hh"
#include "one_of.hh"
#include "ranges.hh"
#include "stl.hh"
#include "unreachable.hh"
#include "view.hh"

#include <cassert>
#include <functional>
#include <iostream>
#include <memory>

using std::make_unique;
using std::string;

namespace openmsx {

class AddRemoveUpdate
{
public:
	explicit AddRemoveUpdate(MSXMotherBoard& motherBoard);
	AddRemoveUpdate(const AddRemoveUpdate&) = delete;
	AddRemoveUpdate(AddRemoveUpdate&&) = delete;
	AddRemoveUpdate& operator=(const AddRemoveUpdate&) = delete;
	AddRemoveUpdate& operator=(AddRemoveUpdate&&) = delete;
	~AddRemoveUpdate();
private:
	MSXMotherBoard& motherBoard;
};

class ResetCmd final : public RecordedCommand
{
public:
	explicit ResetCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result,
	             EmuTime::param time) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class LoadMachineCmd final : public Command
{
public:
	explicit LoadMachineCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class ListExtCmd final : public Command
{
public:
	explicit ListExtCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class RemoveExtCmd final : public RecordedCommand
{
public:
	explicit RemoveExtCmd(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens, TclObject& result,
	             EmuTime::param time) override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineNameInfo final : public InfoTopic
{
public:
	explicit MachineNameInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineTypeInfo final : public InfoTopic
{
public:
	explicit MachineTypeInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineExtensionInfo final : public InfoTopic
{
public:
	explicit MachineExtensionInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class MachineMediaInfo final : public InfoTopic
{
public:
	explicit MachineMediaInfo(MSXMotherBoard& motherBoard);
	~MachineMediaInfo() {
		assert(providers.empty());
	}
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<string>& tokens) const override;
	void registerProvider(std::string_view name, MediaInfoProvider& provider);
	void unregisterProvider(MediaInfoProvider& provider);
private:
	struct ProviderInfo {
		ProviderInfo(std::string_view n, MediaInfoProvider* p)
			: name(n), provider(p) {} // clang-15 workaround
		std::string_view name;
		MediaInfoProvider* provider;
	};
	// There will only be a handful of providers, use an unsorted vector.
	std::vector<ProviderInfo> providers;
};

class DeviceInfo final : public InfoTopic
{
public:
	explicit DeviceInfo(MSXMotherBoard& motherBoard);
	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
};

class FastForwardHelper final : private Schedulable
{
public:
	explicit FastForwardHelper(MSXMotherBoard& motherBoard);
	void setTarget(EmuTime::param targetTime);
private:
	void executeUntil(EmuTime::param time) override;
	MSXMotherBoard& motherBoard;
};

class JoyPortDebuggable final : public SimpleDebuggable
{
public:
	explicit JoyPortDebuggable(MSXMotherBoard& motherBoard);
	[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
	void write(unsigned address, byte value) override;
};

class SettingObserver final : public Observer<Setting>
{
public:
	explicit SettingObserver(MSXMotherBoard& motherBoard);
	void update(const Setting& setting) noexcept override;
private:
	MSXMotherBoard& motherBoard;
};


static unsigned machineIDCounter = 0;

MSXMotherBoard::MSXMotherBoard(Reactor& reactor_)
	: reactor(reactor_)
	, machineID(strCat("machine", ++machineIDCounter))
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
	, suppressMessagesSetting(*msxCommandController, "suppressmessages",
		"Suppress info, warning and error messages for this machine. "
		"Intended use is for scripts that create temporary machines "
		"of which you don't want to see warning messages about blank "
		"SRAM content or PSG port directions for instance.",
		false, Setting::Save::NO)
	, fastForwardHelper(make_unique<FastForwardHelper>(*this))
	, settingObserver(make_unique<SettingObserver>(*this))
	, powerSetting(reactor.getGlobalSettings().getPowerSetting())
{
	slotManager = make_unique<CartridgeSlotManager>(*this);
	reverseManager = make_unique<ReverseManager>(*this);
	resetCommand = make_unique<ResetCmd>(*this);
	loadMachineCommand = make_unique<LoadMachineCmd>(*this);
	listExtCommand = make_unique<ListExtCmd>(*this);
	extCommand = make_unique<ExtCmd>(*this, "ext");
	removeExtCommand = make_unique<RemoveExtCmd>(*this);
	machineNameInfo = make_unique<MachineNameInfo>(*this);
	machineTypeInfo = make_unique<MachineTypeInfo>(*this);
	machineExtensionInfo = make_unique<MachineExtensionInfo>(*this);
	machineMediaInfo = make_unique<MachineMediaInfo>(*this);
	deviceInfo = make_unique<DeviceInfo>(*this);
	debugger = make_unique<Debugger>(*this);

	// Do this before machine-specific settings are created, otherwise
	// a setting-info CliComm message is send with a machine id that hasn't
	// been announced yet over CliComm.
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
	suppressMessagesSetting.attach(*settingObserver);
}

MSXMotherBoard::~MSXMotherBoard()
{
	suppressMessagesSetting.detach(*settingObserver);
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
			          << e.getMessage() << '\n';
			assert(false);
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

std::string_view MSXMotherBoard::getMachineType() const
{
	if (const HardwareConfig* machine = getMachineConfig()) {
		if (const auto* info = machine->getConfig().findChild("info")) {
			if (const auto* type = info->findChild("type")) {
				return type->getData();
			}
		}
	}
	return "";
}

bool MSXMotherBoard::isTurboR() const
{
	const HardwareConfig* config = getMachineConfig();
	assert(config);
	return config->getConfig().getChild("devices").findChild("S1990") != nullptr;
}

bool MSXMotherBoard::hasToshibaEngine() const
{
	const HardwareConfig* config = getMachineConfig();
	assert(config);
	const XMLElement& devices = config->getConfig().getChild("devices");
	return devices.findChild("T7775") != nullptr ||
	       devices.findChild("T7937") != nullptr ||
		   devices.findChild("T9763") != nullptr ||
		   devices.findChild("T9769") != nullptr;
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
		throw MSXException("Machine \"", machine, "\" not found: ",
		                   e.getMessage());
	} catch (MSXException& e) {
		throw MSXException("Error in \"", machine, "\" machine: ",
		                   e.getMessage());
	}
	try {
		machineConfig->parseSlots();
		machineConfig->createDevices();
	} catch (MSXException& e) {
		throw MSXException("Error in \"", machine, "\" machine: ",
		                   e.getMessage());
	}
	if (powerSetting.getBoolean()) {
		powerUp();
	}
	machineName = machine;
	return machineName;
}

std::unique_ptr<HardwareConfig> MSXMotherBoard::loadExtension(std::string_view name, std::string_view slotName)
{
	try {
		return HardwareConfig::createExtensionConfig(
			*this, string(name), slotName);
	} catch (FileException& e) {
		throw MSXException(
			"Extension \"", name, "\" not found: ", e.getMessage());
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"", name, "\" extension: ", e.getMessage());
	}
}

string MSXMotherBoard::insertExtension(
	std::string_view name, std::unique_ptr<HardwareConfig> extension)
{
	try {
		extension->parseSlots();
		extension->createDevices();
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"", name, "\" extension: ", e.getMessage());
	}
	string result = extension->getName();
	extensions.push_back(std::move(extension));
	getMSXCliComm().update(CliComm::UpdateType::EXTENSION, result, "add");
	return result;
}

HardwareConfig* MSXMotherBoard::findExtension(std::string_view extensionName)
{
	auto it = ranges::find(extensions, extensionName, &HardwareConfig::getName);
	return (it != end(extensions)) ? it->get() : nullptr;
}

void MSXMotherBoard::removeExtension(const HardwareConfig& extension)
{
	extension.testRemove();
	getMSXCliComm().update(CliComm::UpdateType::EXTENSION, extension.getName(), "remove");
	auto it = rfind_unguarded(extensions, &extension,
	                          [](auto& e) { return e.get(); });
	extensions.erase(it);
}

MSXCliComm& MSXMotherBoard::getMSXCliComm()
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
			cassettePort = make_unique<DummyCassettePort>(*this);
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
		std::string_view ports = getMachineConfig()->getConfig().getChildData(
			"JoystickPorts", "AB");
		if (ports != one_of("AB", "", "A", "B")) {
			throw ConfigException(
				"Invalid JoystickPorts specification, "
				"should be one of '', 'A', 'B' or 'AB'.");
		}
		PluggingController& ctrl = getPluggingController();
		if (ports == one_of("AB", "A")) {
			joystickPort[0] = make_unique<JoystickPort>(
				ctrl, "joyporta", "MSX Joystick port A");
		} else {
			joystickPort[0] = make_unique<DummyJoystickPort>();
		}
		if (ports == one_of("AB", "B")) {
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
			*this,
			getMachineConfig()->getConfig());
	}
	return *renShaTurbo;
}

LedStatus& MSXMotherBoard::getLedStatus()
{
	if (!ledStatus) {
		(void)getMSXCliComm(); // force init, to be on the safe side
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

EmuTime::param MSXMotherBoard::getCurrentTime() const
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

	ScopedAssign sa(fastForwarding, fast);
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
	reactor.getEventDistributor().distributeEvent(BootEvent());
}

byte MSXMotherBoard::readIRQVector() const
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
	reactor.getEventDistributor().distributeEvent(BootEvent());
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
	auto event = active ? Event(MachineActivatedEvent())
	                    : Event(MachineDeactivatedEvent());
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

MSXDevice* MSXMotherBoard::findDevice(std::string_view name)
{
	auto it = ranges::find(availableDevices, name, &MSXDevice::getName);
	return (it != end(availableDevices)) ? *it : nullptr;
}

MSXMapperIO& MSXMotherBoard::createMapperIO()
{
	if (mapperIOCounter == 0) {
		mapperIO = DeviceFactory::createMapperIO(*getMachineConfig());

		MSXCPUInterface& cpuInterface = getCPUInterface();
		for (auto port : {0xfc, 0xfd, 0xfe, 0xff}) {
			cpuInterface.register_IO_Out(narrow_cast<byte>(port), mapperIO.get());
			cpuInterface.register_IO_In (narrow_cast<byte>(port), mapperIO.get());
		}
	}
	++mapperIOCounter;
	return *mapperIO;
}

void MSXMotherBoard::destroyMapperIO()
{
	assert(mapperIO);
	assert(mapperIOCounter);
	--mapperIOCounter;
	if (mapperIOCounter == 0) {
		MSXCPUInterface& cpuInterface = getCPUInterface();
		for (auto port : {0xfc, 0xfd, 0xfe, 0xff}) {
			cpuInterface.unregister_IO_Out(narrow_cast<byte>(port), mapperIO.get());
			cpuInterface.unregister_IO_In (narrow_cast<byte>(port), mapperIO.get());
		}

		mapperIO.reset();
	}
}

string MSXMotherBoard::getUserName(const string& hwName)
{
	auto& s = userNames[hwName];
	unsigned n = 0;
	string userName;
	do {
		userName = strCat("untitled", ++n);
	} while (contains(s, userName));
	s.push_back(userName);
	return userName;
}

void MSXMotherBoard::freeUserName(const string& hwName, const string& userName)
{
	auto& s = userNames[hwName];
	move_pop_back(s, rfind_unguarded(s, userName));
}

void MSXMotherBoard::registerMediaInfo(std::string_view name, MediaInfoProvider& provider)
{
	machineMediaInfo->registerProvider(name, provider);
}

void MSXMotherBoard::unregisterMediaInfo(MediaInfoProvider& provider)
{
	machineMediaInfo->unregisterProvider(provider);
}


// AddRemoveUpdate

AddRemoveUpdate::AddRemoveUpdate(MSXMotherBoard& motherBoard_)
	: motherBoard(motherBoard_)
{
	motherBoard.getReactor().getGlobalCliComm().update(
		CliComm::UpdateType::HARDWARE, motherBoard.getMachineID(), "add");
}

AddRemoveUpdate::~AddRemoveUpdate()
{
	motherBoard.getReactor().getGlobalCliComm().update(
		CliComm::UpdateType::HARDWARE, motherBoard.getMachineID(), "remove");
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

void ResetCmd::execute(std::span<const TclObject> /*tokens*/, TclObject& /*result*/,
                       EmuTime::param /*time*/)
{
	motherBoard.doReset();
}

string ResetCmd::help(std::span<const TclObject> /*tokens*/) const
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
	// - We also disallow executing most machine-specific commands on an
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

void LoadMachineCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	checkNumArgs(tokens, 2, "machine");
	if (motherBoard.getMachineConfig()) {
		throw CommandException("Already loaded a config in this machine.");
	}
	result = motherBoard.loadMachine(string(tokens[1].getString()));
}

string LoadMachineCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Load a msx machine configuration into an empty machine.";
}

void LoadMachineCmd::tabCompletion(std::vector<string>& tokens) const
{
	completeString(tokens, Reactor::getHwConfigs("machines"));
}


// ListExtCmd
ListExtCmd::ListExtCmd(MSXMotherBoard& motherBoard_)
	: Command(motherBoard_.getCommandController(), "list_extensions")
	, motherBoard(motherBoard_)
{
}

void ListExtCmd::execute(std::span<const TclObject> /*tokens*/, TclObject& result)
{
	result.addListElements(
		view::transform(motherBoard.getExtensions(), &HardwareConfig::getName));
}

string ListExtCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Return a list of all inserted extensions.";
}


// ExtCmd
ExtCmd::ExtCmd(MSXMotherBoard& motherBoard_, std::string commandName_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getStateChangeDistributor(),
	                  motherBoard_.getScheduler(),
	                  commandName_)
	, motherBoard(motherBoard_)
	, commandName(std::move(commandName_))
{
}

void ExtCmd::execute(std::span<const TclObject> tokens, TclObject& result,
                     EmuTime::param /*time*/)
{
	checkNumArgs(tokens, Between{2, 3}, "extension");
	if (tokens.size() == 3 && tokens[1].getString() != "insert") {
		throw SyntaxError();
	}
	try {
		auto name = tokens[tokens.size() - 1].getString();
		auto slotName = (commandName.size() == 4)
			? std::string_view(&commandName[3], 1)
			: "any";
		auto extension = motherBoard.loadExtension(name, slotName);
		if (slotName != "any") {
			const auto& manager = motherBoard.getSlotManager();
			if (const auto* extConf = manager.getConfigForSlot(commandName[3] - 'a')) {
				// still a cartridge inserted, (try to) remove it now
				motherBoard.removeExtension(*extConf);
			}
		}
		result = motherBoard.insertExtension(name, std::move(extension));
	} catch (MSXException& e) {
		throw CommandException(std::move(e).getMessage());
	}
}

string ExtCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Insert a hardware extension.";
}

void ExtCmd::tabCompletion(std::vector<string>& tokens) const
{
	completeString(tokens, Reactor::getHwConfigs("extensions"));
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

void RemoveExtCmd::execute(std::span<const TclObject> tokens, TclObject& /*result*/,
                           EmuTime::param /*time*/)
{
	checkNumArgs(tokens, 2, "extension");
	std::string_view extName = tokens[1].getString();
	HardwareConfig* extension = motherBoard.findExtension(extName);
	if (!extension) {
		throw CommandException("No such extension: ", extName);
	}
	try {
		motherBoard.removeExtension(*extension);
	} catch (MSXException& e) {
		throw CommandException("Can't remove extension '", extName,
		                       "': ", e.getMessage());
	}
}

string RemoveExtCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "Remove an extension from the MSX machine.";
}

void RemoveExtCmd::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		completeString(tokens, view::transform(
			motherBoard.getExtensions(),
			[](auto& e) -> std::string_view { return e->getName(); }));
	}
}


// MachineNameInfo

MachineNameInfo::MachineNameInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "config_name")
	, motherBoard(motherBoard_)
{
}

void MachineNameInfo::execute(std::span<const TclObject> /*tokens*/,
                              TclObject& result) const
{
	result = motherBoard.getMachineName();
}

string MachineNameInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the configuration name for this machine.";
}

// MachineTypeInfo

MachineTypeInfo::MachineTypeInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "type")
	, motherBoard(motherBoard_)
{
}

void MachineTypeInfo::execute(std::span<const TclObject> /*tokens*/,
                              TclObject& result) const
{
	result = motherBoard.getMachineType();
}

string MachineTypeInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns the machine type for this machine.";
}


// MachineExtensionInfo

MachineExtensionInfo::MachineExtensionInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "extension")
	, motherBoard(motherBoard_)
{
}

void MachineExtensionInfo::execute(std::span<const TclObject> tokens,
                              TclObject& result) const
{
	checkNumArgs(tokens, Between{2, 3}, Prefix{2}, "?extension-instance-name?");
	if (tokens.size() == 2) {
		result.addListElements(
			view::transform(motherBoard.getExtensions(), &HardwareConfig::getName));
	} else if (tokens.size() == 3) {
		std::string_view extName = tokens[2].getString();
		const HardwareConfig* extension = motherBoard.findExtension(extName);
		if (!extension) {
			throw CommandException("No such extension: ", extName);
		}
		if (extension->getType() == HardwareConfig::Type::EXTENSION) {
			// A 'true' extension, as specified in an XML file
			result.addDictKeyValue("config", extension->getConfigName());
		} else {
			assert(extension->getType() == HardwareConfig::Type::ROM);
			// A ROM cartridge, peek into the internal config for the original filename
			const auto& filename = extension->getConfig()
				.getChild("devices").getChild("primary").getChild("secondary")
				.getChild("ROM").getChild("rom").getChildData("filename");
			result.addDictKeyValue("rom", filename);
		}
		TclObject deviceList;
		deviceList.addListElements(
			view::transform(extension->getDevices(), &MSXDevice::getName));
		result.addDictKeyValue("devices", deviceList);
	}
}

string MachineExtensionInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns information about the given extension instance.";
}

void MachineExtensionInfo::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, view::transform(
			motherBoard.getExtensions(),
			[](auto& e) -> std::string_view { return e->getName(); }));
	}
}


// MachineMediaInfo

MachineMediaInfo::MachineMediaInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "media")
{
}

void MachineMediaInfo::execute(std::span<const TclObject> tokens,
                              TclObject& result) const
{
	checkNumArgs(tokens, Between{2, 3}, Prefix{2}, "?media-slot-name?");
	if (tokens.size() == 2) {
		result.addListElements(
			view::transform(providers, &ProviderInfo::name));
	} else if (tokens.size() == 3) {
		auto name = tokens[2].getString();
		if (auto it = ranges::find(providers, name, &ProviderInfo::name);
		    it != providers.end()) {
			it->provider->getMediaInfo(result);
		} else {
			throw CommandException("No info about media slot ", name);
		}
	}
}

string MachineMediaInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Returns information about the given media slot.";
}

void MachineMediaInfo::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, view::transform(
			providers, &ProviderInfo::name));
	}
}

void MachineMediaInfo::registerProvider(std::string_view name, MediaInfoProvider& provider)
{
	assert(!contains(providers, name, &ProviderInfo::name));
	assert(!contains(providers, &provider, &ProviderInfo::provider));
	providers.emplace_back(name, &provider);
}

void MachineMediaInfo::unregisterProvider(MediaInfoProvider& provider)
{
	move_pop_back(providers,
	              rfind_unguarded(providers, &provider, &ProviderInfo::provider));
}

// DeviceInfo

DeviceInfo::DeviceInfo(MSXMotherBoard& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "device")
	, motherBoard(motherBoard_)
{
}

void DeviceInfo::execute(std::span<const TclObject> tokens, TclObject& result) const
{
	checkNumArgs(tokens, Between{2, 3}, Prefix{2}, "?device?");
	switch (tokens.size()) {
	case 2:
		result.addListElements(
			view::transform(motherBoard.availableDevices,
			                [](auto& d) { return d->getName(); }));
		break;
	case 3: {
		std::string_view deviceName = tokens[2].getString();
		const MSXDevice* device = motherBoard.findDevice(deviceName);
		if (!device) {
			throw CommandException("No such device: ", deviceName);
		}
		device->getDeviceInfo(result);
		break;
	}
	}
}

string DeviceInfo::help(std::span<const TclObject> /*tokens*/) const
{
	return "Without any arguments, returns the list of used device names.\n"
	       "With a device name as argument, returns the type (and for some "
	       "devices the subtype) of the given device.";
}

void DeviceInfo::tabCompletion(std::vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, view::transform(
			motherBoard.availableDevices,
			[](auto& d) -> std::string_view { return d->getName(); }));
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

void SettingObserver::update(const Setting& setting) noexcept
{
	if (&setting == &motherBoard.powerSetting) {
		if (motherBoard.powerSetting.getBoolean()) {
			motherBoard.powerUp();
		} else {
			motherBoard.powerDown();
		}
	} else if (&setting == &motherBoard.suppressMessagesSetting) {
		motherBoard.msxCliComm->setSuppressMessages(motherBoard.suppressMessagesSetting.getBoolean());
	} else {
		UNREACHABLE;
	}
}


// serialize
// version 1: initial version
// version 2: added reRecordCount
// version 3: removed reRecordCount (moved to ReverseManager)
// version 4: moved joystickportA/B from MSXPSG to here
// version 5: do serialize renShaTurbo
template<typename Archive>
void MSXMotherBoard::serialize(Archive& ar, unsigned version)
{
	// don't serialize:
	//    machineID, userNames, availableDevices, addRemoveUpdate,
	//    sharedStuffMap, msxCliComm, msxEventDistributor,
	//    msxCommandController, slotManager, eventDelay,
	//    debugger, msxMixer, panasonicMemory, ledStatus

	// Scheduler must come early so that devices can query current time
	ar.serialize("scheduler", *scheduler);
	// MSXMixer has already set sync points, those are invalid now
	// the following call will fix this
	if constexpr (Archive::IS_LOADER) {
		msxMixer->reInit();
	}

	ar.serialize("name", machineName);
	ar.serializeWithID("config", machineConfig2, std::ref(*this));
	assert(getMachineConfig() == machineConfig2.get());
	ar.serializeWithID("extensions", extensions, std::ref(*this));

	if (mapperIO) ar.serialize("mapperIO", *mapperIO);

	if (auto& devSwitch = getDeviceSwitch();
	    devSwitch.hasRegisteredDevices()) {
		ar.serialize("deviceSwitch", devSwitch);
	}

	if (getMachineConfig()) {
		ar.serialize("cpu", getCPU());
	}
	ar.serialize("cpuInterface", getCPUInterface());

	if (auto* port = dynamic_cast<CassettePort*>(&getCassettePort())) {
		ar.serialize("cassetteport", *port);
	}
	if (ar.versionAtLeast(version, 4)) {
		if (auto* port = dynamic_cast<JoystickPort*>(
				joystickPort[0].get())) {
			ar.serialize("joystickportA", *port);
		}
		if (auto* port = dynamic_cast<JoystickPort*>(
				joystickPort[1].get())) {
			ar.serialize("joystickportB", *port);
		}
	}
	if (ar.versionAtLeast(version, 5)) {
		if (renShaTurbo) ar.serialize("renShaTurbo", *renShaTurbo);
	}

	if constexpr (Archive::IS_LOADER) {
		powered = true; // must come before changing power setting
		powerSetting.setBoolean(true);
		getLedStatus().setLed(LedStatus::POWER, true);
		msxMixer->unmute();
	}

	if (version == 2) {
		assert(Archive::IS_LOADER);
		unsigned reRecordCount = 0; // silence warning
		ar.serialize("reRecordCount", reRecordCount);
		getReverseManager().setReRecordCount(reRecordCount);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXMotherBoard)

} // namespace openmsx

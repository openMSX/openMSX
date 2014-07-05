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
#include "VideoSourceSetting.hh"
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
#include <vector>
#include <set>
#include <iostream>

using std::set;
using std::string;
using std::vector;
using std::unique_ptr;

namespace openmsx {

class AddRemoveUpdate;
class ResetCmd;
class LoadMachineCmd;
class ListExtCmd;
class ExtCmd;
class RemoveExtCmd;
class MachineNameInfo;
class DeviceInfo;
class FastForwardHelper;
class JoyPortDebuggable;

class MSXMotherBoard::Impl : private Observer<Setting>, private noncopyable
{
public:
	Impl(MSXMotherBoard& self, Reactor& reactor);
	virtual ~Impl();

	const string& getMachineID();
	const string& getMachineName() const;

	bool execute();
	void fastForward(EmuTime::param time, bool fast);
	void exitCPULoopAsync();
	void exitCPULoopSync();
	void pause();
	void unpause();
	void powerUp();
	void doReset();
	void activate(bool active);
	bool isActive() const;
	bool isFastForwarding() const;
	byte readIRQVector();

	const HardwareConfig* getMachineConfig() const;
	void setMachineConfig(MSXMotherBoard& self, HardwareConfig* machineConfig);
	bool isTurboR() const;
	string loadMachine(MSXMotherBoard& self, const string& machine);

	typedef std::vector<std::unique_ptr<HardwareConfig>> Extensions;
	const Extensions& getExtensions() const;
	HardwareConfig* findExtension(string_ref extensionName);
	string loadExtension(MSXMotherBoard& self, string_ref extensionName, string_ref slotname);
	string insertExtension(string_ref name,
	                       unique_ptr<HardwareConfig> extension);
	void removeExtension(const HardwareConfig& extension);

	CliComm& getMSXCliComm();
	MSXEventDistributor& getMSXEventDistributor();
	StateChangeDistributor& getStateChangeDistributor();
	MSXCommandController& getMSXCommandController();
	Scheduler& getScheduler();
	CartridgeSlotManager& getSlotManager();
	RealTime& getRealTime();
	Debugger& getDebugger();
	MSXMixer& getMSXMixer();
	PluggingController& getPluggingController(MSXMotherBoard& self);
	MSXCPU& getCPU();
	MSXCPUInterface& getCPUInterface();
	PanasonicMemory& getPanasonicMemory(MSXMotherBoard& self);
	MSXDeviceSwitch& getDeviceSwitch();
	CassettePortInterface& getCassettePort();
	JoystickPortIf& getJoystickPort(unsigned port, MSXMotherBoard& self);
	RenShaTurbo& getRenShaTurbo();
	LedStatus& getLedStatus();
	ReverseManager& getReverseManager();
	Reactor& getReactor();
	VideoSourceSetting& getVideoSource();
	CommandController& getCommandController();
	InfoCommand& getMachineInfoCommand();
	EmuTime::param getCurrentTime();

	void addDevice(MSXDevice& device);
	void removeDevice(MSXDevice& device);
	MSXDevice* findDevice(string_ref name);

	MSXMotherBoard::SharedStuff& getSharedStuff(string_ref name);
	MSXMapperIO* createMapperIO();
	void destroyMapperIO();

	string getUserName(const string& hwName);
	void freeUserName(const string& hwName, const string& userName);

	template<typename Archive>
	void serialize(MSXMotherBoard& self, Archive& ar, unsigned version);

private:
	void powerDown();
	void deleteMachine();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	Reactor& reactor;
	string machineID;
	string machineName;

	vector<MSXDevice*> availableDevices; // no ownership

	StringMap<MSXMotherBoard::SharedStuff> sharedStuffMap;
	StringMap<set<string>> userNames;

	unique_ptr<MSXMapperIO> mapperIO;
	unsigned mapperIOCounter;

	// These two should normally be the same, only during savestate loading
	// machineConfig will already be filled in, but machineConfig2 not yet.
	// This is important when an exception happens during loading of
	// machineConfig2 (otherwise machineConfig2 gets deleted twice).
	// See also HardwareConfig::serialize() and setMachineConfig()
	unique_ptr<HardwareConfig> machineConfig2;
	HardwareConfig* machineConfig;

	Extensions extensions;

	// order of unique_ptr's is important!
	unique_ptr<AddRemoveUpdate> addRemoveUpdate;
	unique_ptr<MSXCliComm> msxCliComm;
	unique_ptr<MSXEventDistributor> msxEventDistributor;
	unique_ptr<StateChangeDistributor> stateChangeDistributor;
	unique_ptr<MSXCommandController> msxCommandController;
	unique_ptr<Scheduler> scheduler;
	unique_ptr<EventDelay> eventDelay;
	unique_ptr<RealTime> realTime;
	unique_ptr<Debugger> debugger;
	unique_ptr<MSXMixer> msxMixer;
	unique_ptr<PluggingController> pluggingController;
	unique_ptr<MSXCPU> msxCpu;
	unique_ptr<MSXCPUInterface> msxCpuInterface;
	unique_ptr<PanasonicMemory> panasonicMemory;
	unique_ptr<MSXDeviceSwitch> deviceSwitch;
	unique_ptr<CassettePortInterface> cassettePort;
	unique_ptr<JoystickPortIf> joystickPort[2];
	unique_ptr<JoyPortDebuggable> joyPortDebuggable;
	unique_ptr<RenShaTurbo> renShaTurbo;
	unique_ptr<LedStatus> ledStatus;
	unique_ptr<VideoSourceSetting> videoSourceSetting;

	unique_ptr<CartridgeSlotManager> slotManager;
	unique_ptr<ReverseManager> reverseManager;
	unique_ptr<ResetCmd>     resetCommand;
	unique_ptr<LoadMachineCmd> loadMachineCommand;
	unique_ptr<ListExtCmd>   listExtCommand;
	unique_ptr<ExtCmd>       extCommand;
	unique_ptr<RemoveExtCmd> removeExtCommand;
	unique_ptr<MachineNameInfo> machineNameInfo;
	unique_ptr<DeviceInfo>   deviceInfo;
	friend class DeviceInfo;

	unique_ptr<FastForwardHelper> fastForwardHelper;

	BooleanSetting& powerSetting;

	bool powered;
	bool active;
	bool fastForwarding;
};


class AddRemoveUpdate
{
public:
	AddRemoveUpdate(MSXMotherBoard::Impl& motherBoard);
	~AddRemoveUpdate();
private:
	MSXMotherBoard::Impl& motherBoard;
};

class ResetCmd : public RecordedCommand
{
public:
	ResetCmd(MSXMotherBoard::Impl& motherBoard);
	virtual void execute(array_ref<TclObject> tokens, TclObject& result,
	                     EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
private:
	MSXMotherBoard::Impl& motherBoard;
};

class LoadMachineCmd : public Command
{
public:
	LoadMachineCmd(MSXMotherBoard& motherBoard);
	virtual void execute(array_ref<TclObject> tokens, TclObject& result);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMotherBoard& motherBoard;
};

class ListExtCmd : public Command
{
public:
	ListExtCmd(MSXMotherBoard::Impl& motherBoard);
	virtual void execute(array_ref<TclObject> tokens, TclObject& result);
	virtual string help(const vector<string>& tokens) const;
private:
	MSXMotherBoard::Impl& motherBoard;
};

class RemoveExtCmd : public RecordedCommand
{
public:
	RemoveExtCmd(MSXMotherBoard::Impl& motherBoard);
	virtual void execute(array_ref<TclObject> tokens, TclObject& result,
	                     EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMotherBoard::Impl& motherBoard;
};

class MachineNameInfo : public InfoTopic
{
public:
	MachineNameInfo(MSXMotherBoard::Impl& motherBoard);
	virtual void execute(array_ref<TclObject> tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	MSXMotherBoard::Impl& motherBoard;
};

class DeviceInfo : public InfoTopic
{
public:
	DeviceInfo(MSXMotherBoard::Impl& motherBoard);
	virtual void execute(array_ref<TclObject> tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMotherBoard::Impl& motherBoard;
};

class FastForwardHelper : private Schedulable
{
public:
	FastForwardHelper(MSXMotherBoard::Impl& msxMotherBoardImpl);
	void setTarget(EmuTime::param targetTime);
private:
	virtual void executeUntil(EmuTime::param time, int userData);
	MSXMotherBoard::Impl& motherBoard;
};

class JoyPortDebuggable : public SimpleDebuggable
{
public:
	JoyPortDebuggable(MSXMotherBoard& motherBoard);
	virtual byte read(unsigned address, EmuTime::param time);
	virtual void write(unsigned address, byte value);
};


static unsigned machineIDCounter = 0;

MSXMotherBoard::Impl::Impl(
		MSXMotherBoard& self, Reactor& reactor_)
	: reactor(reactor_)
	, machineID(StringOp::Builder() << "machine" << ++machineIDCounter)
	, mapperIOCounter(0)
	, machineConfig(nullptr)
	, msxCliComm(make_unique<MSXCliComm>(self, reactor.getGlobalCliComm()))
	, msxEventDistributor(make_unique<MSXEventDistributor>())
	, stateChangeDistributor(make_unique<StateChangeDistributor>())
	, msxCommandController(make_unique<MSXCommandController>(
		reactor.getGlobalCommandController(), reactor,
		self, *msxEventDistributor, machineID))
	, scheduler(make_unique<Scheduler>())
	, msxMixer(make_unique<MSXMixer>(
		reactor.getMixer(), *scheduler, *msxCommandController,
		reactor.getGlobalSettings()))
	, fastForwardHelper(make_unique<FastForwardHelper>(*this))
	, powerSetting(reactor.getGlobalSettings().getPowerSetting())
	, powered(false)
	, active(false)
	, fastForwarding(false)
{
#if UNIQUE_PTR_BUG
	self.pimpl2.reset(this);
	self.pimpl = self.pimpl2.get();
#else
	self.pimpl.reset(this);
#endif

	slotManager = make_unique<CartridgeSlotManager>(self);
	reverseManager = make_unique<ReverseManager>(self);
	resetCommand = make_unique<ResetCmd>(*this);
	loadMachineCommand = make_unique<LoadMachineCmd>(self);
	listExtCommand = make_unique<ListExtCmd>(*this);
	extCommand = make_unique<ExtCmd>(self, "ext");
	removeExtCommand = make_unique<RemoveExtCmd>(*this);
	machineNameInfo = make_unique<MachineNameInfo>(*this);
	deviceInfo = make_unique<DeviceInfo>(*this);
	debugger = make_unique<Debugger>(self);

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
	videoSourceSetting = make_unique<VideoSourceSetting>(
		*msxCommandController);
	realTime = make_unique<RealTime>(
		self, reactor.getGlobalSettings(), *eventDelay);

	powerSetting.attach(*this);
}

MSXMotherBoard::Impl::~Impl()
{
	powerSetting.detach(*this);
	deleteMachine();

	assert(mapperIOCounter == 0);
	assert(availableDevices.empty());
	assert(extensions.empty());
	assert(!machineConfig2);
	assert(!getMachineConfig());
}

void MSXMotherBoard::Impl::deleteMachine()
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

const string& MSXMotherBoard::Impl::getMachineID()
{
	return machineID;
}

const string& MSXMotherBoard::Impl::getMachineName() const
{
	return machineName;
}

const HardwareConfig* MSXMotherBoard::Impl::getMachineConfig() const
{
	return machineConfig;
}

void MSXMotherBoard::Impl::setMachineConfig(
	MSXMotherBoard& self, HardwareConfig* machineConfig_)
{
	assert(!getMachineConfig());
	machineConfig = machineConfig_;

	// make sure the CPU gets instantiated from the main thread
	assert(!msxCpu);
	msxCpu = make_unique<MSXCPU>(self);
	msxCpuInterface = make_unique<MSXCPUInterface>(self);
}

bool MSXMotherBoard::Impl::isTurboR() const
{
	const HardwareConfig* config = getMachineConfig();
	assert(config);
	return config->getConfig().getChild("devices").findChild("S1990") != nullptr;
}

string MSXMotherBoard::Impl::loadMachine(MSXMotherBoard& self, const string& machine)
{
	assert(machineName.empty());
	assert(extensions.empty());
	assert(!machineConfig2);
	assert(!getMachineConfig());

	try {
		machineConfig2 = HardwareConfig::createMachineConfig(self, machine);
		setMachineConfig(self, machineConfig2.get());
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

string MSXMotherBoard::Impl::loadExtension(MSXMotherBoard& self, string_ref name, string_ref slotname)
{
	unique_ptr<HardwareConfig> extension;
	try {
		extension = HardwareConfig::createExtensionConfig(self, name, slotname);
	} catch (FileException& e) {
		throw MSXException(
			"Extension \"" + name + "\" not found: " + e.getMessage());
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"" + name + "\" extension: " + e.getMessage());
	}
	return insertExtension(name, std::move(extension));
}

string MSXMotherBoard::Impl::insertExtension(
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

HardwareConfig* MSXMotherBoard::Impl::findExtension(string_ref extensionName)
{
	auto it = std::find_if(begin(extensions), end(extensions),
		[&](Extensions::value_type& v) {
			return v->getName() == extensionName; });
	return (it != end(extensions)) ? it->get() : nullptr;
}

const MSXMotherBoard::Impl::Extensions& MSXMotherBoard::Impl::getExtensions() const
{
	return extensions;
}

void MSXMotherBoard::Impl::removeExtension(const HardwareConfig& extension)
{
	extension.testRemove();
	auto it = find_if_unguarded(extensions,
		[&](Extensions::value_type& v) { return v.get() == &extension; });
	getMSXCliComm().update(CliComm::EXTENSION, extension.getName(), "remove");
	extensions.erase(it);
}

CliComm& MSXMotherBoard::Impl::getMSXCliComm()
{
	return *msxCliComm;
}

MSXEventDistributor& MSXMotherBoard::Impl::getMSXEventDistributor()
{
	return *msxEventDistributor;
}

StateChangeDistributor& MSXMotherBoard::Impl::getStateChangeDistributor()
{
	return *stateChangeDistributor;
}

MSXCommandController& MSXMotherBoard::Impl::getMSXCommandController()
{
	return *msxCommandController;
}

Scheduler& MSXMotherBoard::Impl::getScheduler()
{
	return *scheduler;
}

CartridgeSlotManager& MSXMotherBoard::Impl::getSlotManager()
{
	return *slotManager;
}

RealTime& MSXMotherBoard::Impl::getRealTime()
{
	return *realTime;
}

Debugger& MSXMotherBoard::Impl::getDebugger()
{
	return *debugger;
}

MSXMixer& MSXMotherBoard::Impl::getMSXMixer()
{
	return *msxMixer;
}

PluggingController& MSXMotherBoard::Impl::getPluggingController(MSXMotherBoard& self)
{
	assert(getMachineConfig()); // needed for PluggableFactory::createAll()
	if (!pluggingController) {
		pluggingController = make_unique<PluggingController>(self);
	}
	return *pluggingController;
}

MSXCPU& MSXMotherBoard::Impl::getCPU()
{
	assert(getMachineConfig()); // because CPU needs to know if we're
	                            // emulating turbor or not
	return *msxCpu;
}

MSXCPUInterface& MSXMotherBoard::Impl::getCPUInterface()
{
	assert(getMachineConfig());
	return *msxCpuInterface;
}

PanasonicMemory& MSXMotherBoard::Impl::getPanasonicMemory(MSXMotherBoard& self)
{
	if (!panasonicMemory) {
		panasonicMemory = make_unique<PanasonicMemory>(self);
	}
	return *panasonicMemory;
}

MSXDeviceSwitch& MSXMotherBoard::Impl::getDeviceSwitch()
{
	if (!deviceSwitch) {
		deviceSwitch = DeviceFactory::createDeviceSwitch(*getMachineConfig());
	}
	return *deviceSwitch;
}

CassettePortInterface& MSXMotherBoard::Impl::getCassettePort()
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

JoystickPortIf& MSXMotherBoard::Impl::getJoystickPort(
	unsigned port, MSXMotherBoard& self)
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
		PluggingController& ctrl = getPluggingController(self);
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
		joyPortDebuggable = make_unique<JoyPortDebuggable>(self);
	}
	return *joystickPort[port];
}

RenShaTurbo& MSXMotherBoard::Impl::getRenShaTurbo()
{
	if (!renShaTurbo) {
		assert(getMachineConfig());
		renShaTurbo = make_unique<RenShaTurbo>(
			*msxCommandController,
			getMachineConfig()->getConfig());
	}
	return *renShaTurbo;
}

LedStatus& MSXMotherBoard::Impl::getLedStatus()
{
	if (!ledStatus) {
		getMSXCliComm(); // force init, to be on the safe side
		ledStatus = make_unique<LedStatus>(
			reactor.getEventDistributor(),
			*msxCommandController,
			*msxCliComm);
	}
	return *ledStatus;
}

ReverseManager& MSXMotherBoard::Impl::getReverseManager()
{
	return *reverseManager;
}

Reactor& MSXMotherBoard::Impl::getReactor()
{
	return reactor;
}

VideoSourceSetting& MSXMotherBoard::Impl::getVideoSource()
{
	return *videoSourceSetting;
}

CommandController& MSXMotherBoard::Impl::getCommandController()
{
	return *msxCommandController;
}

InfoCommand& MSXMotherBoard::Impl::getMachineInfoCommand()
{
	return msxCommandController->getMachineInfoCommand();
}

EmuTime::param MSXMotherBoard::Impl::getCurrentTime()
{
	return scheduler->getCurrentTime();
}

bool MSXMotherBoard::Impl::execute()
{
	if (!powered) {
		return false;
	}
	assert(getMachineConfig()); // otherwise powered cannot be true

	getCPU().execute(false);
	return true;
}

void MSXMotherBoard::Impl::fastForward(EmuTime::param time, bool fast)
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

void MSXMotherBoard::Impl::pause()
{
	if (getMachineConfig()) {
		getCPU().setPaused(true);
	}
	msxMixer->mute();
}

void MSXMotherBoard::Impl::unpause()
{
	if (getMachineConfig()) {
		getCPU().setPaused(false);
	}
	msxMixer->unmute();
}

void MSXMotherBoard::Impl::addDevice(MSXDevice& device)
{
	availableDevices.push_back(&device);
}

void MSXMotherBoard::Impl::removeDevice(MSXDevice& device)
{
	availableDevices.erase(find_unguarded(availableDevices, &device));
}

void MSXMotherBoard::Impl::doReset()
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

byte MSXMotherBoard::Impl::readIRQVector()
{
	byte result = 0xff;
	for (auto& d : availableDevices) {
		result &= d->readIRQVector();
	}
	return result;
}

void MSXMotherBoard::Impl::powerUp()
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

void MSXMotherBoard::Impl::powerDown()
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

void MSXMotherBoard::Impl::activate(bool active_)
{
	active = active_;
	auto event = std::make_shared<SimpleEvent>(
		active ? OPENMSX_MACHINE_ACTIVATED : OPENMSX_MACHINE_DEACTIVATED);
	msxEventDistributor->distributeEvent(event, scheduler->getCurrentTime());
	if (active) {
		realTime->resync();
	}
}
bool MSXMotherBoard::Impl::isActive() const
{
	return active;
}
bool MSXMotherBoard::Impl::isFastForwarding() const
{
	return fastForwarding;
}

void MSXMotherBoard::Impl::exitCPULoopAsync()
{
	if (getMachineConfig()) {
		getCPU().exitCPULoopAsync();
	}
}

void MSXMotherBoard::Impl::exitCPULoopSync()
{
	getCPU().exitCPULoopSync();
}

// Observer<Setting>
void MSXMotherBoard::Impl::update(const Setting& setting)
{
	if (&setting == &powerSetting) {
		if (powerSetting.getBoolean()) {
			powerUp();
		} else {
			powerDown();
		}
	} else {
		UNREACHABLE;
	}
}

MSXDevice* MSXMotherBoard::Impl::findDevice(string_ref name)
{
	for (auto& d : availableDevices) {
		if (d->getName() == name) {
			return d;
		}
	}
	return nullptr;
}

MSXMotherBoard::SharedStuff& MSXMotherBoard::Impl::getSharedStuff(
	string_ref name)
{
	return sharedStuffMap[name];
}

MSXMapperIO* MSXMotherBoard::Impl::createMapperIO()
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

void MSXMotherBoard::Impl::destroyMapperIO()
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

string MSXMotherBoard::Impl::getUserName(const string& hwName)
{
	auto& s = userNames[hwName];
	unsigned n = 0;
	string userName;
	do {
		userName = StringOp::Builder() << "untitled" << ++n;
	} while (s.find(userName) != end(s));
	s.insert(userName);
	return userName;
}

void MSXMotherBoard::Impl::freeUserName(const string& hwName,
                                      const string& userName)
{
	auto& s = userNames[hwName];
	assert(s.find(userName) != end(s));
	s.erase(userName);
}

// AddRemoveUpdate

AddRemoveUpdate::AddRemoveUpdate(MSXMotherBoard::Impl& motherBoard_)
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
ResetCmd::ResetCmd(MSXMotherBoard::Impl& motherBoard_)
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
ListExtCmd::ListExtCmd(MSXMotherBoard::Impl& motherBoard_)
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
RemoveExtCmd::RemoveExtCmd(MSXMotherBoard::Impl& motherBoard_)
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
	string_ref name = tokens[1].getString();
	HardwareConfig* extension = motherBoard.findExtension(name);
	if (!extension) {
		throw CommandException("No such extension: " + name);
	}
	try {
		motherBoard.removeExtension(*extension);
	} catch (MSXException& e) {
		throw CommandException("Can't remove extension '" + name +
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

MachineNameInfo::MachineNameInfo(MSXMotherBoard::Impl& motherBoard_)
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

DeviceInfo::DeviceInfo(MSXMotherBoard::Impl& motherBoard_)
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
		string_ref name = tokens[2].getString();
		MSXDevice* device = motherBoard.findDevice(name);
		if (!device) {
			throw CommandException("No such device: " + name);
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

FastForwardHelper::FastForwardHelper(MSXMotherBoard::Impl& motherBoard_)
	: Schedulable(motherBoard_.getScheduler())
	, motherBoard(motherBoard_)
{
}

void FastForwardHelper::setTarget(EmuTime::param targetTime)
{
	setSyncPoint(targetTime);
}

void FastForwardHelper::executeUntil(EmuTime::param /*time*/, int /*userData*/)
{
	motherBoard.exitCPULoopSync();
}


// class JoyPortDebuggable

JoyPortDebuggable::JoyPortDebuggable(MSXMotherBoard& motherBoard)
	: SimpleDebuggable(motherBoard, "joystickports", "MSX Joystick Ports", 2)
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

// serialize
// version 1: initial version
// version 2: added reRecordCount
// version 3: removed reRecordCount (moved to ReverseManager)
// version 4: moved joystickportA/B from MSXPSG to here
template<typename Archive>
void MSXMotherBoard::Impl::serialize(MSXMotherBoard& self, Archive& ar, unsigned version)
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
	ar.serializeWithID("config", machineConfig2, std::ref(self));
	assert(getMachineConfig() == machineConfig2.get());
	ar.serializeWithID("extensions", extensions, std::ref(self));

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


// MSXMotherBoard

MSXMotherBoard::MSXMotherBoard(Reactor& reactor)
{
	new MSXMotherBoard::Impl(*this, reactor);
}
MSXMotherBoard::~MSXMotherBoard()
{
}
const string& MSXMotherBoard::getMachineID()
{
	return pimpl->getMachineID();
}
bool MSXMotherBoard::execute()
{
	return pimpl->execute();
}
void MSXMotherBoard::fastForward(EmuTime::param time, bool fast)
{
	return pimpl->fastForward(time, fast);
}
void MSXMotherBoard::exitCPULoopAsync()
{
	pimpl->exitCPULoopAsync();
}
void MSXMotherBoard::exitCPULoopSync()
{
	pimpl->exitCPULoopSync();
}
void MSXMotherBoard::pause()
{
	pimpl->pause();
}
void MSXMotherBoard::unpause()
{
	pimpl->unpause();
}
void MSXMotherBoard::powerUp()
{
	pimpl->powerUp();
}
void MSXMotherBoard::activate(bool active)
{
	pimpl->activate(active);
}
bool MSXMotherBoard::isActive() const
{
	return pimpl->isActive();
}
bool MSXMotherBoard::isFastForwarding() const
{
	return pimpl->isFastForwarding();
}
byte MSXMotherBoard::readIRQVector()
{
	return pimpl->readIRQVector();
}
const HardwareConfig* MSXMotherBoard::getMachineConfig() const
{
	return pimpl->getMachineConfig();
}
void MSXMotherBoard::setMachineConfig(HardwareConfig* machineConfig)
{
	pimpl->setMachineConfig(*this, machineConfig);
}
bool MSXMotherBoard::isTurboR() const
{
	return pimpl->isTurboR();
}
string MSXMotherBoard::loadMachine(const string& machine)
{
	return pimpl->loadMachine(*this, machine);
}
HardwareConfig* MSXMotherBoard::findExtension(string_ref extensionName)
{
	return pimpl->findExtension(extensionName);
}
string MSXMotherBoard::loadExtension(string_ref extensionName, string_ref slotname)
{
	return pimpl->loadExtension(*this, extensionName, slotname);
}
string MSXMotherBoard::insertExtension(
	string_ref name, unique_ptr<HardwareConfig> extension)
{
	return pimpl->insertExtension(name, std::move(extension));
}
void MSXMotherBoard::removeExtension(const HardwareConfig& extension)
{
	pimpl->removeExtension(extension);
}
CliComm& MSXMotherBoard::getMSXCliComm()
{
	// note: return-type is CliComm instead of MSXCliComm
	return pimpl->getMSXCliComm();
}
MSXCommandController& MSXMotherBoard::getMSXCommandController()
{
	return pimpl->getMSXCommandController();
}
Scheduler& MSXMotherBoard::getScheduler()
{
	return pimpl->getScheduler();
}
MSXEventDistributor& MSXMotherBoard::getMSXEventDistributor()
{
	return pimpl->getMSXEventDistributor();
}
StateChangeDistributor& MSXMotherBoard::getStateChangeDistributor()
{
	return pimpl->getStateChangeDistributor();
}
CartridgeSlotManager& MSXMotherBoard::getSlotManager()
{
	return pimpl->getSlotManager();
}
RealTime& MSXMotherBoard::getRealTime()
{
	return pimpl->getRealTime();
}
Debugger& MSXMotherBoard::getDebugger()
{
	return pimpl->getDebugger();
}
MSXMixer& MSXMotherBoard::getMSXMixer()
{
	return pimpl->getMSXMixer();
}
PluggingController& MSXMotherBoard::getPluggingController()
{
	return pimpl->getPluggingController(*this);
}
MSXCPU& MSXMotherBoard::getCPU()
{
	return pimpl->getCPU();
}
MSXCPUInterface& MSXMotherBoard::getCPUInterface()
{
	return pimpl->getCPUInterface();
}
PanasonicMemory& MSXMotherBoard::getPanasonicMemory()
{
	return pimpl->getPanasonicMemory(*this);
}
MSXDeviceSwitch& MSXMotherBoard::getDeviceSwitch()
{
	return pimpl->getDeviceSwitch();
}
CassettePortInterface& MSXMotherBoard::getCassettePort()
{
	return pimpl->getCassettePort();
}
JoystickPortIf& MSXMotherBoard::getJoystickPort(unsigned port)
{
	return pimpl->getJoystickPort(port, *this);
}
RenShaTurbo& MSXMotherBoard::getRenShaTurbo()
{
	return pimpl->getRenShaTurbo();
}
LedStatus& MSXMotherBoard::getLedStatus()
{
	return pimpl->getLedStatus();
}
ReverseManager& MSXMotherBoard::getReverseManager()
{
	return pimpl->getReverseManager();
}
Reactor& MSXMotherBoard::getReactor()
{
	return pimpl->getReactor();
}
VideoSourceSetting& MSXMotherBoard::getVideoSource()
{
	return pimpl->getVideoSource();
}
CommandController& MSXMotherBoard::getCommandController()
{
	return pimpl->getCommandController();
}
InfoCommand& MSXMotherBoard::getMachineInfoCommand()
{
	return pimpl->getMachineInfoCommand();
}
EmuTime::param MSXMotherBoard::getCurrentTime()
{
	return pimpl->getCurrentTime();
}
void MSXMotherBoard::addDevice(MSXDevice& device)
{
	pimpl->addDevice(device);
}
void MSXMotherBoard::removeDevice(MSXDevice& device)
{
	pimpl->removeDevice(device);
}
MSXDevice* MSXMotherBoard::findDevice(string_ref name)
{
	return pimpl->findDevice(name);
}
MSXMotherBoard::SharedStuff& MSXMotherBoard::getSharedStuff(string_ref name)
{
	return pimpl->getSharedStuff(name);
}
MSXMapperIO* MSXMotherBoard::createMapperIO()
{
	return pimpl->createMapperIO();
}
void MSXMotherBoard::destroyMapperIO()
{
	pimpl->destroyMapperIO();
}
string MSXMotherBoard::getUserName(const string& hwName)
{
	return pimpl->getUserName(hwName);
}
void MSXMotherBoard::freeUserName(const string& hwName,
                                  const string& userName)
{
	pimpl->freeUserName(hwName, userName);
}

template<typename Archive>
void MSXMotherBoard::serialize(Archive& ar, unsigned version)
{
	pimpl->serialize(*this, ar, version);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMotherBoard)

} // namespace openmsx

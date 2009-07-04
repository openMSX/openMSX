// $Id$

#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXDevice.hh"
#include "ReverseManager.hh"
#include "HardwareConfig.hh"
#include "XMLElement.hh"
#include "MSXCliComm.hh"
#include "GlobalCliComm.hh"
#include "MSXCommandController.hh"
#include "Scheduler.hh"
#include "CartridgeSlotManager.hh"
#include "EventDistributor.hh"
#include "Debugger.hh"
#include "MSXMixer.hh"
#include "PluggingController.hh"
#include "DummyDevice.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"
#include "PanasonicMemory.hh"
#include "MSXDeviceSwitch.hh"
#include "MSXMapperIO.hh"
#include "CassettePort.hh"
#include "RenShaTurbo.hh"
#include "LedStatus.hh"
#include "MSXEventDistributor.hh"
#include "EventDelay.hh"
#include "RealTime.hh"
#include "DeviceFactory.hh"
#include "BooleanSetting.hh"
#include "GlobalSettings.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "RecordedCommand.hh"
#include "InfoTopic.hh"
#include "FileException.hh"
#include "TclObject.hh"
#include "Observer.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "serialize_stl.hh"
#include "ref.hh"
#include <cassert>
#include <map>
#include <vector>
#include <iostream>

using std::set;
using std::map;
using std::string;
using std::vector;
using std::auto_ptr;

namespace openmsx {

class AddRemoveUpdate;
class ResetCmd;
class LoadMachineCmd;
class ListExtCmd;
class ExtCmd;
class RemoveExtCmd;
class MachineNameInfo;
class DeviceInfo;

class MSXMotherBoardImpl : private Observer<Setting>, private noncopyable
{
public:
	MSXMotherBoardImpl(MSXMotherBoard& self,
	                   Reactor& reactor, FilePool& filePool);
	virtual ~MSXMotherBoardImpl();

	const string& getMachineID();
	const string& getMachineName() const;

	bool execute();
	void exitCPULoopAsync();
	void pause();
	void unpause();
	void powerUp();
	void doReset();
	void activate(bool active);
	bool isActive() const;
	byte readIRQVector();

	const HardwareConfig* getMachineConfig() const;
	void setMachineConfig(HardwareConfig* machineConfig);
	bool isTurboR() const;
	void loadMachine(const string& machine);

	typedef std::vector<HardwareConfig*> Extensions;
	const Extensions& getExtensions() const;
	HardwareConfig* findExtension(const string& extensionName);
	string loadExtension(const string& extensionName);
	string insertExtension(const std::string& name,
	                       std::auto_ptr<HardwareConfig> extension);
	void removeExtension(const HardwareConfig& extension);

	CliComm& getMSXCliComm();
	CliComm* getMSXCliCommIfAvailable();
	MSXEventDistributor& getMSXEventDistributor();
	MSXCommandController& getMSXCommandController();
	Scheduler& getScheduler();
	CartridgeSlotManager& getSlotManager();
	EventDelay& getEventDelay();
	RealTime& getRealTime();
	Debugger& getDebugger();
	MSXMixer& getMSXMixer();
	PluggingController& getPluggingController();
	DummyDevice& getDummyDevice();
	MSXCPU& getCPU();
	MSXCPUInterface& getCPUInterface();
	PanasonicMemory& getPanasonicMemory();
	MSXDeviceSwitch& getDeviceSwitch();
	CassettePortInterface& getCassettePort();
	RenShaTurbo& getRenShaTurbo();
	LedStatus& getLedStatus();
	ReverseManager& getReverseManager();
	Reactor& getReactor();
	FilePool& getFilePool();
	CommandController& getCommandController();
	InfoCommand& getMachineInfoCommand();
	EmuTime::param getCurrentTime();

	void addDevice(MSXDevice& device);
	void removeDevice(MSXDevice& device);
	MSXDevice* findDevice(const string& name);

	MSXMotherBoard::SharedStuff& getSharedStuff(const string& name);
	MSXMapperIO* createMapperIO();
	void destroyMapperIO();

	string getUserName(const string& hwName);
	void freeUserName(const string& hwName, const string& userName);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void powerDown();
	void deleteMachine();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	MSXMotherBoard& self;

	Reactor& reactor;
	FilePool& filePool;
	string machineID;
	string machineName;

	typedef vector<MSXDevice*> Devices;
	Devices availableDevices; // no ownership

	typedef map<string, MSXMotherBoard::SharedStuff> SharedStuffMap;
	SharedStuffMap sharedStuffMap;
	map<string, set<string> > userNames;

	auto_ptr<MSXMapperIO> mapperIO;
	unsigned mapperIOCounter;

	// These two should normally be the same, only during savestate loading
	// machineConfig will already be filled in, but machineConfig2 not yet.
	// This is important when an exception happens during loading of
	// machineConfig2 (otherwise machineConfig2 gets deleted twice).
	// See also HardwareConfig::serialize() and setMachineConfig()
	auto_ptr<HardwareConfig> machineConfig2;
	HardwareConfig* machineConfig;

	Extensions extensions;

	// order of auto_ptr's is important!
	auto_ptr<AddRemoveUpdate> addRemoveUpdate;
	auto_ptr<MSXCliComm> msxCliComm;
	auto_ptr<MSXEventDistributor> msxEventDistributor;
	auto_ptr<MSXCommandController> msxCommandController;
	auto_ptr<Scheduler> scheduler;
	auto_ptr<CartridgeSlotManager> slotManager;
	auto_ptr<EventDelay> eventDelay;
	auto_ptr<RealTime> realTime;
	auto_ptr<Debugger> debugger;
	auto_ptr<MSXMixer> msxMixer;
	auto_ptr<PluggingController> pluggingController;
	auto_ptr<DummyDevice> dummyDevice;
	auto_ptr<MSXCPU> msxCpu;
	auto_ptr<MSXCPUInterface> msxCpuInterface;
	auto_ptr<PanasonicMemory> panasonicMemory;
	auto_ptr<MSXDeviceSwitch> deviceSwitch;
	auto_ptr<CassettePortInterface> cassettePort;
	auto_ptr<RenShaTurbo> renShaTurbo;
	auto_ptr<LedStatus> ledStatus;

	auto_ptr<ReverseManager> reverseManager;
	auto_ptr<ResetCmd>     resetCommand;
	auto_ptr<LoadMachineCmd> loadMachineCommand;
	auto_ptr<ListExtCmd>   listExtCommand;
	auto_ptr<ExtCmd>       extCommand;
	auto_ptr<RemoveExtCmd> removeExtCommand;
	auto_ptr<MachineNameInfo> machineNameInfo;
	auto_ptr<DeviceInfo>   deviceInfo;
	friend class DeviceInfo;

	BooleanSetting& powerSetting;

	bool powered;
	bool active;
};


class AddRemoveUpdate
{
public:
	AddRemoveUpdate(MSXMotherBoardImpl& motherBoard);
	~AddRemoveUpdate();
private:
	MSXMotherBoardImpl& motherBoard;
};

class ResetCmd : public RecordedCommand
{
public:
	ResetCmd(MSXMotherBoardImpl& motherBoard);
	virtual string execute(const vector<string>& tokens,
	                       EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
private:
	MSXMotherBoardImpl& motherBoard;
};

class LoadMachineCmd : public SimpleCommand
{
public:
	LoadMachineCmd(MSXMotherBoardImpl& motherBoard);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMotherBoardImpl& motherBoard;
};

class ListExtCmd : public Command
{
public:
	ListExtCmd(MSXMotherBoardImpl& motherBoard);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual string help(const vector<string>& tokens) const;
private:
	MSXMotherBoardImpl& motherBoard;
};

class ExtCmd : public RecordedCommand
{
public:
	ExtCmd(MSXMotherBoardImpl& motherBoard);
	virtual string execute(const vector<string>& tokens,
	                       EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMotherBoardImpl& motherBoard;
};

class RemoveExtCmd : public RecordedCommand
{
public:
	RemoveExtCmd(MSXMotherBoardImpl& motherBoard);
	virtual string execute(const vector<string>& tokens,
	                       EmuTime::param time);
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMotherBoardImpl& motherBoard;
};

class MachineNameInfo : public InfoTopic
{
public:
	MachineNameInfo(MSXMotherBoardImpl& motherBoard);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
private:
	MSXMotherBoardImpl& motherBoard;
};

class DeviceInfo : public InfoTopic
{
public:
	DeviceInfo(MSXMotherBoardImpl& motherBoard);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMotherBoardImpl& motherBoard;
};


MSXMotherBoardImpl::MSXMotherBoardImpl(
		MSXMotherBoard& self_, Reactor& reactor_, FilePool& filePool_)
	: self(self_)
	, reactor(reactor_)
	, filePool(filePool_)
	, mapperIOCounter(0)
	, machineConfig(NULL)
	, scheduler(new Scheduler())
	, powerSetting(reactor.getGlobalSettings().getPowerSetting())
	, powered(false)
	, active(false)
{
	self.pimple.reset(this);

	reverseManager.reset(new ReverseManager(self));
	resetCommand.reset(new ResetCmd(*this));
	loadMachineCommand.reset(new LoadMachineCmd(*this));
	listExtCommand.reset(new ListExtCmd(*this));
	extCommand.reset(new ExtCmd(*this));
	removeExtCommand.reset(new RemoveExtCmd(*this));
	machineNameInfo.reset(new MachineNameInfo(*this));
	deviceInfo.reset(new DeviceInfo(*this));

	getMSXMixer().mute(); // powered down
	getRealTime(); // make sure it's instantiated
	getEventDelay();
	powerSetting.attach(*this);

	addRemoveUpdate.reset(new AddRemoveUpdate(*this));
}

MSXMotherBoardImpl::~MSXMotherBoardImpl()
{
	powerSetting.detach(*this);
	deleteMachine();

	assert(mapperIOCounter == 0);
	assert(availableDevices.empty());
	assert(extensions.empty());
	assert(!machineConfig2.get());
	assert(!getMachineConfig());
}

void MSXMotherBoardImpl::deleteMachine()
{
	while (!extensions.empty()) {
		try {
			removeExtension(*extensions.back());
		} catch (MSXException& e) {
			std::cerr << "Internal error: failed to remove "
			             "extension while deleting a machine: "
			          << e.getMessage() << std::endl;
			assert(false);
		}
	}

	machineConfig2.reset();
	machineConfig = NULL;
}

const string& MSXMotherBoardImpl::getMachineID()
{
	if (machineID.empty()) {
		static unsigned counter = 0;
		machineID = "machine" + StringOp::toString(++counter);
	}
	return machineID;
}

const string& MSXMotherBoardImpl::getMachineName() const
{
	return machineName;
}

const HardwareConfig* MSXMotherBoardImpl::getMachineConfig() const
{
	return machineConfig;
}

void MSXMotherBoardImpl::setMachineConfig(HardwareConfig* machineConfig_)
{
	assert(!getMachineConfig());
	machineConfig = machineConfig_;

	// make sure the CPU gets instantiated from the main thread
	getCPU();
}

bool MSXMotherBoardImpl::isTurboR() const
{
	const HardwareConfig* config = getMachineConfig();
	assert(config);
	return config->getConfig().getChild("devices").findChild("S1990") != NULL;
}

void MSXMotherBoardImpl::loadMachine(const string& machine)
{
	assert(machineName.empty());
	assert(extensions.empty());
	assert(!machineConfig2.get());
	assert(!getMachineConfig());

	try {
		machineConfig2 = HardwareConfig::createMachineConfig(self, machine);
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
	if (powerSetting.getValue()) {
		powerUp();
	}
	machineName = machine;
}

string MSXMotherBoardImpl::loadExtension(const string& name)
{
	auto_ptr<HardwareConfig> extension;
	try {
		extension = HardwareConfig::createExtensionConfig(self, name);
	} catch (FileException& e) {
		throw MSXException(
			"Extension \"" + name + "\" not found: " + e.getMessage());
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"" + name + "\" extension: " + e.getMessage());
	}
	return insertExtension(name, extension);
}

string MSXMotherBoardImpl::insertExtension(
	const string& name, auto_ptr<HardwareConfig> extension)
{
	try {
		extension->parseSlots();
		extension->createDevices();
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"" + name + "\" extension: " + e.getMessage());
	}
	string result = extension->getName();
	extensions.push_back(extension.release());
	self.getMSXCliComm().update(CliComm::EXTENSION, result, "add");
	return result;
}

HardwareConfig* MSXMotherBoardImpl::findExtension(const string& extensionName)
{
	for (Extensions::const_iterator it = extensions.begin();
	     it != extensions.end(); ++it) {
		if ((*it)->getName() == extensionName) {
			return *it;
		}
	}
	return NULL;
}

const MSXMotherBoardImpl::Extensions& MSXMotherBoardImpl::getExtensions() const
{
	return extensions;
}

void MSXMotherBoardImpl::removeExtension(const HardwareConfig& extension)
{
	extension.testRemove();
	Extensions::iterator it =
		find(extensions.begin(), extensions.end(), &extension);
	assert(it != extensions.end());
	self.getMSXCliComm().update(CliComm::EXTENSION, extension.getName(), "remove");
	delete &extension;
	extensions.erase(it);
}

CliComm& MSXMotherBoardImpl::getMSXCliComm()
{
	if (!msxCliComm.get()) {
		msxCliComm.reset(new MSXCliComm(self, reactor.getGlobalCliComm()));
	}
	return *msxCliComm;
}

CliComm* MSXMotherBoardImpl::getMSXCliCommIfAvailable()
{
	return msxCliComm.get();
}

MSXEventDistributor& MSXMotherBoardImpl::getMSXEventDistributor()
{
	if (!msxEventDistributor.get()) {
		msxEventDistributor.reset(new MSXEventDistributor());
	}
	return *msxEventDistributor;
}

MSXCommandController& MSXMotherBoardImpl::getMSXCommandController()
{
	if (!msxCommandController.get()) {
		msxCommandController.reset(new MSXCommandController(
			reactor.getGlobalCommandController(), self));
	}
	return *msxCommandController;
}

Scheduler& MSXMotherBoardImpl::getScheduler()
{
	return *scheduler;
}

CartridgeSlotManager& MSXMotherBoardImpl::getSlotManager()
{
	if (!slotManager.get()) {
		slotManager.reset(new CartridgeSlotManager(self));
	}
	return *slotManager;
}

EventDelay& MSXMotherBoardImpl::getEventDelay()
{
	if (!eventDelay.get()) {
		eventDelay.reset(new EventDelay(
			*scheduler, getCommandController(),
			reactor.getEventDistributor(), getMSXEventDistributor()));
	}
	return *eventDelay;
}

RealTime& MSXMotherBoardImpl::getRealTime()
{
	if (!realTime.get()) {
		realTime.reset(new RealTime(self, reactor.getGlobalSettings()));
	}
	return *realTime;
}

Debugger& MSXMotherBoardImpl::getDebugger()
{
	if (!debugger.get()) {
		debugger.reset(new Debugger(self));
	}
	return *debugger;
}

MSXMixer& MSXMotherBoardImpl::getMSXMixer()
{
	if (!msxMixer.get()) {
		msxMixer.reset(new MSXMixer(reactor.getMixer(), *scheduler,
		                            getMSXCommandController()));
	}
	return *msxMixer;
}

PluggingController& MSXMotherBoardImpl::getPluggingController()
{
	if (!pluggingController.get()) {
		pluggingController.reset(new PluggingController(self));
	}
	return *pluggingController;
}

DummyDevice& MSXMotherBoardImpl::getDummyDevice()
{
	if (!dummyDevice.get()) {
		dummyDevice = DeviceFactory::createDummyDevice(self);
	}
	return *dummyDevice;
}

MSXCPU& MSXMotherBoardImpl::getCPU()
{
	// because CPU needs to know if we're emulating turbor or not
	assert(getMachineConfig());

	if (!msxCpu.get()) {
		msxCpu.reset(new MSXCPU(self));
	}
	return *msxCpu;
}

MSXCPUInterface& MSXMotherBoardImpl::getCPUInterface()
{
	if (!msxCpuInterface.get()) {
		assert(getMachineConfig());
		msxCpuInterface.reset(new MSXCPUInterface(self));
	}
	return *msxCpuInterface;
}

PanasonicMemory& MSXMotherBoardImpl::getPanasonicMemory()
{
	if (!panasonicMemory.get()) {
		panasonicMemory.reset(new PanasonicMemory(self));
	}
	return *panasonicMemory;
}

MSXDeviceSwitch& MSXMotherBoardImpl::getDeviceSwitch()
{
	if (!deviceSwitch.get()) {
		deviceSwitch = DeviceFactory::createDeviceSwitch(self);
	}
	return *deviceSwitch;
}

CassettePortInterface& MSXMotherBoardImpl::getCassettePort()
{
	if (!cassettePort.get()) {
		assert(getMachineConfig());
		if (getMachineConfig()->getConfig().findChild("CassettePort")) {
			cassettePort.reset(new CassettePort(self));
		} else {
			cassettePort.reset(new DummyCassettePort());
		}
	}
	return *cassettePort;
}

RenShaTurbo& MSXMotherBoardImpl::getRenShaTurbo()
{
	if (!renShaTurbo.get()) {
		assert(getMachineConfig());
		renShaTurbo.reset(new RenShaTurbo(
			getCommandController(),
			getMachineConfig()->getConfig()));
	}
	return *renShaTurbo;
}

LedStatus& MSXMotherBoardImpl::getLedStatus()
{
	if (!ledStatus.get()) {
		getMSXCliComm(); // force init, to be on the safe side
		ledStatus.reset(new LedStatus(
			reactor.getEventDistributor(),
			getCommandController(),
			*msxCliComm));
	}
	return *ledStatus;
}

ReverseManager& MSXMotherBoardImpl::getReverseManager()
{
	return *reverseManager;
}

Reactor& MSXMotherBoardImpl::getReactor()
{
	return reactor;
}

FilePool& MSXMotherBoardImpl::getFilePool()
{
	return filePool;
}

CommandController& MSXMotherBoardImpl::getCommandController()
{
	return getMSXCommandController();
}

InfoCommand& MSXMotherBoardImpl::getMachineInfoCommand()
{
	return getMSXCommandController().getMachineInfoCommand();
}

EmuTime::param MSXMotherBoardImpl::getCurrentTime()
{
	return scheduler->getCurrentTime();
}

bool MSXMotherBoardImpl::execute()
{
	if (!powered) {
		return false;
	}
	assert(getMachineConfig()); // otherwise powered cannot be true

	getCPU().execute();
	return true;
}

void MSXMotherBoardImpl::pause()
{
	if (getMachineConfig()) {
		getCPU().setPaused(true);
	}
	getMSXMixer().mute();
}

void MSXMotherBoardImpl::unpause()
{
	if (getMachineConfig()) {
		getCPU().setPaused(false);
	}
	getMSXMixer().unmute();
}

void MSXMotherBoardImpl::addDevice(MSXDevice& device)
{
	availableDevices.push_back(&device);
}

void MSXMotherBoardImpl::removeDevice(MSXDevice& device)
{
	Devices::iterator it = find(availableDevices.begin(),
	                            availableDevices.end(), &device);
	assert(it != availableDevices.end());
	availableDevices.erase(it);
}

void MSXMotherBoardImpl::doReset()
{
	if (!powered) return;
	assert(getMachineConfig());

	EmuTime::param time = getCurrentTime();
	getCPUInterface().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reset(time);
	}
	getCPU().doReset(time);
	// let everyone know we're booting, note that the fact that this is
	// done after the reset call to the devices is arbitrary here
	reactor.getEventDistributor().distributeEvent(
		new SimpleEvent(OPENMSX_BOOT_EVENT));
}

byte MSXMotherBoardImpl::readIRQVector()
{
	byte result = 0xff;
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		result &= (*it)->readIRQVector();
	}
	return result;
}

void MSXMotherBoardImpl::powerUp()
{
	if (powered) return;
	if (!getMachineConfig()) return;

	powered = true;
	// TODO: If our "powered" field is always equal to the power setting,
	//       there is not really a point in keeping it.
	// TODO: assert disabled see note in Reactor::run() where this method
	//       is called
	//assert(powerSetting.getValue() == powered);
	powerSetting.changeValue(true);
	// TODO: We could make the power LED a device, so we don't have to handle
	//       it separately here.
	getLedStatus().setLed(LedStatus::POWER, true);

	EmuTime::param time = getCurrentTime();
	getCPUInterface().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerUp(time);
	}
	getCPU().doReset(time);
	getMSXMixer().unmute();
	// let everyone know we're booting, note that the fact that this is
	// done after the reset call to the devices is arbitrary here
	reactor.getEventDistributor().distributeEvent(
		new SimpleEvent(OPENMSX_BOOT_EVENT));
}

void MSXMotherBoardImpl::powerDown()
{
	if (!powered) return;

	powered = false;
	// TODO: This assertion fails in 1 case: when quitting with a running MSX.
	//       How do we want the Reactor to shutdown: immediately or after
	//       handling all pending commands/events/updates?
	//assert(powerSetting.getValue() == powered);
	powerSetting.changeValue(false);
	getLedStatus().setLed(LedStatus::POWER, false);

	getMSXMixer().mute();

	EmuTime::param time = getCurrentTime();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerDown(time);
	}
}

void MSXMotherBoardImpl::activate(bool active_)
{
	active = active_;
	MSXEventDistributor::EventPtr event = active
		? MSXEventDistributor::EventPtr(new SimpleEvent(OPENMSX_MACHINE_ACTIVATED))
		: MSXEventDistributor::EventPtr(new SimpleEvent(OPENMSX_MACHINE_DEACTIVATED));
	getMSXEventDistributor().distributeEvent(event, scheduler->getCurrentTime());
	if (active) {
		getRealTime().resync();
	}
}
bool MSXMotherBoardImpl::isActive() const
{
	return active;
}

void MSXMotherBoardImpl::exitCPULoopAsync()
{
	if (getMachineConfig()) {
		getCPU().exitCPULoopAsync();
	}
}

// Observer<Setting>
void MSXMotherBoardImpl::update(const Setting& setting)
{
	if (&setting == &powerSetting) {
		if (powerSetting.getValue()) {
			powerUp();
		} else {
			powerDown();
		}
	} else {
		assert(false);
	}
}

MSXDevice* MSXMotherBoardImpl::findDevice(const string& name)
{
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		if ((*it)->getName() == name) {
			return *it;
		}
	}
	return NULL;
}

MSXMotherBoard::SharedStuff& MSXMotherBoardImpl::getSharedStuff(
	const string& name)
{
	return sharedStuffMap[name];
}

MSXMapperIO* MSXMotherBoardImpl::createMapperIO()
{
	if (mapperIOCounter == 0) {
		mapperIO = DeviceFactory::createMapperIO(self);

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

void MSXMotherBoardImpl::destroyMapperIO()
{
	assert(mapperIO.get());
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

string MSXMotherBoardImpl::getUserName(const string& hwName)
{
	set<string>& s = userNames[hwName];
	unsigned n = 0;
	string userName;
	do {
		userName = "untitled" + StringOp::toString(++n);
	} while (s.find(userName) != s.end());
	s.insert(userName);
	return userName;
}

void MSXMotherBoardImpl::freeUserName(const string& hwName,
                                      const string& userName)
{
	set<string>& s = userNames[hwName];
	assert(s.find(userName) != s.end());
	s.erase(userName);
}


// AddRemoveUpdate

AddRemoveUpdate::AddRemoveUpdate(MSXMotherBoardImpl& motherBoard_)
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
ResetCmd::ResetCmd(MSXMotherBoardImpl& motherBoard_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getMSXEventDistributor(),
	                  motherBoard_.getScheduler(),
	                  "reset")
	, motherBoard(motherBoard_)
{
}

string ResetCmd::execute(const vector<string>& /*tokens*/,
                         EmuTime::param /*time*/)
{
	motherBoard.doReset();
	return "";
}

string ResetCmd::help(const vector<string>& /*tokens*/) const
{
	return "Resets the MSX.";
}


// LoadMachineCmd
LoadMachineCmd::LoadMachineCmd(MSXMotherBoardImpl& motherBoard_)
	: SimpleCommand(motherBoard_.getCommandController(), "load_machine")
	, motherBoard(motherBoard_)
{
}

string LoadMachineCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	if (motherBoard.getMachineConfig()) {
		throw CommandException("Already loaded a config in this machine.");
	}
	motherBoard.loadMachine(tokens[1]);
	return motherBoard.getMachineName();
}

string LoadMachineCmd::help(const vector<string>& /*tokens*/) const
{
	return "Load a msx machine configuration into an empty machine.";
}

void LoadMachineCmd::tabCompletion(vector<string>& tokens) const
{
	set<string> machines;
	Reactor::getHwConfigs("machines", machines);
	completeString(tokens, machines);
}


// ListExtCmd
ListExtCmd::ListExtCmd(MSXMotherBoardImpl& motherBoard_)
	: Command(motherBoard_.getCommandController(), "list_extensions")
	, motherBoard(motherBoard_)
{
}

void ListExtCmd::execute(const vector<TclObject*>& /*tokens*/,
                         TclObject& result)
{
	const MSXMotherBoardImpl::Extensions& extensions = motherBoard.getExtensions();
	for (MSXMotherBoardImpl::Extensions::const_iterator it = extensions.begin();
	     it != extensions.end(); ++it) {
		result.addListElement((*it)->getName());
	}
}

string ListExtCmd::help(const vector<string>& /*tokens*/) const
{
	return "Return a list of all inserted extensions.";
}


// ExtCmd
ExtCmd::ExtCmd(MSXMotherBoardImpl& motherBoard_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getMSXEventDistributor(),
	                  motherBoard_.getScheduler(),
	                  "ext")
	, motherBoard(motherBoard_)
{
}

string ExtCmd::execute(const vector<string>& tokens, EmuTime::param /*time*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	try {
		return motherBoard.loadExtension(tokens[1]);
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
	set<string> extensions;
	Reactor::getHwConfigs("extensions", extensions);
	completeString(tokens, extensions);
}


// RemoveExtCmd
RemoveExtCmd::RemoveExtCmd(MSXMotherBoardImpl& motherBoard_)
	: RecordedCommand(motherBoard_.getCommandController(),
	                  motherBoard_.getMSXEventDistributor(),
	                  motherBoard_.getScheduler(),
	                  "remove_extension")
	, motherBoard(motherBoard_)
{
}

string RemoveExtCmd::execute(const vector<string>& tokens, EmuTime::param /*time*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	HardwareConfig* extension = motherBoard.findExtension(tokens[1]);
	if (!extension) {
		throw CommandException("No such extension: " + tokens[1]);
	}
	try {
		motherBoard.removeExtension(*extension);
	} catch (MSXException& e) {
		throw CommandException("Can't remove extension '" + tokens[1] +
		                       "': " + e.getMessage());
	}
	return "";
}

string RemoveExtCmd::help(const vector<string>& /*tokens*/) const
{
	return "Remove an extension from the MSX machine.";
}

void RemoveExtCmd::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 2) {
		set<string> names;
		for (MSXMotherBoardImpl::Extensions::const_iterator it =
		         motherBoard.getExtensions().begin();
		     it != motherBoard.getExtensions().end(); ++it) {
			names.insert((*it)->getName());
		}
		completeString(tokens, names);
	}
}


// MachineNameInfo

MachineNameInfo::MachineNameInfo(MSXMotherBoardImpl& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "config_name")
	, motherBoard(motherBoard_)
{
}

void MachineNameInfo::execute(const vector<TclObject*>& /*tokens*/,
                              TclObject& result) const
{
	result.setString(motherBoard.getMachineName());
}

string MachineNameInfo::help(const vector<string>& /*tokens*/) const
{
	return "Returns the configuration name for this machine.";
}


// DeviceInfo

DeviceInfo::DeviceInfo(MSXMotherBoardImpl& motherBoard_)
	: InfoTopic(motherBoard_.getMachineInfoCommand(), "device")
	, motherBoard(motherBoard_)
{
}

void DeviceInfo::execute(const vector<TclObject*>& tokens,
                         TclObject& result) const
{
	switch (tokens.size()) {
	case 2:
		for (MSXMotherBoardImpl::Devices::const_iterator it =
		        motherBoard.availableDevices.begin();
		     it != motherBoard.availableDevices.end(); ++it) {
			result.addListElement((*it)->getName());
		}
		break;
	case 3: {
		string name = tokens[2]->getString();
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
		set<string> names;
		for (MSXMotherBoardImpl::Devices::const_iterator it =
		         motherBoard.availableDevices.begin();
		     it != motherBoard.availableDevices.end(); ++it) {
			names.insert((*it)->getName());
		}
		completeString(tokens, names);
	}
}

// serialize
template<typename Archive>
void MSXMotherBoardImpl::serialize(Archive& ar, unsigned /*version*/)
{
	// don't serialize:
	//    machineID, userNames, availableDevices, addRemoveUpdate,
	//    sharedStuffMap, msxCliComm, msxEventDistributor,
	//    msxCommandController, slotManager, eventDelay,
	//    debugger, msxMixer, dummyDevice, panasonicMemory, renShaTurbo,
	//    ledStatus

	// Scheduler must come early so that devices can query current time
	ar.serialize("scheduler", *scheduler);
	// MSXMixer has already set syncpoints, those are invalid now
	// the following call will fix this
	getMSXMixer().reschedule();

	ar.serialize("name", machineName);
	ar.serializeWithID("config", machineConfig2, ref(self));
	assert(getMachineConfig() == machineConfig2.get());
	ar.serializeWithID("extensions", extensions, ref(self));

	if (mapperIO.get()) {
		ar.serialize("mapperIO", *mapperIO);
	}

	MSXDeviceSwitch& devSwitch = getDeviceSwitch();
	if (devSwitch.hasRegisteredDevices()) {
		ar.serialize("deviceSwitch", devSwitch);
	}

	if (getMachineConfig()) {
		ar.serialize("cpu", getCPU());
	}
	ar.serialize("cpuInterface", getCPUInterface());

	if (CassettePort* port = dynamic_cast<CassettePort*>(&getCassettePort())) {
		ar.serialize("cassetteport", *port);
	}

	if (ar.isLoader()) {
		if (powerSetting.getValue()) {
			powered = true;
			getLedStatus().setLed(LedStatus::POWER, true);
			getMSXMixer().unmute();
		}
	}
}


// MSXMotherBoard

MSXMotherBoard::MSXMotherBoard(Reactor& reactor, FilePool& filePool)
{
	new MSXMotherBoardImpl(*this, reactor, filePool);
}
MSXMotherBoard::~MSXMotherBoard()
{
}
const string& MSXMotherBoard::getMachineID()
{
	return pimple->getMachineID();
}
bool MSXMotherBoard::execute()
{
	return pimple->execute();
}
void MSXMotherBoard::exitCPULoopAsync()
{
	pimple->exitCPULoopAsync();
}
void MSXMotherBoard::pause()
{
	pimple->pause();
}
void MSXMotherBoard::unpause()
{
	pimple->unpause();
}
void MSXMotherBoard::powerUp()
{
	pimple->powerUp();
}
void MSXMotherBoard::activate(bool active)
{
	pimple->activate(active);
}
bool MSXMotherBoard::isActive() const
{
	return pimple->isActive();
}
byte MSXMotherBoard::readIRQVector()
{
	return pimple->readIRQVector();
}
const HardwareConfig* MSXMotherBoard::getMachineConfig() const
{
	return pimple->getMachineConfig();
}
void MSXMotherBoard::setMachineConfig(HardwareConfig* machineConfig)
{
	pimple->setMachineConfig(machineConfig);
}
bool MSXMotherBoard::isTurboR() const
{
	return pimple->isTurboR();
}
void MSXMotherBoard::loadMachine(const string& machine)
{
	pimple->loadMachine(machine);
}
HardwareConfig* MSXMotherBoard::findExtension(const string& extensionName)
{
	return pimple->findExtension(extensionName);
}
string MSXMotherBoard::loadExtension(const string& extensionName)
{
	return pimple->loadExtension(extensionName);
}
string MSXMotherBoard::insertExtension(
	const string& name, auto_ptr<HardwareConfig> extension)
{
	return pimple->insertExtension(name, extension);
}
void MSXMotherBoard::removeExtension(const HardwareConfig& extension)
{
	pimple->removeExtension(extension);
}
CliComm& MSXMotherBoard::getMSXCliComm()
{
	// note: return-type is CliComm instead of MSXCliComm
	return pimple->getMSXCliComm();
}
CliComm* MSXMotherBoard::getMSXCliCommIfAvailable()
{
	return pimple->getMSXCliCommIfAvailable();
}
MSXCommandController& MSXMotherBoard::getMSXCommandController()
{
	return pimple->getMSXCommandController();
}
Scheduler& MSXMotherBoard::getScheduler()
{
	return pimple->getScheduler();
}
MSXEventDistributor& MSXMotherBoard::getMSXEventDistributor()
{
	return pimple->getMSXEventDistributor();
}
CartridgeSlotManager& MSXMotherBoard::getSlotManager()
{
	return pimple->getSlotManager();
}
EventDelay& MSXMotherBoard::getEventDelay()
{
	return pimple->getEventDelay();
}
RealTime& MSXMotherBoard::getRealTime()
{
	return pimple->getRealTime();
}
Debugger& MSXMotherBoard::getDebugger()
{
	return pimple->getDebugger();
}
MSXMixer& MSXMotherBoard::getMSXMixer()
{
	return pimple->getMSXMixer();
}
PluggingController& MSXMotherBoard::getPluggingController()
{
	return pimple->getPluggingController();
}
DummyDevice& MSXMotherBoard::getDummyDevice()
{
	return pimple->getDummyDevice();
}
MSXCPU& MSXMotherBoard::getCPU()
{
	return pimple->getCPU();
}
MSXCPUInterface& MSXMotherBoard::getCPUInterface()
{
	return pimple->getCPUInterface();
}
PanasonicMemory& MSXMotherBoard::getPanasonicMemory()
{
	return pimple->getPanasonicMemory();
}
MSXDeviceSwitch& MSXMotherBoard::getDeviceSwitch()
{
	return pimple->getDeviceSwitch();
}
CassettePortInterface& MSXMotherBoard::getCassettePort()
{
	return pimple->getCassettePort();
}
RenShaTurbo& MSXMotherBoard::getRenShaTurbo()
{
	return pimple->getRenShaTurbo();
}
LedStatus& MSXMotherBoard::getLedStatus()
{
	return pimple->getLedStatus();
}
ReverseManager& MSXMotherBoard::getReverseManager()
{
	return pimple->getReverseManager();
}
Reactor& MSXMotherBoard::getReactor()
{
	return pimple->getReactor();
}
FilePool& MSXMotherBoard::getFilePool()
{
	return pimple->getFilePool();
}
CommandController& MSXMotherBoard::getCommandController()
{
	return pimple->getCommandController();
}
InfoCommand& MSXMotherBoard::getMachineInfoCommand()
{
	return pimple->getMachineInfoCommand();
}
EmuTime::param MSXMotherBoard::getCurrentTime()
{
	return pimple->getCurrentTime();
}
void MSXMotherBoard::addDevice(MSXDevice& device)
{
	pimple->addDevice(device);
}
void MSXMotherBoard::removeDevice(MSXDevice& device)
{
	pimple->removeDevice(device);
}
MSXDevice* MSXMotherBoard::findDevice(const string& name)
{
	return pimple->findDevice(name);
}
MSXMotherBoard::SharedStuff& MSXMotherBoard::getSharedStuff(const string& name)
{
	return pimple->getSharedStuff(name);
}
MSXMapperIO* MSXMotherBoard::createMapperIO()
{
	return pimple->createMapperIO();
}
void MSXMotherBoard::destroyMapperIO()
{
	pimple->destroyMapperIO();
}
string MSXMotherBoard::getUserName(const string& hwName)
{
	return pimple->getUserName(hwName);
}
void MSXMotherBoard::freeUserName(const string& hwName,
                                  const string& userName)
{
	pimple->freeUserName(hwName, userName);
}

template<typename Archive>
void MSXMotherBoard::serialize(Archive& ar, unsigned version)
{
	pimple->serialize(ar, version);
}
INSTANTIATE_SERIALIZE_METHODS(MSXMotherBoard)

} // namespace openmsx

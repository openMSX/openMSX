// $Id$

#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "MSXDevice.hh"
#include "MachineConfig.hh"
#include "ExtensionConfig.hh"
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
#include "CassettePort.hh"
#include "RenShaTurbo.hh"
#include "LedEvent.hh"
#include "MSXEventDistributor.hh"
#include "EventDelay.hh"
#include "EventTranslator.hh"
#include "RealTime.hh"
#include "DeviceFactory.hh"
#include "BooleanSetting.hh"
#include "GlobalSettings.hh"
#include "Command.hh"
#include "RecordedCommand.hh"
#include "InfoTopic.hh"
#include "FileException.hh"
#include "Observer.hh"
#include <cassert>
#include <map>

using std::set;
using std::map;
using std::string;
using std::vector;
using std::auto_ptr;

namespace openmsx {

class AddRemoveUpdate;
class ResetCmd;
class ListExtCmd;
class ExtCmd;
class RemoveExtCmd;
class MachineNameInfo;

class MSXMotherBoardImpl : private Observer<Setting>, private noncopyable
{
public:
	MSXMotherBoardImpl(MSXMotherBoard& self, Reactor& reactor);
	virtual ~MSXMotherBoardImpl();

	const string& getMachineID();
	const string& getMachineName() const;

	bool execute();
	void exitCPULoopSync();
	void exitCPULoopAsync();
	void block();
	void unblock();
	void pause();
	void unpause();
	void powerUp();
	void schedulePowerDown();
	void doPowerDown(const EmuTime& time);
	void scheduleReset();
	void doReset(const EmuTime& time);
	byte readIRQVector();

	const MachineConfig& getMachineConfig() const;
	void loadMachine(const string& machine);
	const MSXMotherBoard::Extensions& getExtensions() const;
	ExtensionConfig* findExtension(const string& extensionName);
	ExtensionConfig& loadExtension(const string& extensionName);
	ExtensionConfig& loadRom(
		const string& romname, const string& slotname,
		const vector<string>& options);
	void removeExtension(const ExtensionConfig& extension);

	MSXCliComm& getMSXCliComm();
	MSXCommandController& getMSXCommandController();
	Scheduler& getScheduler();
	MSXEventDistributor& getMSXEventDistributor();
	CartridgeSlotManager& getSlotManager();
	EventDelay& getEventDelay();
	EventTranslator& getEventTranslator();
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
	EventDistributor& getEventDistributor();
	Display& getDisplay();
	DiskManipulator& getDiskManipulator();
	FilePool& getFilePool();
	GlobalSettings& getGlobalSettings();
	GlobalCliComm& getGlobalCliComm();
	CommandController& getCommandController();

	void addDevice(MSXDevice& device);
	void removeDevice(MSXDevice& device);
	MSXDevice* findDevice(const string& name);

	MSXMotherBoard::SharedStuff& getSharedStuff(const string& name);

	string getUserName(const string& hwName);
	void freeUserName(const string& hwName, const string& userName);

private:
	void deleteMachine();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	MSXMotherBoard& self;

	Reactor& reactor;
	string machineID;
	string machineName;

	typedef vector<MSXDevice*> Devices;
	Devices availableDevices;

	typedef map<string, MSXMotherBoard::SharedStuff> SharedStuffMap;
	SharedStuffMap sharedStuffMap;
	map<string, set<string> > userNames;

	auto_ptr<MachineConfig> machineConfig;
	MSXMotherBoard::Extensions extensions;

	// order of auto_ptr's is important!
	auto_ptr<AddRemoveUpdate> addRemoveUpdate;
	auto_ptr<MSXCliComm> msxCliComm;
	auto_ptr<MSXCommandController> msxCommandController;
	auto_ptr<Scheduler> scheduler;
	auto_ptr<MSXEventDistributor> msxEventDistributor;
	auto_ptr<CartridgeSlotManager> slotManager;
	auto_ptr<EventDelay> eventDelay;
	auto_ptr<EventTranslator> eventTranslator;
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

	auto_ptr<ResetCmd>     resetCommand;
	auto_ptr<ListExtCmd>   listExtCommand;
	auto_ptr<ExtCmd>       extCommand;
	auto_ptr<RemoveExtCmd> removeExtCommand;
	auto_ptr<MachineNameInfo> machineNameInfo;
	BooleanSetting& powerSetting;

	int blockedCounter;
	bool powered;
	bool needReset;
	bool needPowerDown;
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
	                       const EmuTime& time);
	virtual string help(const vector<string>& tokens) const;
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
	                       const EmuTime& time);
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
	                       const EmuTime& time);
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


MSXMotherBoardImpl::MSXMotherBoardImpl(MSXMotherBoard& self_, Reactor& reactor_)
	: self(self_)
	, reactor(reactor_)
	, powerSetting(getGlobalSettings().getPowerSetting())
	, blockedCounter(0)
	, powered(false)
	, needReset(false)
	, needPowerDown(false)
{
	self.pimple.reset(this);

	resetCommand.reset(new ResetCmd(*this));
	listExtCommand.reset(new ListExtCmd(*this));
	extCommand.reset(new ExtCmd(*this));
	removeExtCommand.reset(new RemoveExtCmd(*this));
	machineNameInfo.reset(new MachineNameInfo(*this));

	getMSXMixer().mute(); // powered down
	getRealTime(); // make sure it's instantiated
	getEventTranslator();
	powerSetting.attach(*this);
}

MSXMotherBoardImpl::~MSXMotherBoardImpl()
{
	powerSetting.detach(*this);
	deleteMachine();

	assert(availableDevices.empty());
	assert(extensions.empty());
	assert(!machineConfig.get());
}

void MSXMotherBoardImpl::deleteMachine()
{
	while (!extensions.empty()) {
	 	removeExtension(*extensions.back());
	}

	machineConfig.reset();
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

const MachineConfig& MSXMotherBoardImpl::getMachineConfig() const
{
	assert(machineConfig.get());
	return *machineConfig;
}

void MSXMotherBoardImpl::loadMachine(const string& machine)
{
	assert(machineName.empty());
	assert(extensions.empty());
	assert(!machineConfig.get());
	machineName = machine;

	try {
		machineConfig.reset(new MachineConfig(self, machine));
	} catch (FileException& e) {
		throw MSXException("Machine \"" + machine + "\" not found: " +
		                   e.getMessage());
	} catch (MSXException& e) {
		throw MSXException("Error in \"" + machine + "\" machine: " +
		                   e.getMessage());
	}
	addRemoveUpdate.reset(new AddRemoveUpdate(*this));
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
}

ExtensionConfig& MSXMotherBoardImpl::loadExtension(const string& name)
{
	auto_ptr<ExtensionConfig> extension;
	try {
		extension.reset(new ExtensionConfig(self, name));
	} catch (FileException& e) {
		throw MSXException(
			"Extension \"" + name + "\" not found: " + e.getMessage()
			);
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"" + name + "\" extension: " + e.getMessage()
			);
	}
	try {
		extension->parseSlots();
		extension->createDevices();
	} catch (MSXException& e) {
		throw MSXException(
			"Error in \"" + name + "\" extension: " + e.getMessage()
			);
	}
	ExtensionConfig& result = *extension;
	extensions.push_back(extension.release());
	self.getMSXCliComm().update(CliComm::EXTENSION, result.getName(), "add");
	return result;
}

ExtensionConfig& MSXMotherBoardImpl::loadRom(
		const string& romname, const string& slotname,
		const vector<string>& options)
{
	auto_ptr<ExtensionConfig> extension(
		new ExtensionConfig(self, romname, slotname, options));
	extension->parseSlots();
	extension->createDevices();
	ExtensionConfig& result = *extension;
	extensions.push_back(extension.release());
	return result;
}

ExtensionConfig* MSXMotherBoardImpl::findExtension(const string& extensionName)
{
	for (MSXMotherBoard::Extensions::const_iterator it = extensions.begin();
	     it != extensions.end(); ++it) {
		if ((*it)->getName() == extensionName) {
			return *it;
		}
	}
	return NULL;
}

const MSXMotherBoard::Extensions& MSXMotherBoardImpl::getExtensions() const
{
	return extensions;
}

void MSXMotherBoardImpl::removeExtension(const ExtensionConfig& extension)
{
	extension.testRemove();
	MSXMotherBoard::Extensions::iterator it =
		find(extensions.begin(), extensions.end(), &extension);
	assert(it != extensions.end());
	self.getMSXCliComm().update(CliComm::EXTENSION, extension.getName(), "remove");
	delete &extension;
	extensions.erase(it);
}

MSXCliComm& MSXMotherBoardImpl::getMSXCliComm()
{
	if (!msxCliComm.get()) {
		msxCliComm.reset(new MSXCliComm(
			self, reactor.getGlobalCliComm()));
	}
	return *msxCliComm;
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
	if (!scheduler.get()) {
		scheduler.reset(new Scheduler());
	}
	return *scheduler;
}

MSXEventDistributor& MSXMotherBoardImpl::getMSXEventDistributor()
{
	if (!msxEventDistributor.get()) {
		msxEventDistributor.reset(new MSXEventDistributor());
	}
	return *msxEventDistributor;
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
			getScheduler(), getCommandController(),
			getMSXEventDistributor()));
	}
	return *eventDelay;
}

EventTranslator& MSXMotherBoardImpl::getEventTranslator()
{
	if (!eventTranslator.get()) {
		eventTranslator.reset(new EventTranslator(
			getEventDistributor(), getEventDelay()));
	}
	return *eventTranslator;
}

RealTime& MSXMotherBoardImpl::getRealTime()
{
	if (!realTime.get()) {
		realTime.reset(new RealTime(
			getScheduler(), getEventDistributor(),
			getEventDelay(), getGlobalSettings()));
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
		msxMixer.reset(new MSXMixer(reactor.getMixer(), getScheduler(),
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
	if (!msxCpu.get()) {
		msxCpu.reset(new MSXCPU(self));
	}
	return *msxCpu;
}

MSXCPUInterface& MSXMotherBoardImpl::getCPUInterface()
{
	if (!msxCpuInterface.get()) {
		// TODO assert hw config already loaded
		msxCpuInterface = MSXCPUInterface::create(
			self, getMachineConfig().getConfig());
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
		if (getMachineConfig().getConfig().findChild("CassettePort")) {
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
		renShaTurbo.reset(new RenShaTurbo(
			getCommandController(), getMachineConfig().getConfig()));
	}
	return *renShaTurbo;
}

EventDistributor& MSXMotherBoardImpl::getEventDistributor()
{
	return reactor.getEventDistributor();
}

Display& MSXMotherBoardImpl::getDisplay()
{
	return reactor.getDisplay();
}

DiskManipulator& MSXMotherBoardImpl::getDiskManipulator()
{
	return reactor.getDiskManipulator();
}

GlobalSettings& MSXMotherBoardImpl::getGlobalSettings()
{
	return reactor.getGlobalSettings();
}

GlobalCliComm& MSXMotherBoardImpl::getGlobalCliComm()
{
	return reactor.getGlobalCliComm();
}

FilePool& MSXMotherBoardImpl::getFilePool()
{
	return reactor.getFilePool();
}

CommandController& MSXMotherBoardImpl::getCommandController()
{
	return getMSXCommandController();
}

bool MSXMotherBoardImpl::execute()
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

void MSXMotherBoardImpl::block()
{
	++blockedCounter;
	exitCPULoopSync();
	getMSXMixer().mute();
}

void MSXMotherBoardImpl::unblock()
{
	--blockedCounter;
	assert(blockedCounter >= 0);
	getMSXMixer().unmute();
}

void MSXMotherBoardImpl::pause()
{
	getCPU().setPaused(true);
	getMSXMixer().mute();
}

void MSXMotherBoardImpl::unpause()
{
	getCPU().setPaused(false);
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

void MSXMotherBoardImpl::scheduleReset()
{
	needReset = true;
	exitCPULoopSync();
}

void MSXMotherBoardImpl::doReset(const EmuTime& time)
{
	getCPUInterface().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->reset(time);
	}
	getCPU().doReset(time);
	// let everyone know we're booting, note that the fact that this is
	// done after the reset call to the devices is arbitrary here
	getEventDistributor().distributeEvent(
		new SimpleEvent<OPENMSX_BOOT_EVENT>());
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
		new LedEvent(LedEvent::POWER, true, self));

	const EmuTime& time = getScheduler().getCurrentTime();
	getCPUInterface().reset();
	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerUp(time);
	}
	getCPU().doReset(time);
	getMSXMixer().unmute();
	// let everyone know we're booting, note that the fact that this is
	// done after the reset call to the devices is arbitrary here
	getEventDistributor().distributeEvent(
		new SimpleEvent<OPENMSX_BOOT_EVENT>());
}

void MSXMotherBoardImpl::schedulePowerDown()
{
	needPowerDown = true;
	exitCPULoopSync();
}

void MSXMotherBoardImpl::doPowerDown(const EmuTime& time)
{
	if (!powered) return;

	powered = false;
	// TODO: This assertion fails in 1 case: when quitting with a running MSX.
	//       How do we want the Reactor to shutdown: immediately or after
	//       handling all pending commands/events/updates?
	//assert(powerSetting.getValue() == powered);
	powerSetting.setValue(false);
	getEventDistributor().distributeEvent(
		new LedEvent(LedEvent::POWER, false, self));

	getMSXMixer().mute();

	for (Devices::iterator it = availableDevices.begin();
	     it != availableDevices.end(); ++it) {
		(*it)->powerDown(time);
	}
}

void MSXMotherBoardImpl::exitCPULoopSync()
{
	getCPU().exitCPULoopSync();
}

void MSXMotherBoardImpl::exitCPULoopAsync()
{
	getCPU().exitCPULoopAsync();
}

// Observer<Setting>
void MSXMotherBoardImpl::update(const Setting& setting)
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
	motherBoard.getGlobalCliComm().update(
		CliComm::HARDWARE, motherBoard.getMachineID(), "add");
}

AddRemoveUpdate::~AddRemoveUpdate()
{
	motherBoard.getGlobalCliComm().update(
		CliComm::HARDWARE, motherBoard.getMachineID(), "remove");
}


// ResetCmd
ResetCmd::ResetCmd(MSXMotherBoardImpl& motherBoard_)
	: RecordedCommand(motherBoard_.getMSXCommandController(),
	                  motherBoard_.getMSXEventDistributor(),
	                  motherBoard_.getScheduler(),
	                  "reset")
	, motherBoard(motherBoard_)
{
}

string ResetCmd::execute(const vector<string>& /*tokens*/,
                         const EmuTime& /*time*/)
{
	motherBoard.scheduleReset();
	return "";
}

string ResetCmd::help(const vector<string>& /*tokens*/) const
{
	return "Resets the MSX.\n";
}


// ListExtCmd
ListExtCmd::ListExtCmd(MSXMotherBoardImpl& motherBoard_)
	: Command(motherBoard_.getMSXCommandController(), "list_extensions")
	, motherBoard(motherBoard_)
{
}

void ListExtCmd::execute(const vector<TclObject*>& /*tokens*/,
                         TclObject& result)
{
	const MSXMotherBoard::Extensions& extensions = motherBoard.getExtensions();
	for (MSXMotherBoard::Extensions::const_iterator it = extensions.begin();
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
	: RecordedCommand(motherBoard_.getMSXCommandController(),
	                  motherBoard_.getMSXEventDistributor(),
	                  motherBoard_.getScheduler(),
	                  "ext")
	, motherBoard(motherBoard_)
{
}

string ExtCmd::execute(const vector<string>& tokens, const EmuTime& /*time*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	try {
		ExtensionConfig& extension =
			motherBoard.loadExtension(tokens[1]);
		return extension.getName();
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
	: RecordedCommand(motherBoard_.getMSXCommandController(),
	                  motherBoard_.getMSXEventDistributor(),
	                  motherBoard_.getScheduler(),
	                  "remove_extension")
	, motherBoard(motherBoard_)
{
}

string RemoveExtCmd::execute(const vector<string>& tokens, const EmuTime& /*time*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	ExtensionConfig* extension = motherBoard.findExtension(tokens[1]);
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
		for (MSXMotherBoard::Extensions::const_iterator it =
		         motherBoard.getExtensions().begin();
		     it != motherBoard.getExtensions().end(); ++it) {
			names.insert((*it)->getName());
		}
		completeString(tokens, names);
	}
}


// MachineNameInfo

MachineNameInfo::MachineNameInfo(MSXMotherBoardImpl& motherBoard_)
	: InfoTopic(motherBoard_.getMSXCommandController().getMachineInfoCommand(),
	            "config_name")
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


// MSXMotherBoard

MSXMotherBoard::MSXMotherBoard(Reactor& reactor)
{
	new MSXMotherBoardImpl(*this, reactor);
}
MSXMotherBoard::~MSXMotherBoard()
{
}
const string& MSXMotherBoard::getMachineID()
{
	return pimple->getMachineID();
}
const string& MSXMotherBoard::getMachineName() const
{
	return pimple->getMachineName();
}
bool MSXMotherBoard::execute()
{
	return pimple->execute();
}
void MSXMotherBoard::exitCPULoopSync()
{
	pimple->exitCPULoopSync();
}
void MSXMotherBoard::exitCPULoopAsync()
{
	pimple->exitCPULoopAsync();
}
void MSXMotherBoard::block()
{
	pimple->block();
}
void MSXMotherBoard::unblock()
{
	pimple->unblock();
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
void MSXMotherBoard::schedulePowerDown()
{
	pimple->schedulePowerDown();
}
void MSXMotherBoard::doPowerDown(const EmuTime& time)
{
	pimple->doPowerDown(time);
}
void MSXMotherBoard::scheduleReset()
{
	pimple->scheduleReset();
}
void MSXMotherBoard::doReset(const EmuTime& time)
{
	pimple->doReset(time);
}
byte MSXMotherBoard::readIRQVector()
{
	return pimple->readIRQVector();
}
const MachineConfig& MSXMotherBoard::getMachineConfig() const
{
	return pimple->getMachineConfig();
}
void MSXMotherBoard::loadMachine(const string& machine)
{
	pimple->loadMachine(machine);
}
const MSXMotherBoard::Extensions& MSXMotherBoard::getExtensions() const
{
	return pimple->getExtensions();
}
ExtensionConfig* MSXMotherBoard::findExtension(const string& extensionName)
{
	return pimple->findExtension(extensionName);
}
ExtensionConfig& MSXMotherBoard::loadExtension(const string& extensionName)
{
	return pimple->loadExtension(extensionName);
}
ExtensionConfig& MSXMotherBoard::loadRom(
	const string& romname, const string& slotname,
	const vector<string>& options)
{
	return pimple->loadRom(romname, slotname, options);
}
void MSXMotherBoard::removeExtension(const ExtensionConfig& extension)
{
	pimple->removeExtension(extension);
}
MSXCliComm& MSXMotherBoard::getMSXCliComm()
{
	return pimple->getMSXCliComm();
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
EventTranslator& MSXMotherBoard::getEventTranslator()
{
	return pimple->getEventTranslator();
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
EventDistributor& MSXMotherBoard::getEventDistributor()
{
	return pimple->getEventDistributor();
}
Display& MSXMotherBoard::getDisplay()
{
	return pimple->getDisplay();
}
DiskManipulator& MSXMotherBoard::getDiskManipulator()
{
	return pimple->getDiskManipulator();
}
FilePool& MSXMotherBoard::getFilePool()
{
	return pimple->getFilePool();
}
GlobalSettings& MSXMotherBoard::getGlobalSettings()
{
	return pimple->getGlobalSettings();
}
GlobalCliComm& MSXMotherBoard::getGlobalCliComm()
{
	return pimple->getGlobalCliComm();
}
CommandController& MSXMotherBoard::getCommandController()
{
	return pimple->getCommandController();
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
string MSXMotherBoard::getUserName(const string& hwName)
{
	return pimple->getUserName(hwName);
}
void MSXMotherBoard::freeUserName(const string& hwName,
                                  const string& userName)
{
	pimple->freeUserName(hwName, userName);
}

} // namespace openmsx

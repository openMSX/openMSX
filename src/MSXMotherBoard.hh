// $Id$

#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include "Observer.hh"
#include "noncopyable.hh"
#include <memory>
#include <vector>
#include <string>
#include <map>

namespace openmsx {

class Reactor;
class MSXDevice;
class MachineConfig;
class ExtensionConfig;
class MSXCommandController;
class Scheduler;
class CartridgeSlotManager;
class EventDistributor;
class MSXEventDistributor;
class EventDelay;
class EventTranslator;
class CliComm;
class RealTime;
class Debugger;
class MSXMixer;
class PluggingController;
class DummyDevice;
class MSXCPU;
class MSXCPUInterface;
class PanasonicMemory;
class MSXDeviceSwitch;
class CassettePortInterface;
class RenShaTurbo;
class Display;
class DiskManipulator;
class FilePool;
class GlobalSettings;
class CommandController;
class BooleanSetting;
class EmuTime;
class Setting;
class ResetCmd;
class ListExtCmd;
class ExtCmd;
class RemoveExtCmd;

class MSXMotherBoard : private Observer<Setting>, private noncopyable
{
public:
	explicit MSXMotherBoard(Reactor& reactor);
	virtual ~MSXMotherBoard();

	/**
	 * Run emulation.
	 * @return True if emulation steps were done,
	 *   false if emulation is suspended.
	 */
	bool execute();

	/** Request to exit the main CPU loop.
	  * Note: can get called from different threads
	  */
	void exitCPULoop();

	/**
	 * Block the complete MSX (CPU and devices), used by breakpoints
	 */
	void block();
	void unblock();

	/**
	 * Pause MSX machine. Only CPU is paused, other devices continue
	 * running. Used by turbor hardware pause.
	 */
	void pause();
	void unpause();

	void powerUp();
	void schedulePowerDown();
	void doPowerDown(const EmuTime& time);

	/**
	 * This will reset all MSXDevices (the reset() method of
	 * all registered MSXDevices is called)
	 */
	void scheduleReset();
	void doReset(const EmuTime& time);

	const MachineConfig& getMachineConfig() const;
	void loadMachine(const std::string& machine);

	typedef std::vector<ExtensionConfig*> Extensions;
	const Extensions& getExtensions() const;
	ExtensionConfig* findExtension(const std::string& extensionName);
	ExtensionConfig& loadExtension(const std::string& extensionName);
	ExtensionConfig& loadRom(
		const std::string& romname, const std::string& slotname,
		const std::vector<std::string>& options);
	void removeExtension(const ExtensionConfig& extension);

	// The following classes are unique per MSX machine
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

	// These are only convenience methods, Reactor keeps these objects
	EventDistributor& getEventDistributor();
	CliComm& getCliComm();
	Display& getDisplay();
	DiskManipulator& getDiskManipulator();
	FilePool& getFilePool();
	GlobalSettings& getGlobalSettings();

	// convenience methods
	CommandController& getCommandController();

	/**
	 * All MSXDevices should be registered by the MotherBoard.
	 */
	void addDevice(MSXDevice& device);
	void removeDevice(MSXDevice& device);

	/** Find a MSXDevice by name
	  * @Param name The name of the device as returned by
	  *             MSXDevice::getName()
	  * @Return A pointer to the device or NULL if the device could not
	  *         be found.
	  */
	MSXDevice* findDevice(const std::string& name);

	/** Some MSX device parts are shared between several MSX devices
	  * (e.g. all memory mappers share IO ports 0xFC-0xFF). But this
	  * sharing is limited to one MSX machine. This method offers the
	  * storage to implement per-machine reference counted objects.
	  */
	struct SharedStuff {
		SharedStuff() : counter(0), stuff(NULL) {}
		unsigned counter;
		void* stuff;
	};
	SharedStuff& getSharedStuff(const std::string& name);
	
private:
	void deleteMachine();

	// Observer<Setting>
	virtual void update(const Setting& setting);

	Reactor& reactor;
	
	typedef std::vector<MSXDevice*> Devices;
	Devices availableDevices;

	bool powered;
	bool needReset;
	bool needPowerDown;
	int blockedCounter;

	typedef std::map<std::string, SharedStuff> SharedStuffMap;
	SharedStuffMap sharedStuffMap;

	std::auto_ptr<MachineConfig> machineConfig;
	Extensions extensions;

	// order of auto_ptr's is important!
	std::auto_ptr<MSXCommandController> msxCommandController;
	std::auto_ptr<Scheduler> scheduler;
	std::auto_ptr<MSXEventDistributor> msxEventDistributor;
	std::auto_ptr<CartridgeSlotManager> slotManager;
	std::auto_ptr<EventDelay> eventDelay;
	std::auto_ptr<EventTranslator> eventTranslator;
	std::auto_ptr<RealTime> realTime;
	std::auto_ptr<Debugger> debugger;
	std::auto_ptr<MSXMixer> msxMixer;
	std::auto_ptr<PluggingController> pluggingController;
	std::auto_ptr<DummyDevice> dummyDevice;
	std::auto_ptr<MSXCPU> msxCpu;
	std::auto_ptr<MSXCPUInterface> msxCpuInterface;
	std::auto_ptr<PanasonicMemory> panasonicMemory;
	std::auto_ptr<MSXDeviceSwitch> deviceSwitch;
	std::auto_ptr<CassettePortInterface> cassettePort;
	std::auto_ptr<RenShaTurbo> renShaTurbo;

	const std::auto_ptr<ResetCmd>     resetCommand;
	const std::auto_ptr<ListExtCmd>   listExtCommand;
	const std::auto_ptr<ExtCmd>       extCommand;
	const std::auto_ptr<RemoveExtCmd> removeExtCommand;
	BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif

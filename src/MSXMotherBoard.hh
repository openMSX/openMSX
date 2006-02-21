// $Id$

#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include "Observer.hh"
#include <memory>
#include <vector>
#include <string>

namespace openmsx {

class Reactor;
class MSXDevice;
class XMLElement;
class MachineConfig;
class ExtensionConfig;
class Scheduler;
class CartridgeSlotManager;
class CommandController;
class EventDistributor;
class UserInputEventDistributor;
class InputEventGenerator;
class CliComm;
class RealTime;
class Debugger;
class Mixer;
class PluggingController;
class DummyDevice;
class MSXCPU;
class MSXCPUInterface;
class PanasonicMemory;
class MSXDeviceSwitch;
class CassettePortInterface;
class RenShaTurbo;
class CommandConsole;
class Display;
class IconStatus;
class FileManipulator;
class FilePool;
class BooleanSetting;
class EmuTime;
class Setting;
class ResetCmd;
class ListExtCmd;
class ExtCmd;
class CartCmd;
class RemoveExtCmd;

class MSXMotherBoard : private Observer<Setting>
{
public:
	MSXMotherBoard(Reactor& reactor);
	virtual ~MSXMotherBoard();

	/**
	 * Run emulation.
	 * @return True if emulation steps were done,
	 *   false if emulation is suspended.
	 */
	bool execute();

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
	void removeExtension(ExtensionConfig& extension);

	// The following classes are unique per MSX machine
	Scheduler& getScheduler();
	CartridgeSlotManager& getSlotManager();
	CommandController& getCommandController();
	EventDistributor& getEventDistributor();
	UserInputEventDistributor& getUserInputEventDistributor();
	InputEventGenerator& getInputEventGenerator();
	CliComm& getCliComm();
	RealTime& getRealTime();
	Debugger& getDebugger();
	Mixer& getMixer();
	PluggingController& getPluggingController();
	DummyDevice& getDummyDevice();
	MSXCPU& getCPU();
	MSXCPUInterface& getCPUInterface();
	PanasonicMemory& getPanasonicMemory();
	MSXDeviceSwitch& getDeviceSwitch();
	CassettePortInterface& getCassettePort();
	RenShaTurbo& getRenShaTurbo();
	CommandConsole& getCommandConsole();
	Display& getDisplay();
	IconStatus& getIconStatus();
	FileManipulator& getFileManipulator();
	FilePool& getFilePool();

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

	std::auto_ptr<MachineConfig> machineConfig;
	Extensions extensions;

	// order of auto_ptr's is important!
	std::auto_ptr<Scheduler> scheduler;
	std::auto_ptr<CartridgeSlotManager> slotManager;
	std::auto_ptr<CommandController> commandController;
	std::auto_ptr<EventDistributor> eventDistributor;
	std::auto_ptr<UserInputEventDistributor> userInputEventDistributor;
	std::auto_ptr<InputEventGenerator> inputEventGenerator;
	std::auto_ptr<CliComm> cliComm;
	std::auto_ptr<RealTime> realTime;
	std::auto_ptr<Debugger> debugger;
	std::auto_ptr<Mixer> mixer;
	std::auto_ptr<PluggingController> pluggingController;
	std::auto_ptr<XMLElement> dummyDeviceConfig;
	std::auto_ptr<DummyDevice> dummyDevice;
	std::auto_ptr<MSXCPU> msxCpu;
	std::auto_ptr<MSXCPUInterface> msxCpuInterface;
	std::auto_ptr<PanasonicMemory> panasonicMemory;
	std::auto_ptr<XMLElement> devSwitchConfig;
	std::auto_ptr<MSXDeviceSwitch> deviceSwitch;
	std::auto_ptr<CassettePortInterface> cassettePort;
	std::auto_ptr<RenShaTurbo> renShaTurbo;
	std::auto_ptr<CommandConsole> commandConsole;
	std::auto_ptr<Display> display;
	std::auto_ptr<IconStatus> iconStatus;
	std::auto_ptr<FileManipulator> fileManipulator;
	std::auto_ptr<FilePool> filePool;

	const std::auto_ptr<ResetCmd>     resetCommand;
	const std::auto_ptr<ListExtCmd>   listExtCommand;
	const std::auto_ptr<ExtCmd>       extCommand;
	const std::auto_ptr<CartCmd>      cartCommand;
	const std::auto_ptr<CartCmd>      cartaCommand;
	const std::auto_ptr<CartCmd>      cartbCommand;
	const std::auto_ptr<CartCmd>      cartcCommand;
	const std::auto_ptr<CartCmd>      cartdCommand;
	const std::auto_ptr<RemoveExtCmd> removeExtCommand;
	BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif

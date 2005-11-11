// $Id$

#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include "Observer.hh"
#include "Command.hh"
#include <memory>
#include <vector>

namespace openmsx {

class MSXDevice;
class XMLElement;
class Scheduler;
class CartridgeSlotManager;
class CommandController;
class EventDistributor;
class UserInputEventDistributor;
class InputEventGenerator;
class CliComm;
class RealTime;
class PluggingController;
class Debugger;
class Mixer;
class DummyDevice;
class MSXCPU;
class MSXCPUInterface;
class PanasonicMemory;
class MSXDeviceSwitch;
class CassettePortInterface;
class RenShaTurbo;
class CommandConsole;
class RenderSettings;
class Display;
class FileManipulator;
class FilePool;
class BooleanSetting;
class EmuTime;
class Setting;

class MSXMotherBoard : private Observer<Setting>
{
public:
	MSXMotherBoard();
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

	/** Parse machine config file and instantiate MSX machine
	  */
	void readConfig();

	// The following classes are unique per MSX machine
	Scheduler& getScheduler();
	CartridgeSlotManager& getSlotManager();
	CommandController& getCommandController();
	EventDistributor& getEventDistributor();
	UserInputEventDistributor& getUserInputEventDistributor();
	InputEventGenerator& getInputEventGenerator();
	CliComm& getCliComm();
	RealTime& getRealTime();
	PluggingController& getPluggingController();
	Debugger& getDebugger();
	Mixer& getMixer();
	DummyDevice& getDummyDevice();
	MSXCPU& getCPU();
	MSXCPUInterface& getCPUInterface();
	PanasonicMemory& getPanasonicMemory();
	MSXDeviceSwitch& getDeviceSwitch();
	CassettePortInterface& getCassettePort();
	RenShaTurbo& getRenShaTurbo();
	CommandConsole& getCommandConsole();
	RenderSettings& getRenderSettings();
	Display& getDisplay();
	FileManipulator& getFileManipulator();
	FilePool& getFilePool();

private:
	// Observer<Setting>
	virtual void update(const Setting& setting);

	/**
	 * All MSXDevices should be registered by the MotherBoard.
	 * This method should only be called at start-up
	 */
	void addDevice(std::auto_ptr<MSXDevice> device);

	void createDevices(const XMLElement& elem);

	typedef std::vector<MSXDevice*> Devices;
	Devices availableDevices;

	bool powered;
	bool needReset;
	bool needPowerDown;
	int blockedCounter;

	// order of auto_ptr's is important!
	std::auto_ptr<Scheduler> scheduler;
	std::auto_ptr<CartridgeSlotManager> slotManager;
	std::auto_ptr<CommandController> commandController;
	std::auto_ptr<EventDistributor> eventDistributor;
	std::auto_ptr<UserInputEventDistributor> userInputEventDistributor;
	std::auto_ptr<InputEventGenerator> inputEventGenerator;
	std::auto_ptr<CliComm> cliComm;
	std::auto_ptr<RealTime> realTime;
	std::auto_ptr<PluggingController> pluggingController;
	std::auto_ptr<Debugger> debugger;
	std::auto_ptr<Mixer> mixer;
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
	std::auto_ptr<RenderSettings> renderSettings;
	std::auto_ptr<Display> display;
	std::auto_ptr<FileManipulator> fileManipulator;
	std::auto_ptr<FilePool> filePool;

	class ResetCmd : public SimpleCommand {
	public:
		ResetCmd(CommandController& commandController,
		         MSXMotherBoard& motherBoard);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		MSXMotherBoard& motherBoard;
	} resetCommand;

	BooleanSetting& powerSetting;
};

} // namespace openmsx

#endif

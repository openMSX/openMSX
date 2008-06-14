// $Id$

#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>
#include <vector>
#include <string>

namespace openmsx {

class Reactor;
class MSXDevice;
class MachineConfig;
class ExtensionConfig;
class CliComm;
class MSXCommandController;
class Scheduler;
class CartridgeSlotManager;
class EventDistributor;
class MSXEventDistributor;
class EventDelay;
class EventTranslator;
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
class LedStatus;
class Display;
class DiskManipulator;
class FilePool;
class GlobalSettings;
class CommandController;
class InfoCommand;
class EmuTime;
class MSXMotherBoardImpl;

class MSXMotherBoard : private noncopyable
{
public:
	explicit MSXMotherBoard(Reactor& reactor);
	~MSXMotherBoard();

	const std::string& getMachineID();
	const std::string& getMachineName() const;

	/**
	 * Run emulation.
	 * @return True if emulation steps were done,
	 *   false if emulation is suspended.
	 */
	bool execute();

	/** See CPU::exitCPULoopsync(). */
	void exitCPULoopSync();
	/** See CPU::exitCPULoopAsync(). */
	void exitCPULoopAsync();

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

	void activate(bool active);
	bool isActive() const;

	/**
	 * This will reset all MSXDevices (the reset() method of
	 * all registered MSXDevices is called)
	 */
	void scheduleReset();
	void doReset(const EmuTime& time);

	byte readIRQVector();

	const MachineConfig* getMachineConfig() const;
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
	CliComm& getMSXCliComm();
	CliComm* getMSXCliCommIfAvailable();
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
	LedStatus& getLedStatus();

	// These are only convenience methods, Reactor keeps these objects
	EventDistributor& getEventDistributor();
	Display& getDisplay();
	DiskManipulator& getDiskManipulator();
	FilePool& getFilePool();
	GlobalSettings& getGlobalSettings();
	CliComm& getGlobalCliComm();

	// convenience methods
	CommandController& getCommandController();
	InfoCommand& getMachineInfoCommand();

	/** Convenience method:
	  * This is the same as getScheduler().getCurrentTime(). */
	const EmuTime& getCurrentTime();

	/**
	 * All MSXDevices should be registered by the MotherBoard.
	 */
	void addDevice(MSXDevice& device);
	void removeDevice(MSXDevice& device);

	/** Find a MSXDevice by name
	  * @param name The name of the device as returned by
	  *             MSXDevice::getName()
	  * @return A pointer to the device or NULL if the device could not
	  *         be found.
	  */
	MSXDevice* findDevice(const std::string& name);

	/** Some MSX device parts are shared between several MSX devices
	  * (e.g. all memory mappers share IO ports 0xFC-0xFF). But this
	  * sharing is limited to one MSX machine. This method offers the
	  * storage to implement per-machine reference counted objects.
	  */
	struct SharedStuff {
		SharedStuff() : stuff(NULL), counter(0) {}
		void* stuff;
		unsigned counter;
	};
	SharedStuff& getSharedStuff(const std::string& name);

	/** Keep track of which 'usernames' are in use.
	 * For example to be able to use several fmpac extensions at once, each
	 * with its own SRAM file, we need to generate unique filenames. We
	 * also want to reuse existing filenames as much as possible.
	 * ATM the usernames always have the format 'untitled[N]'. In the future
	 * we might allow really user specified names.
	 */
	std::string getUserName(const std::string& hwName);
	void freeUserName(const std::string& hwName,
	                  const std::string& userName);

private:
	std::auto_ptr<MSXMotherBoardImpl> pimple;
	friend class MSXMotherBoardImpl;
};

} // namespace openmsx

#endif

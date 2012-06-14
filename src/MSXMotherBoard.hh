// $Id$

#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include "EmuTime.hh"
#include "serialize_meta.hh"
#include "string_ref.hh"
#include "openmsx.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class Reactor;
class MSXDevice;
class HardwareConfig;
class CliComm;
class MSXCommandController;
class Scheduler;
class CartridgeSlotManager;
class EventDistributor;
class MSXEventDistributor;
class StateChangeDistributor;
class RealTime;
class Debugger;
class MSXMixer;
class PluggingController;
class MSXCPU;
class MSXCPUInterface;
class PanasonicMemory;
class MSXDeviceSwitch;
class CassettePortInterface;
class RenShaTurbo;
class LedStatus;
class ReverseManager;
class Display;
class DiskManipulator;
class CommandController;
class InfoCommand;
class MSXMapperIO;

class MSXMotherBoard : private noncopyable
{
public:
	explicit MSXMotherBoard(Reactor& reactor);
	~MSXMotherBoard();

	const std::string& getMachineID();

	/** Run emulation.
	 * @return True if emulation steps were done,
	 *   false if emulation is suspended.
	 */
	bool execute();

	/** Run emulation until a certain time in fast forward mode.
	 */
	void fastForward(EmuTime::param time, bool fast);

	/** See CPU::exitCPULoopAsync(). */
	void exitCPULoopAsync();

	/** Pause MSX machine. Only CPU is paused, other devices continue
	  * running. Used by turbor hardware pause.
	  */
	void pause();
	void unpause();

	void powerUp();

	void activate(bool active);
	bool isActive() const;
	bool isFastForwarding() const;

	byte readIRQVector();

	const HardwareConfig* getMachineConfig() const;
	void setMachineConfig(HardwareConfig* machineConfig);
	bool isTurboR() const;

	void loadMachine(const std::string& machine);

	HardwareConfig* findExtension(string_ref extensionName);
	std::string loadExtension(const std::string& extensionName);
	std::string insertExtension(const std::string& name,
	                            std::auto_ptr<HardwareConfig> extension);
	void removeExtension(const HardwareConfig& extension);

	// The following classes are unique per MSX machine
	CliComm& getMSXCliComm();
	MSXCommandController& getMSXCommandController();
	Scheduler& getScheduler();
	MSXEventDistributor& getMSXEventDistributor();
	StateChangeDistributor& getStateChangeDistributor();
	CartridgeSlotManager& getSlotManager();
	RealTime& getRealTime();
	Debugger& getDebugger();
	MSXMixer& getMSXMixer();
	PluggingController& getPluggingController();
	MSXCPU& getCPU();
	MSXCPUInterface& getCPUInterface();
	PanasonicMemory& getPanasonicMemory();
	MSXDeviceSwitch& getDeviceSwitch();
	CassettePortInterface& getCassettePort();
	RenShaTurbo& getRenShaTurbo();
	LedStatus& getLedStatus();
	ReverseManager& getReverseManager();
	Reactor& getReactor();

	// convenience methods
	CommandController& getCommandController();
	InfoCommand& getMachineInfoCommand();

	/** Convenience method:
	  * This is the same as getScheduler().getCurrentTime(). */
	EmuTime::param getCurrentTime();

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
	MSXDevice* findDevice(string_ref name);

	/** Some MSX device parts are shared between several MSX devices
	  * (e.g. all memory mappers share IO ports 0xFC-0xFF). But this
	  * sharing is limited to one MSX machine. This method offers the
	  * storage to implement per-machine reference counted objects.
	  * TODO This doesn't play nicely with savestates. For example memory
	  *      mappers don't use this mechanism anymore because of this.
	  *      Maybe this method can be removed when savestates are finished.
	  */
	struct SharedStuff {
		SharedStuff() : stuff(NULL), counter(0) {}
		void* stuff;
		unsigned counter;
	};
	SharedStuff& getSharedStuff(string_ref name);

	/** All memory mappers in one MSX machine share the same four (logical)
	  * memory mapper registers. These two methods handle this sharing.
	  */
	MSXMapperIO* createMapperIO();
	void destroyMapperIO();

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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

	class Impl;
private:
	std::auto_ptr<Impl> pimpl;
	friend class Impl;
};
SERIALIZE_CLASS_VERSION(MSXMotherBoard, 3);

} // namespace openmsx

#endif

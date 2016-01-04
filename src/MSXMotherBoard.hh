#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include "EmuTime.hh"
#include "VideoSourceSetting.hh"
#include "hash_map.hh"
#include "serialize_meta.hh"
#include "string_ref.hh"
#include "xxhash.hh"
#include "openmsx.hh"
#include "RecordedCommand.hh"
#include <memory>
#include <vector>

namespace openmsx {

class AddRemoveUpdate;
class BooleanSetting;
class CartridgeSlotManager;
class CassettePortInterface;
class CliComm;
class CommandController;
class Debugger;
class DeviceInfo;
class EventDelay;
class ExtCmd;
class FastForwardHelper;
class HardwareConfig;
class InfoCommand;
class JoyPortDebuggable;
class JoystickPortIf;
class LedStatus;
class ListExtCmd;
class LoadMachineCmd;
class MachineNameInfo;
class MSXCliComm;
class MSXCommandController;
class MSXCPU;
class MSXCPUInterface;
class MSXDevice;
class MSXDeviceSwitch;
class MSXEventDistributor;
class MSXMapperIO;
class MSXMixer;
class PanasonicMemory;
class PluggingController;
class Reactor;
class RealTime;
class RemoveExtCmd;
class RenShaTurbo;
class ResetCmd;
class ReverseManager;
class SettingObserver;
class Scheduler;
class Setting;
class StateChangeDistributor;

class MSXMotherBoard final
{
public:
	MSXMotherBoard(const MSXMotherBoard&) = delete;
	MSXMotherBoard& operator=(const MSXMotherBoard&) = delete;

	explicit MSXMotherBoard(Reactor& reactor);
	~MSXMotherBoard();

	const std::string& getMachineID()   const { return machineID; }
	const std::string& getMachineName() const { return machineName; }

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
	void exitCPULoopSync();

	/** Pause MSX machine. Only CPU is paused, other devices continue
	  * running. Used by turbor hardware pause.
	  */
	void pause();
	void unpause();

	void powerUp();

	void doReset();
	void activate(bool active);
	bool isActive() const { return active; }
	bool isFastForwarding() const { return fastForwarding; }

	byte readIRQVector();

	const HardwareConfig* getMachineConfig() const { return machineConfig; }
	void setMachineConfig(HardwareConfig* machineConfig);
	bool isTurboR() const;

	std::string loadMachine(const std::string& machine);

	using Extensions = std::vector<std::unique_ptr<HardwareConfig>>;
	const Extensions& getExtensions() const { return extensions; }
	HardwareConfig* findExtension(string_ref extensionName);
	std::string loadExtension(string_ref extensionName, string_ref slotname);
	std::string insertExtension(string_ref name,
	                            std::unique_ptr<HardwareConfig> extension);
	void removeExtension(const HardwareConfig& extension);

	// The following classes are unique per MSX machine
	CliComm& getMSXCliComm();
	MSXCommandController& getMSXCommandController() { return *msxCommandController; }
	Scheduler& getScheduler() { return *scheduler; }
	MSXEventDistributor& getMSXEventDistributor() { return *msxEventDistributor; }
	StateChangeDistributor& getStateChangeDistributor() { return *stateChangeDistributor; }
	CartridgeSlotManager& getSlotManager() { return *slotManager; }
	RealTime& getRealTime() { return *realTime; }
	Debugger& getDebugger() { return *debugger; }
	MSXMixer& getMSXMixer() { return *msxMixer; }
	PluggingController& getPluggingController();
	MSXCPU& getCPU();
	MSXCPUInterface& getCPUInterface();
	PanasonicMemory& getPanasonicMemory();
	MSXDeviceSwitch& getDeviceSwitch();
	CassettePortInterface& getCassettePort();
	JoystickPortIf& getJoystickPort(unsigned port);
	RenShaTurbo& getRenShaTurbo();
	LedStatus& getLedStatus();
	ReverseManager& getReverseManager() { return *reverseManager; }
	Reactor& getReactor() { return reactor; }
	VideoSourceSetting& getVideoSource() { return videoSourceSetting; }

	// convenience methods
	CommandController& getCommandController();
	InfoCommand& getMachineInfoCommand();

	/** Convenience method:
	  * This is the same as getScheduler().getCurrentTime(). */
	EmuTime::param getCurrentTime();

	/** All MSXDevices should be registered by the MotherBoard.
	 */
	void addDevice(MSXDevice& device);
	void removeDevice(MSXDevice& device);

	/** Find a MSXDevice by name
	  * @param name The name of the device as returned by
	  *             MSXDevice::getName()
	  * @return A pointer to the device or nullptr if the device could not
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
	template<typename T, typename ... Args>
	std::shared_ptr<T> getSharedStuff(string_ref name, Args&& ...args)
	{
		auto& weak = sharedStuffMap[name];
		auto shared = std::static_pointer_cast<T>(weak.lock());
		if (shared) return shared;

		shared = std::make_shared<T>(std::forward<Args>(args)...);
		weak = shared;
		return shared;
	}

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
	void freeUserName(const std::string& hwName, const std::string& userName);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void powerDown();
	void deleteMachine();

	Reactor& reactor;
	std::string machineID;
	std::string machineName;

	std::vector<MSXDevice*> availableDevices; // no ownership, no order

	hash_map<string_ref, std::weak_ptr<void>,      XXHasher> sharedStuffMap;
	hash_map<string_ref, std::vector<std::string>, XXHasher> userNames;

	std::unique_ptr<MSXMapperIO> mapperIO;
	unsigned mapperIOCounter;

	// These two should normally be the same, only during savestate loading
	// machineConfig will already be filled in, but machineConfig2 not yet.
	// This is important when an exception happens during loading of
	// machineConfig2 (otherwise machineConfig2 gets deleted twice).
	// See also HardwareConfig::serialize() and setMachineConfig()
	std::unique_ptr<HardwareConfig> machineConfig2;
	HardwareConfig* machineConfig;

	Extensions extensions; // order matters: later extension might depend on earlier ones

	// order of unique_ptr's is important!
	std::unique_ptr<AddRemoveUpdate> addRemoveUpdate;
	std::unique_ptr<MSXCliComm> msxCliComm;
	std::unique_ptr<MSXEventDistributor> msxEventDistributor;
	std::unique_ptr<StateChangeDistributor> stateChangeDistributor;
	std::unique_ptr<MSXCommandController> msxCommandController;
	std::unique_ptr<Scheduler> scheduler;
	std::unique_ptr<EventDelay> eventDelay;
	std::unique_ptr<RealTime> realTime;
	std::unique_ptr<Debugger> debugger;
	std::unique_ptr<MSXMixer> msxMixer;
	std::unique_ptr<PluggingController> pluggingController;
	std::unique_ptr<MSXCPU> msxCpu;
	std::unique_ptr<MSXCPUInterface> msxCpuInterface;
	std::unique_ptr<PanasonicMemory> panasonicMemory;
	std::unique_ptr<MSXDeviceSwitch> deviceSwitch;
	std::unique_ptr<CassettePortInterface> cassettePort;
	std::unique_ptr<JoystickPortIf> joystickPort[2];
	std::unique_ptr<JoyPortDebuggable> joyPortDebuggable;
	std::unique_ptr<RenShaTurbo> renShaTurbo;
	std::unique_ptr<LedStatus> ledStatus;
	VideoSourceSetting videoSourceSetting;

	std::unique_ptr<CartridgeSlotManager> slotManager;
	std::unique_ptr<ReverseManager> reverseManager;
	std::unique_ptr<ResetCmd>     resetCommand;
	std::unique_ptr<LoadMachineCmd> loadMachineCommand;
	std::unique_ptr<ListExtCmd>   listExtCommand;
	std::unique_ptr<ExtCmd>       extCommand;
	std::unique_ptr<RemoveExtCmd> removeExtCommand;
	std::unique_ptr<MachineNameInfo> machineNameInfo;
	std::unique_ptr<DeviceInfo>   deviceInfo;
	friend class DeviceInfo;

	std::unique_ptr<FastForwardHelper> fastForwardHelper;

	std::unique_ptr<SettingObserver> settingObserver;
	friend class SettingObserver;
	BooleanSetting& powerSetting;

	bool powered;
	bool active;
	bool fastForwarding;
};
SERIALIZE_CLASS_VERSION(MSXMotherBoard, 4);

class ExtCmd final : public RecordedCommand
{
public:
	ExtCmd(MSXMotherBoard& motherBoard, string_ref commandName);
	void execute(array_ref<TclObject> tokens, TclObject& result,
	             EmuTime::param time) override;
	std::string help(const std::vector<std::string>& tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
	std::string commandName;
};

} // namespace openmsx

#endif

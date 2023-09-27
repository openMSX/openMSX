#ifndef MSXMOTHERBOARD_HH
#define MSXMOTHERBOARD_HH

#include "EmuTime.hh"
#include "VideoSourceSetting.hh"
#include "BooleanSetting.hh"
#include "hash_map.hh"
#include "serialize_meta.hh"
#include "xxhash.hh"
#include "openmsx.hh"
#include "RecordedCommand.hh"
#include <array>
#include <cassert>
#include <memory>
#include <string_view>
#include <vector>

namespace openmsx {

class AddRemoveUpdate;
class CartridgeSlotManager;
class CassettePortInterface;
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
class MachineExtensionInfo;
class MachineMediaInfo;
class MachineNameInfo;
class MachineTypeInfo;
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
class StateChangeDistributor;

class MediaInfoProvider
{
public:
	MediaInfoProvider(const MediaInfoProvider&) = delete;
	MediaInfoProvider& operator=(const MediaInfoProvider&) = delete;

	/** This method gets called when information is required on the
	 * media inserted in the media slot of the provider. The provider
	 * must attach the info as a dictionary to the given TclObject.
	 */
	virtual void getMediaInfo(TclObject& result) = 0;

protected:
	MediaInfoProvider() = default;
	~MediaInfoProvider() = default;
};


class MSXMotherBoard final
{
public:
	MSXMotherBoard(const MSXMotherBoard&) = delete;
	MSXMotherBoard& operator=(const MSXMotherBoard&) = delete;

	explicit MSXMotherBoard(Reactor& reactor);
	~MSXMotherBoard();

	[[nodiscard]] std::string_view getMachineID()   const { return machineID; }
	[[nodiscard]] std::string_view getMachineName() const { return machineName; }

	/** Run emulation.
	 * @return True if emulation steps were done,
	 *   false if emulation is suspended.
	 */
	[[nodiscard]] bool execute();

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
	[[nodiscard]] bool isActive() const { return active; }
	[[nodiscard]] bool isFastForwarding() const { return fastForwarding; }

	[[nodiscard]] byte readIRQVector();

	[[nodiscard]] const HardwareConfig* getMachineConfig() const { return machineConfig; }
	[[nodiscard]] HardwareConfig* getMachineConfig() { return machineConfig; }
	void setMachineConfig(HardwareConfig* machineConfig);
	[[nodiscard]] std::string_view getMachineType() const;
	[[nodiscard]] bool isTurboR() const;
	[[nodiscard]] bool hasToshibaEngine() const;

	std::string loadMachine(const std::string& machine);

	using Extensions = std::vector<std::unique_ptr<HardwareConfig>>;
	[[nodiscard]] const Extensions& getExtensions() const { return extensions; }
	[[nodiscard]] HardwareConfig* findExtension(std::string_view extensionName);
	std::string loadExtension(std::string_view extensionName, std::string_view slotName);
	std::string insertExtension(std::string_view name,
	                            std::unique_ptr<HardwareConfig> extension);
	void removeExtension(const HardwareConfig& extension);

	// The following classes are unique per MSX machine
	[[nodiscard]] MSXCliComm& getMSXCliComm();
	[[nodiscard]] MSXCommandController& getMSXCommandController() { return *msxCommandController; }
	[[nodiscard]] Scheduler& getScheduler() { return *scheduler; }
	[[nodiscard]] MSXEventDistributor& getMSXEventDistributor() { return *msxEventDistributor; }
	[[nodiscard]] StateChangeDistributor& getStateChangeDistributor() { return *stateChangeDistributor; }
	[[nodiscard]] CartridgeSlotManager& getSlotManager() { return *slotManager; }
	[[nodiscard]] RealTime& getRealTime() { return *realTime; }
	[[nodiscard]] Debugger& getDebugger() { return *debugger; }
	[[nodiscard]] MSXMixer& getMSXMixer() { return *msxMixer; }
	[[nodiscard]] PluggingController& getPluggingController();
	[[nodiscard]] MSXCPU& getCPU();
	[[nodiscard]] MSXCPUInterface& getCPUInterface();
	[[nodiscard]] PanasonicMemory& getPanasonicMemory();
	[[nodiscard]] MSXDeviceSwitch& getDeviceSwitch();
	[[nodiscard]] CassettePortInterface& getCassettePort();
	[[nodiscard]] JoystickPortIf& getJoystickPort(unsigned port);
	[[nodiscard]] RenShaTurbo& getRenShaTurbo();
	[[nodiscard]] LedStatus& getLedStatus();
	[[nodiscard]] ReverseManager& getReverseManager() { return *reverseManager; }
	[[nodiscard]] Reactor& getReactor() { return reactor; }
	[[nodiscard]] VideoSourceSetting& getVideoSource() { return videoSourceSetting; }
	[[nodiscard]] BooleanSetting& suppressMessages() { return suppressMessagesSetting; }

	// convenience methods
	[[nodiscard]] CommandController& getCommandController();
	[[nodiscard]] InfoCommand& getMachineInfoCommand();

	/** Convenience method:
	  * This is the same as getScheduler().getCurrentTime(). */
	[[nodiscard]] EmuTime::param getCurrentTime();

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
	[[nodiscard]] MSXDevice* findDevice(std::string_view name);

	/** Some MSX device parts are shared between several MSX devices
	  * (e.g. all memory mappers share IO ports 0xFC-0xFF). But this
	  * sharing is limited to one MSX machine. This method offers the
	  * storage to implement per-machine reference counted objects.
	  * TODO This doesn't play nicely with savestates. For example memory
	  *      mappers don't use this mechanism anymore because of this.
	  *      Maybe this method can be removed when savestates are finished.
	  */
	template<typename T, typename ... Args>
	[[nodiscard]] std::shared_ptr<T> getSharedStuff(std::string_view name, Args&& ...args)
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
	[[nodiscard]] MSXMapperIO& createMapperIO();
	[[nodiscard]] MSXMapperIO& getMapperIO() const
	{
		assert(mapperIOCounter);
		return *mapperIO;
	}
	void destroyMapperIO();

	/** Keep track of which 'usernames' are in use.
	 * For example to be able to use several fmpac extensions at once, each
	 * with its own SRAM file, we need to generate unique filenames. We
	 * also want to reuse existing filenames as much as possible.
	 * ATM the usernames always have the format 'untitled[N]'. In the future
	 * we might allow really user specified names.
	 */
	[[nodiscard]] std::string getUserName(const std::string& hwName);
	void freeUserName(const std::string& hwName, const std::string& userName);

	/** Register and unregister providers of media info, for the media info
	 * topic.
	 */
	void registerMediaInfo(std::string_view name, MediaInfoProvider& provider);
	void unregisterMediaInfo(MediaInfoProvider& provider);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void powerDown();
	void deleteMachine();

private:
	Reactor& reactor;
	std::string machineID;
	std::string machineName;

	std::vector<MSXDevice*> availableDevices; // no ownership, no order

	hash_map<std::string_view, std::weak_ptr<void>,      XXHasher> sharedStuffMap;
	hash_map<std::string, std::vector<std::string>, XXHasher> userNames;

	std::unique_ptr<MSXMapperIO> mapperIO;
	unsigned mapperIOCounter = 0;

	// These two should normally be the same, only during savestate loading
	// machineConfig will already be filled in, but machineConfig2 not yet.
	// This is important when an exception happens during loading of
	// machineConfig2 (otherwise machineConfig2 gets deleted twice).
	// See also HardwareConfig::serialize() and setMachineConfig()
	std::unique_ptr<HardwareConfig> machineConfig2;
	HardwareConfig* machineConfig = nullptr;

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
	// machineMediaInfo must be BEFORE PluggingController!
	std::unique_ptr<MachineMediaInfo> machineMediaInfo;
	std::unique_ptr<PluggingController> pluggingController;
	std::unique_ptr<MSXCPU> msxCpu;
	std::unique_ptr<MSXCPUInterface> msxCpuInterface;
	std::unique_ptr<PanasonicMemory> panasonicMemory;
	std::unique_ptr<MSXDeviceSwitch> deviceSwitch;
	std::unique_ptr<CassettePortInterface> cassettePort;
	std::array<std::unique_ptr<JoystickPortIf>, 2> joystickPort;
	std::unique_ptr<JoyPortDebuggable> joyPortDebuggable;
	std::unique_ptr<RenShaTurbo> renShaTurbo;
	std::unique_ptr<LedStatus> ledStatus;
	VideoSourceSetting videoSourceSetting;
	BooleanSetting suppressMessagesSetting;

	std::unique_ptr<CartridgeSlotManager> slotManager;
	std::unique_ptr<ReverseManager> reverseManager;
	std::unique_ptr<ResetCmd>     resetCommand;
	std::unique_ptr<LoadMachineCmd> loadMachineCommand;
	std::unique_ptr<ListExtCmd>   listExtCommand;
	std::unique_ptr<ExtCmd>       extCommand;
	std::unique_ptr<RemoveExtCmd> removeExtCommand;
	std::unique_ptr<MachineNameInfo> machineNameInfo;
	std::unique_ptr<MachineTypeInfo> machineTypeInfo;
	std::unique_ptr<MachineExtensionInfo> machineExtensionInfo;
	std::unique_ptr<DeviceInfo>   deviceInfo;
	friend class DeviceInfo;

	std::unique_ptr<FastForwardHelper> fastForwardHelper;

	std::unique_ptr<SettingObserver> settingObserver;
	friend class SettingObserver;
	BooleanSetting& powerSetting;

	bool powered = false;
	bool active = false;
	bool fastForwarding = false;
};
SERIALIZE_CLASS_VERSION(MSXMotherBoard, 5);

class ExtCmd final : public RecordedCommand
{
public:
	ExtCmd(MSXMotherBoard& motherBoard, std::string commandName);
	void execute(std::span<const TclObject> tokens, TclObject& result,
	             EmuTime::param time) override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
private:
	MSXMotherBoard& motherBoard;
	std::string commandName;
};

} // namespace openmsx

#endif

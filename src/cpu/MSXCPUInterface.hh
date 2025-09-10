#ifndef MSXCPUINTERFACE_HH
#define MSXCPUINTERFACE_HH

#include "BreakPoint.hh"
#include "CacheLine.hh"
#include "DebugCondition.hh"
#include "WatchPoint.hh"

#include "SimpleDebuggable.hh"
#include "InfoTopic.hh"
#include "MSXDevice.hh"
#include "ProfileCounters.hh"

#include "narrow.hh"

#include <array>
#include <cstdint>
#include <bitset>
#include <concepts>
#include <memory>
#include <vector>

namespace openmsx {

class BooleanSetting;
class BreakPoint;
class CliComm;
class DummyDevice;
class MSXCPU;
class MSXMotherBoard;
class VDPIODelay;

inline constexpr bool PROFILE_CACHELINES = false;
enum class CacheLineCounters : uint8_t {
	NonCachedRead,
	NonCachedWrite,
	GetReadCacheLine,
	GetWriteCacheLine,
	SlowRead,
	SlowWrite,
	DisallowCacheRead,
	DisallowCacheWrite,
	InvalidateAllSlots,
	InvalidateReadWrite,
	InvalidateRead,
	InvalidateWrite,
	FillReadWrite,
	FillRead,
	FillWrite,
	NUM // must be last
};
std::ostream& operator<<(std::ostream& os, EnumTypeName<CacheLineCounters>);
std::ostream& operator<<(std::ostream& os, EnumValueName<CacheLineCounters> evn);

class MSXCPUInterface : public ProfileCounters<PROFILE_CACHELINES, CacheLineCounters>
{
public:
	explicit MSXCPUInterface(MSXMotherBoard& motherBoard);
	MSXCPUInterface(const MSXCPUInterface&) = delete;
	MSXCPUInterface(MSXCPUInterface&&) = delete;
	MSXCPUInterface& operator=(const MSXCPUInterface&) = delete;
	MSXCPUInterface& operator=(MSXCPUInterface&&) = delete;
	~MSXCPUInterface();

	/**
	 * Devices can register their In ports. This is normally done
	 * in their constructor. Once device are registered, their
	 * readIO() method can get called.
	 */
	void register_IO_In(uint8_t port, MSXDevice* device);
	void unregister_IO_In(uint8_t port, MSXDevice* device);

	/**
	 * Devices can register their Out ports. This is normally done
	 * in their constructor. Once device are registered, their
	 * writeIO() method can get called.
	 */
	void register_IO_Out(uint8_t port, MSXDevice* device);
	void unregister_IO_Out(uint8_t port, MSXDevice* device);

	/** Convenience methods for {un}register_IO_{In,Out}.
	 * At the same time (un)register both In and Out, and/or a range of ports.
	 */
	void register_IO_InOut(uint8_t port, MSXDevice* device);
	void register_IO_In_range(uint8_t port, unsigned num, MSXDevice* device);
	void register_IO_Out_range(uint8_t port, unsigned num, MSXDevice* device);
	void register_IO_InOut_range(uint8_t port, unsigned num, MSXDevice* device);
	void unregister_IO_InOut(uint8_t port, MSXDevice* device);
	void unregister_IO_In_range(uint8_t port, unsigned num, MSXDevice* device);
	void unregister_IO_Out_range(uint8_t port, unsigned num, MSXDevice* device);
	void unregister_IO_InOut_range(uint8_t port, unsigned num, MSXDevice* device);

	/**
	 * These methods replace a previously registered device with a new one.
	 *
	 * This method checks whether the current device is the same as the
	 * 'oldDevice' parameter.
	 * - If it's the same, the current device is replace with 'newDevice'
	 *   and this method returns true.
	 * - If it's not the same, then no changes are made and this method
	 *   returns false.
	 *
	 * The intention is that devices using these methods extend (=wrap) the
	 * functionality of a previously registered device. Typically the
	 * destructor of the wrapping device will perform the inverse
	 * replacement.
	 */
	bool replace_IO_In (uint8_t port, MSXDevice* oldDevice, MSXDevice* newDevice);
	bool replace_IO_Out(uint8_t port, MSXDevice* oldDevice, MSXDevice* newDevice);

	/**
	 * Devices can register themself in the MSX slot structure.
	 * This is normally done in their constructor. Once devices
	 * are registered their readMem() / writeMem() methods can
	 * get called.
	 */
	void registerMemDevice(MSXDevice& device,
	                       int ps, int ss, unsigned base, unsigned size);
	void unregisterMemDevice(MSXDevice& device,
	                         int ps, int ss, unsigned base, unsigned size);

	/** (Un)register global writes.
	  * @see MSXDevice::globalWrite()
	  */
	void   registerGlobalWrite(MSXDevice& device, uint16_t address);
	void unregisterGlobalWrite(MSXDevice& device, uint16_t address);

	/** (Un)register global read.
	  * @see MSXDevice::globalRead()
	  */
	void   registerGlobalRead(MSXDevice& device, uint16_t address);
	void unregisterGlobalRead(MSXDevice& device, uint16_t address);

	/**
	 * Reset (the slot state)
	 */
	void reset();

	/**
	 * This reads a byte from the currently selected device
	 */
	uint8_t readMem(uint16_t address, EmuTime time) {
		tick(CacheLineCounters::SlowRead);
		if (disallowReadCache[address >> CacheLine::BITS]) [[unlikely]] {
			return readMemSlow(address, time);
		}
		return visibleDevices[address >> 14]->readMem(address, time);
	}

	/**
	 * This writes a byte to the currently selected device
	 */
	void writeMem(uint16_t address, uint8_t value, EmuTime time) {
		tick(CacheLineCounters::SlowWrite);
		if (disallowWriteCache[address >> CacheLine::BITS]) [[unlikely]] {
			writeMemSlow(address, value, time);
			return;
		}
		visibleDevices[address>>14]->writeMem(address, value, time);
	}

	/**
	 * This read a byte from the given IO-port
	 * @see MSXDevice::readIO()
	 */
	uint8_t readIO(uint16_t port, EmuTime time) {
		return IO_In[port & 0xFF]->readIO(port, time);
	}

	/**
	 * This writes a byte to the given IO-port
	 * @see MSXDevice::writeIO()
	 */
	void writeIO(uint16_t port, uint8_t value, EmuTime time) {
		IO_Out[port & 0xFF]->writeIO(port, value, time);
	}

	/**
	 * Test that the memory in the interval [start, start +
	 * CacheLine::SIZE) is cacheable for reading. If it is, a pointer to a
	 * buffer containing this interval must be returned. If not, a null
	 * pointer must be returned.
	 * Cacheable for reading means the data may be read directly
	 * from the buffer, thus bypassing the readMem() method, and
	 * thus also ignoring EmuTime.
	 * The default implementation always returns a null pointer.
	 * An interval will never cross a 16KB border.
	 * An interval will never contain the address 0xffff.
	 */
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t start) const {
		tick(CacheLineCounters::GetReadCacheLine);
		if (disallowReadCache[start >> CacheLine::BITS]) [[unlikely]] {
			return nullptr;
		}
		return visibleDevices[start >> 14]->getReadCacheLine(start);
	}

	/**
	 * Test that the memory in the interval [start, start +
	 * CacheLine::SIZE) is cacheable for writing. If it is, a pointer to a
	 * buffer containing this interval must be returned. If not, a null
	 * pointer must be returned.
	 * Cacheable for writing means the data may be written directly
	 * to the buffer, thus bypassing the writeMem() method, and
	 * thus also ignoring EmuTime.
	 * The default implementation always returns a null pointer.
	 * An interval will never cross a 16KB border.
	 * An interval will never contain the address 0xffff.
	 */
	[[nodiscard]] uint8_t* getWriteCacheLine(uint16_t start) {
		tick(CacheLineCounters::GetWriteCacheLine);
		if (disallowWriteCache[start >> CacheLine::BITS]) [[unlikely]] {
			return nullptr;
		}
		return visibleDevices[start >> 14]->getWriteCacheLine(start);
	}

	/**
	 * CPU uses this method to read 'extra' data from the data bus
	 * used in interrupt routines. In MSX this returns always 255.
	 */
	[[nodiscard]] uint8_t readIRQVector() const;

	/*
	 * Should only be used by PPI
	 *  TODO: make private / friend
	 */
	void setPrimarySlots(uint8_t value);

	/** @see MSXCPU::invalidateRWCache() */
	void invalidateRWCache(uint16_t start, unsigned size, int ps, int ss);
	void invalidateRCache (uint16_t start, unsigned size, int ps, int ss);
	void invalidateWCache (uint16_t start, unsigned size, int ps, int ss);

	/** @see MSXCPU::fillRWCache() */
	void fillRWCache(unsigned start, unsigned size, const uint8_t* rData, uint8_t* wData, int ps, int ss);
	void fillRCache (unsigned start, unsigned size, const uint8_t* rData,                 int ps, int ss);
	void fillWCache (unsigned start, unsigned size,                       uint8_t* wData, int ps, int ss);

	/**
	 * Peek memory location
	 * @see MSXDevice::peekMem()
	 */
	[[nodiscard]] uint8_t peekMem(uint16_t address, EmuTime time) const;
	[[nodiscard]] uint8_t peekSlottedMem(unsigned address, EmuTime time) const;
	void peekSlottedMemBlock(unsigned address, std::span<uint8_t> output, EmuTime time) const;
	uint8_t readSlottedMem(unsigned address, EmuTime time);
	void writeSlottedMem(unsigned address, uint8_t value,
	                     EmuTime time);

	void setExpanded(int ps);
	void unsetExpanded(int ps);
	void testUnsetExpanded(int ps,
		               std::span<const std::unique_ptr<MSXDevice>> allowed) const;
	[[nodiscard]] bool isExpanded(int ps) const { return expanded[ps] != 0; }
	void changeExpanded(bool newExpanded);

	[[nodiscard]] auto getPrimarySlot  (int page) const { return primarySlotState[page]; }
	[[nodiscard]] auto getSecondarySlot(int page) const { return secondarySlotState[page]; }

	[[nodiscard]] DummyDevice& getDummyDevice() { return *dummyDevice; }

	void insertBreakPoint(BreakPoint bp);
	void removeBreakPoint(const BreakPoint& bp);
	void removeBreakPoint(unsigned id);
	using BreakPoints = std::vector<BreakPoint>;
	[[nodiscard]] static BreakPoints& getBreakPoints() { return breakPoints; }

	void setWatchPoint(const std::shared_ptr<WatchPoint>& watchPoint);
	void removeWatchPoint(std::shared_ptr<WatchPoint> watchPoint);
	void removeWatchPoint(unsigned id);
	// note: must be shared_ptr (not unique_ptr), see WatchIO::doReadCallback()
	using WatchPoints = std::vector<std::shared_ptr<WatchPoint>>;
	[[nodiscard]] const WatchPoints& getWatchPoints() const { return watchPoints; }
	[[nodiscard]] WatchPoints& getWatchPoints() { return watchPoints; }

	// Temporarily unregister and then re-register a watchpoint. E.g.
	// because you want to change the type or address, and then it needs to
	// be registered in a different way.
	struct ScopedChangeWatchpoint {
		ScopedChangeWatchpoint(const ScopedChangeWatchpoint&) = delete;
		ScopedChangeWatchpoint(ScopedChangeWatchpoint&&) = delete;
		ScopedChangeWatchpoint& operator=(const ScopedChangeWatchpoint&) = delete;
		ScopedChangeWatchpoint& operator=(ScopedChangeWatchpoint&&) = delete;

		ScopedChangeWatchpoint(MSXCPUInterface& interface_, std::shared_ptr<WatchPoint> wp_)
			: interface(interface_), wp(std::move(wp_)) {
			interface.unregisterWatchPoint(*wp);
		}
		~ScopedChangeWatchpoint() {
			interface.registerWatchPoint(*wp);
		}
	private:
		MSXCPUInterface& interface;
		std::shared_ptr<WatchPoint> wp;
	};
	[[nodiscard]] auto getScopedChangeWatchpoint(std::shared_ptr<WatchPoint> wp) {
		return ScopedChangeWatchpoint(*this, std::move(wp));
	}

	void setCondition(DebugCondition cond);
	void removeCondition(const DebugCondition& cond);
	void removeCondition(unsigned id);
	using Conditions = std::vector<DebugCondition>;
	[[nodiscard]] static Conditions& getConditions() { return conditions; }

	[[nodiscard]] static bool isBreaked() { return breaked; }
	void doBreak();
	void doStep();
	void doContinue();

	// breakpoint methods used by CPUCore
	[[nodiscard]] static bool anyBreakPoints()
	{
		return !breakPoints.empty() || !conditions.empty();
	}
	[[nodiscard]] bool checkBreakPoints(unsigned pc);

	// cleanup global variables
	static void cleanup();

	// In fast-forward mode, breakpoints, watchpoints and conditions should
	// not trigger.
	void setFastForward(bool fastForward_) { fastForward = fastForward_; }
	[[nodiscard]] bool isFastForward() const { return fastForward; }

	[[nodiscard]] MSXDevice* getMSXDevice(int ps, int ss, int page);
	[[nodiscard]] MSXDevice* getVisibleMSXDevice(int page) { return visibleDevices[page]; }

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	uint8_t readMemSlow(uint16_t address, EmuTime time);
	void writeMemSlow(uint16_t address, uint8_t value, EmuTime time);

	MSXDevice*& getDevicePtr(uint8_t port, bool isIn);

	void register_IO  (int port, bool isIn,
	                   MSXDevice*& devicePtr, MSXDevice* device);
	void unregister_IO(MSXDevice*& devicePtr, MSXDevice* device);
	void testRegisterSlot(const MSXDevice& device,
	                      int ps, int ss, unsigned base, unsigned size);
	void registerSlot(MSXDevice& device,
	                  int ps, int ss, unsigned base, unsigned size);
	void unregisterSlot(MSXDevice& device,
	                    int ps, int ss, unsigned base, unsigned size);

	void registerWatchPoint(WatchPoint& wp);
	void unregisterWatchPoint(WatchPoint& wp);

	void removeAllWatchPoints();
	void updateMemWatch(WatchPoint::Type type);
	void executeMemWatch(WatchPoint::Type type, unsigned address,
	                     unsigned value = ~0u);

	struct MemoryDebug final : SimpleDebuggable {
		explicit MemoryDebug(MSXMotherBoard& motherBoard);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} memoryDebug;

	struct SlottedMemoryDebug final : SimpleDebuggable {
		explicit SlottedMemoryDebug(MSXMotherBoard& motherBoard);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void readBlock(unsigned start, std::span<uint8_t> output) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} slottedMemoryDebug;

	struct IODebug final : SimpleDebuggable {
		explicit IODebug(MSXMotherBoard& motherBoard);
		[[nodiscard]] uint8_t read(unsigned address, EmuTime time) override;
		void write(unsigned address, uint8_t value, EmuTime time) override;
	} ioDebug;

	struct SlotInfo final : InfoTopic {
		explicit SlotInfo(InfoCommand& machineInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} slotInfo;

	struct SubSlottedInfo final : InfoTopic {
		explicit SubSlottedInfo(InfoCommand& machineInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} subSlottedInfo;

	struct ExternalSlotInfo final : InfoTopic {
		explicit ExternalSlotInfo(InfoCommand& machineInfoCommand);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) const override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	} externalSlotInfo;

	struct IOInfo : InfoTopic {
		IOInfo(InfoCommand& machineInfoCommand, const char* name);
		void helper(std::span<const TclObject> tokens,
		            TclObject& result, std::span<MSXDevice*, 256> devices) const;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	protected:
		~IOInfo() = default;
	};
	struct IInfo final : IOInfo {
		explicit IInfo(InfoCommand& machineInfoCommand)
			: IOInfo(machineInfoCommand, "input_port") {}
		void execute(std::span<const TclObject> tokens,
		             TclObject& result) const override;
	} inputPortInfo;
	struct OInfo final : IOInfo {
		explicit OInfo(InfoCommand& machineInfoCommand)
			: IOInfo(machineInfoCommand, "output_port") {}
		void execute(std::span<const TclObject> tokens,
		             TclObject& result) const override;
	} outputPortInfo;

	/** Updated visibleDevices for a given page and clears the cache
	  * on changes.
	  * Should be called whenever PrimarySlotState or SecondarySlotState
	  * was modified.
	  * @param page page [0..3] to update visibleDevices for.
	  */
	void updateVisible(uint8_t page);
	inline void updateVisible(uint8_t page, uint8_t ps, uint8_t ss);
	void setSubSlot(uint8_t primSlot, uint8_t value);

	std::unique_ptr<DummyDevice> dummyDevice;
	MSXCPU& msxcpu;
	CliComm& cliComm;
	MSXMotherBoard& motherBoard;
	BooleanSetting& pauseSetting;

	std::unique_ptr<VDPIODelay> delayDevice; // can be nullptr

	std::array<uint8_t, CacheLine::NUM> disallowReadCache;
	std::array<uint8_t, CacheLine::NUM> disallowWriteCache;
	std::array<std::bitset<CacheLine::SIZE>, CacheLine::NUM> readWatchSet;
	std::array<std::bitset<CacheLine::SIZE>, CacheLine::NUM> writeWatchSet;

	struct GlobalRwInfo {
		MSXDevice* device;
		uint16_t addr;
		[[nodiscard]] constexpr bool operator==(const GlobalRwInfo&) const = default;
	};
	std::vector<GlobalRwInfo> globalReads;
	std::vector<GlobalRwInfo> globalWrites;

	std::array<MSXDevice*, 256> IO_In;
	std::array<MSXDevice*, 256> IO_Out;
	std::array<std::array<std::array<MSXDevice*, 4>, 4>, 4> slotLayout;
	std::array<MSXDevice*, 4> visibleDevices;
	std::array<uint8_t, 4> subSlotRegister;
	std::array<uint8_t, 4> primarySlotState;
	std::array<uint8_t, 4> secondarySlotState;
	uint8_t initialPrimarySlots;
	std::array<unsigned, 4> expanded;

	bool fastForward = false; // no need to serialize

	//  All CPUs (Z80 and R800) of all MSX machines share this state.
	static inline BreakPoints breakPoints; // unsorted
	WatchPoints watchPoints; // ordered in creation order,  TODO must also be static
	static inline Conditions conditions; // ordered in creation order
	static inline bool breaked = false;
};


// Compile-Time Interval (half-open).
//   TODO possibly move this to utils/
template<unsigned BEGIN, unsigned END = BEGIN + 1>
struct CT_Interval
{
	[[nodiscard]] unsigned begin() const { return BEGIN; } // inclusive
	[[nodiscard]] unsigned end()   const { return END; }   // exclusive
};

// Execute an 'action' for every element in the given interval(s).
inline void foreach_ct_interval(std::invocable<unsigned> auto action, auto ct_interval)
{
	for (auto i = ct_interval.begin(); i != ct_interval.end(); ++i) {
		action(i);
	}
}
inline void foreach_ct_interval(std::invocable<unsigned> auto action,
                                auto front_interval, auto... tail_intervals)
{
	foreach_ct_interval(action, front_interval);
	foreach_ct_interval(action, tail_intervals...);
}


template<typename MSXDEVICE, typename... CT_INTERVALS>
struct GlobalRWHelper
{
	void execute(std::invocable<MSXCPUInterface&, MSXDevice&, unsigned> auto action)
	{
		auto& dev = static_cast<MSXDEVICE&>(*this);
		auto& cpu = dev.getCPUInterface();
		foreach_ct_interval(
			[&](unsigned addr) { action(cpu, dev, addr); },
			CT_INTERVALS()...);
	}
};

template<typename MSXDEVICE, typename... CT_INTERVALS>
struct GlobalWriteClient : GlobalRWHelper<MSXDEVICE, CT_INTERVALS...>
{
	GlobalWriteClient(const GlobalWriteClient&) = delete;
	GlobalWriteClient(GlobalWriteClient&&) = delete;
	GlobalWriteClient& operator=(const GlobalWriteClient&) = delete;
	GlobalWriteClient& operator=(GlobalWriteClient&&) = delete;

	GlobalWriteClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.registerGlobalWrite(dev, narrow<uint16_t>(addr));
		});
	}

	~GlobalWriteClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.unregisterGlobalWrite(dev, narrow<uint16_t>(addr));
		});
	}
};

template<typename MSXDEVICE, typename... CT_INTERVALS>
struct GlobalReadClient : GlobalRWHelper<MSXDEVICE, CT_INTERVALS...>
{
	GlobalReadClient(const GlobalReadClient&) = delete;
	GlobalReadClient(GlobalReadClient&&) = delete;
	GlobalReadClient& operator=(const GlobalReadClient&) = delete;
	GlobalReadClient& operator=(GlobalReadClient&&) = delete;

	GlobalReadClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.registerGlobalRead(dev, narrow<uint16_t>(addr));
		});
	}

	~GlobalReadClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.unregisterGlobalRead(dev, narrow<uint16_t>(addr));
		});
	}
};

} // namespace openmsx

#endif

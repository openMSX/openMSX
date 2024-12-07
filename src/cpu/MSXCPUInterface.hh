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
#include "openmsx.hh"

#include "narrow.hh"
#include "ranges.hh"

#include <array>
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
enum class CacheLineCounters {
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
	void register_IO_In(byte port, MSXDevice* device);
	void unregister_IO_In(byte port, MSXDevice* device);

	/**
	 * Devices can register their Out ports. This is normally done
	 * in their constructor. Once device are registered, their
	 * writeIO() method can get called.
	 */
	void register_IO_Out(byte port, MSXDevice* device);
	void unregister_IO_Out(byte port, MSXDevice* device);

	/** Convenience methods for {un}register_IO_{In,Out}.
	 * At the same time (un)register both In and Out, and/or a range of ports.
	 */
	void register_IO_InOut(byte port, MSXDevice* device);
	void register_IO_In_range(byte port, unsigned num, MSXDevice* device);
	void register_IO_Out_range(byte port, unsigned num, MSXDevice* device);
	void register_IO_InOut_range(byte port, unsigned num, MSXDevice* device);
	void unregister_IO_InOut(byte port, MSXDevice* device);
	void unregister_IO_In_range(byte port, unsigned num, MSXDevice* device);
	void unregister_IO_Out_range(byte port, unsigned num, MSXDevice* device);
	void unregister_IO_InOut_range(byte port, unsigned num, MSXDevice* device);

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
	bool replace_IO_In (byte port, MSXDevice* oldDevice, MSXDevice* newDevice);
	bool replace_IO_Out(byte port, MSXDevice* oldDevice, MSXDevice* newDevice);

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
	void   registerGlobalWrite(MSXDevice& device, word address);
	void unregisterGlobalWrite(MSXDevice& device, word address);

	/** (Un)register global read.
	  * @see MSXDevice::globalRead()
	  */
	void   registerGlobalRead(MSXDevice& device, word address);
	void unregisterGlobalRead(MSXDevice& device, word address);

	/**
	 * Reset (the slot state)
	 */
	void reset();

	/**
	 * This reads a byte from the currently selected device
	 */
	byte readMem(word address, EmuTime::param time) {
		tick(CacheLineCounters::SlowRead);
		if (disallowReadCache[address >> CacheLine::BITS]) [[unlikely]] {
			return readMemSlow(address, time);
		}
		return visibleDevices[address >> 14]->readMem(address, time);
	}

	/**
	 * This writes a byte to the currently selected device
	 */
	void writeMem(word address, byte value, EmuTime::param time) {
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
	byte readIO(word port, EmuTime::param time) {
		return IO_In[port & 0xFF]->readIO(port, time);
	}

	/**
	 * This writes a byte to the given IO-port
	 * @see MSXDevice::writeIO()
	 */
	void writeIO(word port, byte value, EmuTime::param time) {
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
	[[nodiscard]] const byte* getReadCacheLine(word start) const {
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
	[[nodiscard]] byte* getWriteCacheLine(word start) {
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
	[[nodiscard]] byte readIRQVector() const;

	/*
	 * Should only be used by PPI
	 *  TODO: make private / friend
	 */
	void setPrimarySlots(byte value);

	/** @see MSXCPU::invalidateRWCache() */
	void invalidateRWCache(word start, unsigned size, int ps, int ss);
	void invalidateRCache (word start, unsigned size, int ps, int ss);
	void invalidateWCache (word start, unsigned size, int ps, int ss);

	/** @see MSXCPU::fillRWCache() */
	void fillRWCache(unsigned start, unsigned size, const byte* rData, byte* wData, int ps, int ss);
	void fillRCache (unsigned start, unsigned size, const byte* rData,              int ps, int ss);
	void fillWCache (unsigned start, unsigned size,                    byte* wData, int ps, int ss);

	/**
	 * Peek memory location
	 * @see MSXDevice::peekMem()
	 */
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const;
	[[nodiscard]] byte peekSlottedMem(unsigned address, EmuTime::param time) const;
	byte readSlottedMem(unsigned address, EmuTime::param time);
	void writeSlottedMem(unsigned address, byte value,
	                     EmuTime::param time);

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
	[[nodiscard]] static const BreakPoints& getBreakPoints() { return breakPoints; }

	void setWatchPoint(const std::shared_ptr<WatchPoint>& watchPoint);
	void removeWatchPoint(std::shared_ptr<WatchPoint> watchPoint);
	void removeWatchPoint(unsigned id);
	// note: must be shared_ptr (not unique_ptr), see WatchIO::doReadCallback()
	using WatchPoints = std::vector<std::shared_ptr<WatchPoint>>;
	[[nodiscard]] const WatchPoints& getWatchPoints() const { return watchPoints; }

	void setCondition(DebugCondition cond);
	void removeCondition(const DebugCondition& cond);
	void removeCondition(unsigned id);
	using Conditions = std::vector<DebugCondition>;
	[[nodiscard]] static const Conditions& getConditions() { return conditions; }

	[[nodiscard]] static bool isBreaked() { return breaked; }
	void doBreak();
	void doStep();
	void doContinue();

	// breakpoint methods used by CPUCore
	[[nodiscard]] static bool anyBreakPoints()
	{
		return !breakPoints.empty() || !conditions.empty();
	}
	[[nodiscard]] bool checkBreakPoints(unsigned pc)
	{
		auto range = ranges::equal_range(breakPoints, pc, {}, &BreakPoint::getAddress);
		if (conditions.empty() && (range.first == range.second)) {
			return false;
		}

		// slow path non-inlined
		checkBreakPoints(range);
		return isBreaked();
	}

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
	byte readMemSlow(word address, EmuTime::param time);
	void writeMemSlow(word address, byte value, EmuTime::param time);

	MSXDevice*& getDevicePtr(byte port, bool isIn);

	void register_IO  (int port, bool isIn,
	                   MSXDevice*& devicePtr, MSXDevice* device);
	void unregister_IO(MSXDevice*& devicePtr, MSXDevice* device);
	void testRegisterSlot(const MSXDevice& device,
	                      int ps, int ss, unsigned base, unsigned size);
	void registerSlot(MSXDevice& device,
	                  int ps, int ss, unsigned base, unsigned size);
	void unregisterSlot(MSXDevice& device,
	                    int ps, int ss, unsigned base, unsigned size);


	void checkBreakPoints(std::pair<BreakPoints::const_iterator,
	                                BreakPoints::const_iterator> range);

	void removeAllWatchPoints();
	void updateMemWatch(WatchPoint::Type type);
	void executeMemWatch(WatchPoint::Type type, unsigned address,
	                     unsigned value = ~0u);

	struct MemoryDebug final : SimpleDebuggable {
		explicit MemoryDebug(MSXMotherBoard& motherBoard);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} memoryDebug;

	struct SlottedMemoryDebug final : SimpleDebuggable {
		explicit SlottedMemoryDebug(MSXMotherBoard& motherBoard);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} slottedMemoryDebug;

	struct IODebug final : SimpleDebuggable {
		explicit IODebug(MSXMotherBoard& motherBoard);
		[[nodiscard]] byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
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
	void updateVisible(byte page);
	inline void updateVisible(byte page, byte ps, byte ss);
	void setSubSlot(byte primSlot, byte value);

	std::unique_ptr<DummyDevice> dummyDevice;
	MSXCPU& msxcpu;
	CliComm& cliComm;
	MSXMotherBoard& motherBoard;
	BooleanSetting& pauseSetting;

	std::unique_ptr<VDPIODelay> delayDevice; // can be nullptr

	std::array<byte, CacheLine::NUM> disallowReadCache;
	std::array<byte, CacheLine::NUM> disallowWriteCache;
	std::array<std::bitset<CacheLine::SIZE>, CacheLine::NUM> readWatchSet;
	std::array<std::bitset<CacheLine::SIZE>, CacheLine::NUM> writeWatchSet;

	struct GlobalRwInfo {
		MSXDevice* device;
		word addr;
		[[nodiscard]] constexpr bool operator==(const GlobalRwInfo&) const = default;
	};
	std::vector<GlobalRwInfo> globalReads;
	std::vector<GlobalRwInfo> globalWrites;

	std::array<MSXDevice*, 256> IO_In;
	std::array<MSXDevice*, 256> IO_Out;
	std::array<std::array<std::array<MSXDevice*, 4>, 4>, 4> slotLayout;
	std::array<MSXDevice*, 4> visibleDevices;
	std::array<byte, 4> subSlotRegister;
	std::array<byte, 4> primarySlotState;
	std::array<byte, 4> secondarySlotState;
	byte initialPrimarySlots;
	std::array<unsigned, 4> expanded;

	bool fastForward = false; // no need to serialize

	//  All CPUs (Z80 and R800) of all MSX machines share this state.
	static inline BreakPoints breakPoints; // sorted on address
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
			cpu.registerGlobalWrite(dev, narrow<word>(addr));
		});
	}

	~GlobalWriteClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.unregisterGlobalWrite(dev, narrow<word>(addr));
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
			cpu.registerGlobalRead(dev, narrow<word>(addr));
		});
	}

	~GlobalReadClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.unregisterGlobalRead(dev, narrow<word>(addr));
		});
	}
};

} // namespace openmsx

#endif

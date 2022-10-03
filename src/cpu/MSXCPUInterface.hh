#ifndef MSXCPUINTERFACE_HH
#define MSXCPUINTERFACE_HH

#include "DebugCondition.hh"
#include "SimpleDebuggable.hh"
#include "InfoTopic.hh"
#include "CacheLine.hh"
#include "MSXDevice.hh"
#include "BreakPoint.hh"
#include "WatchPoint.hh"
#include "ProfileCounters.hh"
#include "openmsx.hh"
#include "ranges.hh"
#include <bitset>
#include <concepts>
#include <vector>
#include <memory>

namespace openmsx {

class VDPIODelay;
class DummyDevice;
class MSXMotherBoard;
class MSXCPU;
class CliComm;
class BreakPoint;

constexpr bool PROFILE_CACHELINES = false;
enum CacheLineCounters {
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
	MSXCPUInterface(const MSXCPUInterface&) = delete;
	MSXCPUInterface& operator=(const MSXCPUInterface&) = delete;

	explicit MSXCPUInterface(MSXMotherBoard& motherBoard);
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
	 * Devices can register themself in the MSX slotstructure.
	 * This is normally done in their constructor. Once devices
	 * are registered their readMem() / writeMem() methods can
	 * get called.
	 */
	void registerMemDevice(MSXDevice& device,
	                       int ps, int ss, int base, int size);
	void unregisterMemDevice(MSXDevice& device,
	                         int ps, int ss, int base, int size);

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
	inline byte readMem(word address, EmuTime::param time) {
		tick(CacheLineCounters::SlowRead);
		if (disallowReadCache[address >> CacheLine::BITS]) [[unlikely]] {
			return readMemSlow(address, time);
		}
		return visibleDevices[address >> 14]->readMem(address, time);
	}

	/**
	 * This writes a byte to the currently selected device
	 */
	inline void writeMem(word address, byte value, EmuTime::param time) {
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
	inline byte readIO(word port, EmuTime::param time) {
		return IO_In[port & 0xFF]->readIO(port, time);
	}

	/**
	 * This writes a byte to the given IO-port
	 * @see MSXDevice::writeIO()
	 */
	inline void writeIO(word port, byte value, EmuTime::param time) {
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
	[[nodiscard]] inline const byte* getReadCacheLine(word start) const {
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
	[[nodiscard]] inline byte* getWriteCacheLine(word start) const {
		tick(CacheLineCounters::GetWriteCacheLine);
		if (disallowWriteCache[start >> CacheLine::BITS]) [[unlikely]] {
			return nullptr;
		}
		return visibleDevices[start >> 14]->getWriteCacheLine(start);
	}

	/**
	 * CPU uses this method to read 'extra' data from the databus
	 * used in interrupt routines. In MSX this returns always 255.
	 */
	[[nodiscard]] byte readIRQVector();

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
	[[nodiscard]] inline bool isExpanded(int ps) const { return expanded[ps] != 0; }
	void changeExpanded(bool newExpanded);

	[[nodiscard]] DummyDevice& getDummyDevice() { return *dummyDevice; }

	void insertBreakPoint(BreakPoint bp);
	void removeBreakPoint(const BreakPoint& bp);
	using BreakPoints = std::vector<BreakPoint>;
	[[nodiscard]] static const BreakPoints& getBreakPoints() { return breakPoints; }

	void setWatchPoint(const std::shared_ptr<WatchPoint>& watchPoint);
	void removeWatchPoint(std::shared_ptr<WatchPoint> watchPoint);
	// note: must be shared_ptr (not unique_ptr), see WatchIO::doReadCallback()
	using WatchPoints = std::vector<std::shared_ptr<WatchPoint>>;
	[[nodiscard]] const WatchPoints& getWatchPoints() const { return watchPoints; }

	void setCondition(DebugCondition cond);
	void removeCondition(const DebugCondition& cond);
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readMemSlow(word address, EmuTime::param time);
	void writeMemSlow(word address, byte value, EmuTime::param time);

	MSXDevice*& getDevicePtr(byte port, bool isIn);

	void register_IO  (int port, bool isIn,
	                   MSXDevice*& devicePtr, MSXDevice* device);
	void unregister_IO(MSXDevice*& devicePtr, MSXDevice* device);
	void testRegisterSlot(MSXDevice& device,
	                      int ps, int ss, int base, int size);
	void registerSlot(MSXDevice& device,
	                  int ps, int ss, int base, int size);
	void unregisterSlot(MSXDevice& device,
	                    int ps, int ss, int base, int size);


	void checkBreakPoints(std::pair<BreakPoints::const_iterator,
	                                BreakPoints::const_iterator> range);
	void removeBreakPoint(unsigned id);
	void removeCondition(unsigned id);

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
		            TclObject& result, MSXDevice** devices) const;
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
	void updateVisible(int page);
	inline void updateVisible(int page, int ps, int ss);
	void setSubSlot(byte primSlot, byte value);

	std::unique_ptr<DummyDevice> dummyDevice;
	MSXCPU& msxcpu;
	CliComm& cliComm;
	MSXMotherBoard& motherBoard;

	std::unique_ptr<VDPIODelay> delayDevice; // can be nullptr

	byte disallowReadCache [CacheLine::NUM];
	byte disallowWriteCache[CacheLine::NUM];
	std::bitset<CacheLine::SIZE> readWatchSet [CacheLine::NUM];
	std::bitset<CacheLine::SIZE> writeWatchSet[CacheLine::NUM];

	struct GlobalRwInfo {
		MSXDevice* device;
		word addr;
		[[nodiscard]] constexpr bool operator==(const GlobalRwInfo&) const = default;
	};
	std::vector<GlobalRwInfo> globalReads;
	std::vector<GlobalRwInfo> globalWrites;

	MSXDevice* IO_In [256];
	MSXDevice* IO_Out[256];
	MSXDevice* slotLayout[4][4][4];
	MSXDevice* visibleDevices[4];
	byte subSlotRegister[4];
	byte primarySlotState[4];
	byte secondarySlotState[4];
	byte initialPrimarySlots;
	unsigned expanded[4];

	bool fastForward; // no need to serialize

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
	GlobalWriteClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.registerGlobalWrite(dev, addr);
		});
	}

	~GlobalWriteClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.unregisterGlobalWrite(dev, addr);
		});
	}
};

template<typename MSXDEVICE, typename... CT_INTERVALS>
struct GlobalReadClient : GlobalRWHelper<MSXDEVICE, CT_INTERVALS...>
{
	GlobalReadClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.registerGlobalRead(dev, addr);
		});
	}

	~GlobalReadClient()
	{
		this->execute([](MSXCPUInterface& cpu, MSXDevice& dev, unsigned addr) {
			cpu.unregisterGlobalRead(dev, addr);
		});
	}
};

} // namespace openmsx

#endif

#ifndef MSXCPUINTERFACE_HH
#define MSXCPUINTERFACE_HH

#include "SimpleDebuggable.hh"
#include "InfoTopic.hh"
#include "CacheLine.hh"
#include "MSXDevice.hh"
#include "BreakPoint.hh"
#include "WatchPoint.hh"
#include "openmsx.hh"
#include "likely.hh"
#include <algorithm>
#include <bitset>
#include <vector>
#include <memory>

namespace openmsx {

class VDPIODelay;
class DummyDevice;
class MSXMotherBoard;
class MSXCPU;
class CliComm;
class BreakPoint;
class DebugCondition;
class CartridgeSlotManager;

struct CompareBreakpoints {
	bool operator()(const BreakPoint& x, const BreakPoint& y) const {
		return x.getAddress() < y.getAddress();
	}
	bool operator()(const BreakPoint& x, word y) const {
		return x.getAddress() < y;
	}
	bool operator()(word x, const BreakPoint& y) const {
		return x < y.getAddress();
	}
};

class MSXCPUInterface
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
	 * Devices can register themself in the MSX slotstructure.
	 * This is normally done in their constructor. Once devices
	 * are registered their readMem() / writeMem() methods can
	 * get called.
	 */
	void registerMemDevice(MSXDevice& device,
	                       int primSl, int secSL, int base, int size);
	void unregisterMemDevice(MSXDevice& device,
	                         int primSl, int secSL, int base, int size);

	/** (Un)register global writes.
	  * @see MSXDevice::globalWrite()
	  */
	void   registerGlobalWrite(MSXDevice& device, word address);
	void unregisterGlobalWrite(MSXDevice& device, word address);

	/**
	 * Reset (the slot state)
	 */
	void reset();

	/**
	 * This reads a byte from the currently selected device
	 */
	inline byte readMem(word address, EmuTime::param time) {
		if (unlikely(disallowReadCache[address >> CacheLine::BITS])) {
			return readMemSlow(address, time);
		}
		return visibleDevices[address >> 14]->readMem(address, time);
	}

	/**
	 * This writes a byte to the currently selected device
	 */
	inline void writeMem(word address, byte value, EmuTime::param time) {
		if (unlikely(disallowWriteCache[address >> CacheLine::BITS])) {
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
	inline const byte* getReadCacheLine(word start) const {
		if (unlikely(disallowReadCache[start >> CacheLine::BITS])) {
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
	inline byte* getWriteCacheLine(word start) const {
		if (unlikely(disallowWriteCache[start >> CacheLine::BITS])) {
			return nullptr;
		}
		return visibleDevices[start >> 14]->getWriteCacheLine(start);
	}

	/**
	 * CPU uses this method to read 'extra' data from the databus
	 * used in interrupt routines. In MSX this returns always 255.
	 */
	byte readIRQVector();

	/*
	 * Should only be used by PPI
	 *  TODO: make private / friend
	 */
	void setPrimarySlots(byte value);

	/**
	 * Peek memory location
	 * @see MSXDevice::peekMem()
	 */
	byte peekMem(word address, EmuTime::param time) const;
	byte peekSlottedMem(unsigned address, EmuTime::param time) const;
	byte readSlottedMem(unsigned address, EmuTime::param time);
	void writeSlottedMem(unsigned address, byte value,
	                     EmuTime::param time);

	void setExpanded(int ps);
	void unsetExpanded(int ps);
	void testUnsetExpanded(int ps, std::vector<MSXDevice*> allowed) const;
	inline bool isExpanded(int ps) const { return expanded[ps] != 0; }
	void changeExpanded(bool isExpanded);

	DummyDevice& getDummyDevice() { return *dummyDevice; }

	static void insertBreakPoint(const BreakPoint& bp);
	static void removeBreakPoint(const BreakPoint& bp);
	using BreakPoints = std::vector<BreakPoint>;
	static const BreakPoints& getBreakPoints() { return breakPoints; }

	void setWatchPoint(const std::shared_ptr<WatchPoint>& watchPoint);
	void removeWatchPoint(std::shared_ptr<WatchPoint> watchPoint);
	// note: must be shared_ptr (not unique_ptr), see WatchIO::doReadCallback()
	using WatchPoints = std::vector<std::shared_ptr<WatchPoint>>;
	const WatchPoints& getWatchPoints() const { return watchPoints; }

	static void setCondition(const DebugCondition& cond);
	static void removeCondition(const DebugCondition& cond);
	using Conditions = std::vector<DebugCondition>;
	static const Conditions& getConditions() { return conditions; }

	static bool isBreaked() { return breaked; }
	void doBreak();
	void doStep();
	void doContinue();

	// should only be used by CPUCore
	static bool isStep()            { return step; }
	static void setStep    (bool x) { step = x; }
	static bool isContinue()        { return continued; }
	static void setContinue(bool x) { continued = x; }

	// breakpoint methods used by CPUCore
	static bool anyBreakPoints()
	{
		return !breakPoints.empty() || !conditions.empty();
	}
	static bool checkBreakPoints(unsigned pc, MSXMotherBoard& motherBoard)
	{
		auto range = equal_range(begin(breakPoints), end(breakPoints),
		                         pc, CompareBreakpoints());
		if (conditions.empty() && (range.first == range.second)) {
			return false;
		}

		// slow path non-inlined
		checkBreakPoints(range, motherBoard);
		return isBreaked();
	}

	// cleanup global variables
	static void cleanup();

	// In fast-forward mode, breakpoints, watchpoints and conditions should
	// not trigger.
	void setFastForward(bool fastForward_) { fastForward = fastForward_; }
	bool isFastForward() const { return fastForward; }

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


	static void checkBreakPoints(std::pair<BreakPoints::const_iterator,
	                                       BreakPoints::const_iterator> range,
	                             MSXMotherBoard& motherBoard);

	void removeAllWatchPoints();
	void registerIOWatch  (WatchPoint& watchPoint, MSXDevice** devices);
	void unregisterIOWatch(WatchPoint& watchPoint, MSXDevice** devices);
	void updateMemWatch(WatchPoint::Type type);
	void executeMemWatch(WatchPoint::Type type, unsigned address,
	                     unsigned value = ~0u);

	void doContinue2();

	struct MemoryDebug final : SimpleDebuggable {
		MemoryDebug(MSXMotherBoard& motherBoard);
		byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} memoryDebug;

	struct SlottedMemoryDebug final : SimpleDebuggable {
		SlottedMemoryDebug(MSXMotherBoard& motherBoard);
		byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} slottedMemoryDebug;

	struct IODebug final : SimpleDebuggable {
		IODebug(MSXMotherBoard& motherBoard);
		byte read(unsigned address, EmuTime::param time) override;
		void write(unsigned address, byte value, EmuTime::param time) override;
	} ioDebug;

	struct SlotInfo final : InfoTopic {
		SlotInfo(InfoCommand& machineInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} slotInfo;

	struct SubSlottedInfo final : InfoTopic {
		SubSlottedInfo(InfoCommand& machineInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} subSlottedInfo;

	struct ExternalSlotInfo final : InfoTopic {
		ExternalSlotInfo(InfoCommand& machineInfoCommand);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
	} externalSlotInfo;

	struct IOInfo : InfoTopic {
		IOInfo(InfoCommand& machineInfoCommand, const char* name);
		void helper(array_ref<TclObject> tokens,
		            TclObject& result, MSXDevice** devices) const;
		std::string help(const std::vector<std::string>& tokens) const override;
	};
	struct IInfo final : IOInfo {
		IInfo(InfoCommand& machineInfoCommand)
			: IOInfo(machineInfoCommand, "input_port") {}
		void execute(array_ref<TclObject> tokens,
		             TclObject& result) const override;
	} inputPortInfo;
	struct OInfo final : IOInfo {
		OInfo(InfoCommand& machineInfoCommand)
			: IOInfo(machineInfoCommand, "output_port") {}
		void execute(array_ref<TclObject> tokens,
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

	struct GlobalWriteInfo {
		MSXDevice* device;
		word addr;
		bool operator==(const GlobalWriteInfo& rhs) const {
			return (device == rhs.device) &&
			       (addr   == rhs.addr);
		}
	};
	std::vector<GlobalWriteInfo> globalWrites;

	MSXDevice* IO_In [256];
	MSXDevice* IO_Out[256];
	MSXDevice* slotLayout[4][4][4];
	MSXDevice* visibleDevices[4];
	byte subSlotRegister[4];
	byte primarySlotState[4];
	byte secondarySlotState[4];
	unsigned expanded[4];

	bool fastForward; // no need to serialize

	//  All CPUs (Z80 and R800) of all MSX machines share this state.
	static BreakPoints breakPoints; // sorted on address
	WatchPoints watchPoints; // ordered in creation order,  TODO must also be static
	static Conditions conditions; // ordered in creation order
	static bool breaked;
	static bool continued;
	static bool step;
};

} // namespace openmsx

#endif

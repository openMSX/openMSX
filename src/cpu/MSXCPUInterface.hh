// $Id$

#ifndef MSXCPUINTERFACE_HH
#define MSXCPUINTERFACE_HH

#include "CacheLine.hh"
#include "MSXDevice.hh"
#include "WatchPoint.hh"
#include "openmsx.hh"
#include "shared_ptr.hh"
#include "noncopyable.hh"
#include "likely.hh"
#include <bitset>
#include <vector>
#include <map>
#include <memory>

namespace openmsx {

class VDPIODelay;
class DummyDevice;
class MSXMotherBoard;
class MSXCPU;
class CliComm;
class MemoryDebug;
class SlottedMemoryDebug;
class IODebug;
class SlotInfo;
class SubSlottedInfo;
class ExternalSlotInfo;
class IOInfo;
class BreakPoint;
class DebugCondition;

class MSXCPUInterface : private noncopyable
{
public:
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
	 * Test that the memory in the interval [start, start+CacheLine::SIZE)
	 * is cacheable for reading. If it is, a pointer to a buffer
	 * containing this interval must be returned. If not, a null
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
			return NULL;
		}
		return visibleDevices[start >> 14]->getReadCacheLine(start);
	}

	/**
	 * Test that the memory in the interval [start, start+CacheLine::SIZE)
	 * is cacheable for writing. If it is, a pointer to a buffer
	 * containing this interval must be returned. If not, a null
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
			return NULL;
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
	void testUnsetExpanded(int ps, std::vector<MSXDevice*>& alreadyRemoved) const;
	inline bool isExpanded(int ps) const { return expanded[ps] != 0; }

	DummyDevice& getDummyDevice();

	static void insertBreakPoint(shared_ptr<BreakPoint> bp);
	static void removeBreakPoint(const BreakPoint& bp);
	typedef std::multimap<word, shared_ptr<BreakPoint> > BreakPoints;
	static const BreakPoints& getBreakPoints();

	void setWatchPoint(shared_ptr<WatchPoint> watchPoint);
	void removeWatchPoint(shared_ptr<WatchPoint> watchPoint);
	typedef std::vector<shared_ptr<WatchPoint> > WatchPoints;
	const WatchPoints& getWatchPoints() const;

	static void setCondition(shared_ptr<DebugCondition> cond);
	static void removeCondition(const DebugCondition& cond);
	typedef std::vector<shared_ptr<DebugCondition> > Conditions;
	static const Conditions& getConditions();

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
	static bool checkBreakPoints(unsigned pc)
	{
		std::pair<BreakPoints::const_iterator,
		          BreakPoints::const_iterator> range =
		                  breakPoints.equal_range(pc);
		if (conditions.empty() && (range.first == range.second)) {
			return false;
		}

		// slow path non-inlined
		checkBreakPoints(range);
		return isBreaked();
	}

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readMemSlow(word address, EmuTime::param time);
	void writeMemSlow(word address, byte value, EmuTime::param time);

	MSXDevice*& getDevicePtr(byte port, bool isIn);

	void register_IO  (int port, bool isIn,
	                   MSXDevice*& devicePtr, MSXDevice* device);
	void unregister_IO(MSXDevice*& devicePtr, MSXDevice* device);
	void registerSlot(MSXDevice& device,
	                  int ps, int ss, int base, int size);
	void unregisterSlot(MSXDevice& device,
	                    int ps, int ss, int base, int size);


	static void checkBreakPoints(std::pair<BreakPoints::const_iterator,
	                                       BreakPoints::const_iterator> range);

	void removeAllWatchPoints();
	void registerIOWatch  (WatchPoint& watchPoint, MSXDevice** devices);
	void unregisterIOWatch(WatchPoint& watchPoint, MSXDevice** devices);
	void updateMemWatch(WatchPoint::Type type);
	void executeMemWatch(WatchPoint::Type type, unsigned address,
	                     unsigned value = ~0u);

	void doContinue2();

	friend class SlotInfo;
	friend class IODebug;
	friend class IOInfo;
	const std::auto_ptr<MemoryDebug> memoryDebug;
	const std::auto_ptr<SlottedMemoryDebug> slottedMemoryDebug;
	const std::auto_ptr<IODebug> ioDebug;
	const std::auto_ptr<SlotInfo> slotInfo;
	const std::auto_ptr<SubSlottedInfo> subSlottedInfo;
	const std::auto_ptr<ExternalSlotInfo> externalSlotInfo;
	const std::auto_ptr<IOInfo> inputPortInfo;
	const std::auto_ptr<IOInfo> outputPortInfo;

	/** Updated visibleDevices for a given page and clears the cache
	  * on changes.
	  * Should be called whenever PrimarySlotState or SecondarySlotState
	  * was modified.
	  * @param page page [0..3] to update visibleDevices for.
	  */
	void updateVisible(int page);
	void setSubSlot(byte primSlot, byte value);

	std::auto_ptr<DummyDevice> dummyDevice;
	MSXCPU& msxcpu;
	CliComm& cliComm;
	MSXMotherBoard& motherBoard;

	std::auto_ptr<VDPIODelay> delayDevice;

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
	typedef std::vector<GlobalWriteInfo> GlobalWrites;
	GlobalWrites globalWrites;

	MSXDevice* IO_In [256];
	MSXDevice* IO_Out[256];
	MSXDevice* slotLayout[4][4][4];
	MSXDevice* visibleDevices[4];
	byte subSlotRegister[4];
	byte primarySlotState[4];
	byte secondarySlotState[4];
	unsigned expanded[4];

	//  All CPUs (Z80 and R800) of all MSX machines share this state.
	static BreakPoints breakPoints;
	WatchPoints watchPoints; // TODO must also be static
	static Conditions conditions;
	static bool breaked;
	static bool continued;
	static bool step;
};

} // namespace openmsx

#endif

// $Id$

#ifndef __MSXCPUINTERFACE_HH__
#define __MSXCPUINTERFACE_HH__

#include <set>
#include <vector>
#include <memory>
#include "openmsx.hh"
#include "CPU.hh"
#include "Command.hh"
#include "Debuggable.hh"
#include "MSXDevice.hh"

namespace openmsx {

class MSXDevice;
class VDPIODelay;
class DummyDevice;
class HardwareConfig;
class CommandController;
class MSXCPU;
class Scheduler;
class Debugger;
class CliCommOutput;

class MSXCPUInterface
{
public:
	MSXCPUInterface();
	static MSXCPUInterface& instance();
	
	/**
	 * Devices can register their In ports. This is normally done
	 * in their constructor. Once device are registered, their
	 * readIO() method can get called.
	 */
	virtual void register_IO_In(byte port, MSXDevice* device);
	virtual void unregister_IO_In(byte port, MSXDevice* device);

	/**
	 * Devices can register their Out ports. This is normally done
	 * in their constructor. Once device are registered, their
	 * writeIO() method can get called.
	 */
	virtual void register_IO_Out(byte port, MSXDevice* device);
	virtual void unregister_IO_Out(byte port, MSXDevice* device);

	/**
	 * Devices can register themself in the MSX slotstructure.
	 * This is normally done in their constructor. Once devices
	 * are registered their readMem() / writeMem() methods can
	 * get called.
	 */
	void registerMemDevice(MSXDevice& device,
	                       int primSl, int secSL, int pages);
	void unregisterMemDevice(MSXDevice& device,
	                         int primSl, int secSL, int pages);
	
	/**
	 * Reset (the slot state)
	 */
	void reset();

	/**
	 * This reads a byte from the currently selected device
	 */
	inline byte readMem(word address, const EmuTime& time) {
		if ((address != 0xFFFF) || !isSubSlotted[primarySlotState[3]]) {
			return visibleDevices[address >> 14]->readMem(address, time);
		} else {
			return 0xFF ^ subSlotRegister[primarySlotState[3]];
		}
	}

	/**
	 * This writes a byte to the currently selected device
	 */
	inline void writeMem(word address, byte value, const EmuTime& time) {
		if ((address != 0xFFFF) || !isSubSlotted[primarySlotState[3]]) {
			visibleDevices[address>>14]->writeMem(address, value, time);
		} else {
			setSubSlot(primarySlotState[3], value);
		}
	}

	/**
	 * This read a byte from the given IO-port
	 * @see MSXDevice::readIO()
	 */
	inline byte readIO(word prt, const EmuTime& time) {
		byte port = (byte)prt;
		return IO_In[port]->readIO(port, time);
	}

	/**
	 * This writes a byte to the given IO-port
	 * @see MSXDevice::writeIO()
	 */
	inline void writeIO(word prt, byte value, const EmuTime& time) {
		byte port = (byte)prt;
		IO_Out[port]->writeIO(port, value, time);
	}

	/**
	 * Test that the memory in the interval [start, start+CACHE_LINE_SIZE)
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
		if ((start == 0x10000 - CPU::CACHE_LINE_SIZE) && // contains 0xffff
		    (isSubSlotted[primarySlotState[3]])) {
			return NULL;
		} else {
			return visibleDevices[start >> 14]->getReadCacheLine(start);
		}
	}
	
	/**
	 * Test that the memory in the interval [start, start+CACHE_LINE_SIZE)
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
		if ((start == 0x10000 - CPU::CACHE_LINE_SIZE) && // contains 0xffff
		    (isSubSlotted[primarySlotState[3]])) {
			return NULL;
		} else {
			return visibleDevices[start >> 14]->getWriteCacheLine(start);
		}
	}

	/**
	 * CPU uses this method to read 'extra' data from the databus
	 * used in interrupt routines. In MSX this returns always 255.
	 */
	inline byte dataBus() {
		return 255;
	}

	/*
	 * Should only be used by PPI
	 *  TODO: make private / friend
	 */
	void setPrimarySlots(byte value);

	/**
	 * Peek memory location
	 * @see MSXDevice::peekMem()
	 */
	byte peekMem(word address) const;
	byte peekSlottedMem(unsigned address) const;
	void writeSlottedMem(unsigned address, byte value,
	                     const EmuTime& time);

	void setExpanded(int ps, bool expanded);

protected:
	virtual ~MSXCPUInterface();
	
	friend class std::auto_ptr<MSXCPUInterface>;

private:
	void registerSlot(MSXDevice* device,
			  int primSl, int secSL, int page);
	
	class MemoryDebug : public Debuggable {
	public:
		MemoryDebug(MSXCPUInterface& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		MSXCPUInterface& parent;
	} memoryDebug;

	class SlottedMemoryDebug : public Debuggable {
	public:
		SlottedMemoryDebug(MSXCPUInterface& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		MSXCPUInterface& parent;
	} slottedMemoryDebug;

	class IODebug : public Debuggable {
	public:
		IODebug(MSXCPUInterface& parent);
		virtual unsigned getSize() const;
		virtual const string& getDescription() const;
		virtual byte read(unsigned address);
		virtual void write(unsigned address, byte value);
	private:
		MSXCPUInterface& parent;
	} ioDebug;

	class SlotMapCmd : public SimpleCommand {
	public:
		SlotMapCmd(MSXCPUInterface& parent);
		virtual string execute(const std::vector<string>& tokens);
		virtual string help(const std::vector<string>& tokens) const;
	private:
		MSXCPUInterface& parent;
	} slotMapCmd;

	class SlotSelectCmd : public SimpleCommand {
	public:
		SlotSelectCmd(MSXCPUInterface& parent);
		virtual string execute(const std::vector<string>& tokens);
		virtual string help(const std::vector<string>& tokens) const;
	private:
		MSXCPUInterface& parent;
	} slotSelectCmd;

	class IOMapCmd : public SimpleCommand {
	public:
		IOMapCmd(MSXCPUInterface& parent);
		virtual string execute(const std::vector<string>& tokens);
		virtual string help(const std::vector<string>& tokens) const;
	private:
		MSXCPUInterface& parent;
	} ioMapCmd;


	/** Updated visibleDevices for a given page and clears the cache
	  * on changes.
	  * Should be called whenever PrimarySlotState or SecondarySlotState
	  * was modified.
	  * @param page page [0..3] to update visibleDevices for.
	  */
	void updateVisible(int page);
	void setSubSlot(byte primSlot, byte value);

	void printSlotMapPages(std::ostream&, const MSXDevice* const*) const;
	string getSlotMap() const;
	string getIOMap() const;
	string getSlotSelection() const;

	MSXDevice* IO_In [256];
	MSXDevice* IO_Out[256];
	std::set<byte> multiIn;
	std::set<byte> multiOut;

	MSXDevice* slotLayout[4][4][4];
	byte subSlotRegister[4];
	byte primarySlotState[4];
	byte secondarySlotState[4];
	bool isSubSlotted[4];
	MSXDevice* visibleDevices[4];

	DummyDevice& dummyDevice;
	HardwareConfig& hardwareConfig;
	CommandController& commandController;
	MSXCPU& msxcpu;
	Scheduler& scheduler;
	Debugger& debugger;
	CliCommOutput& cliCommOutput;
};

class TurborCPUInterface : public MSXCPUInterface
{
public:
	TurborCPUInterface();
	virtual ~TurborCPUInterface();

	virtual void register_IO_In(byte port, MSXDevice* device);
	virtual void unregister_IO_In(byte port, MSXDevice* device);
	virtual void register_IO_Out(byte port, MSXDevice* device);
	virtual void unregister_IO_Out(byte port, MSXDevice* device);

private:
	MSXDevice* getDelayDevice(MSXDevice& device);
	
	std::auto_ptr<VDPIODelay> delayDevice;
};

} // namespace openmsx

#endif //__MSXCPUINTERFACE_HH__

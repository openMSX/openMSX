// $Id$

#ifndef __MSXCPUINTERFACE_HH__
#define __MSXCPUINTERFACE_HH__

#include <set>
#include <memory>
#include "CPUInterface.hh"
#include "Command.hh"
#include "Debuggable.hh"

using std::set;
using std::auto_ptr;

namespace openmsx {

class MSXIODevice;
class MSXMemDevice;
class VDPIODelay;
class DummyDevice;
class HardwareConfig;
class CommandController;
class MSXCPU;
class Scheduler;
class Debugger;
class CliCommOutput;

class MSXCPUInterface : public CPUInterface
{
public:
	static MSXCPUInterface& instance();
	
	/**
	 * Devices can register their In ports. This is normally done
	 * in their constructor. Once device are registered, their
	 * readIO() method can get called.
	 * TODO: implement automatic registration for MSXIODevice
	 */
	virtual void register_IO_In(byte port, MSXIODevice* device);
	virtual void unregister_IO_In(byte port, MSXIODevice* device);

	/**
	 * Devices can register their Out ports. This is normally done
	 * in their constructor. Once device are registered, their
	 * writeIO() method can get called.
	 * TODO: implement automatic registration for MSXIODevice
	 */
	virtual void register_IO_Out(byte port, MSXIODevice* device);
	virtual void unregister_IO_Out(byte port, MSXIODevice* device);

	/**
	 * Devices can register themself in the MSX slotstructure.
	 * This is normally done in their constructor. Once devices
	 * are registered their readMem() / writeMem() methods can
	 * get called.
	 * Note: if a MSXDevice inherits from MSXMemDevice, it gets
	 *       automatically registered
	 */
	void registerMemDevice(MSXMemDevice& device,
	                       int primSl, int secSL, int pages);
	void unregisterMemDevice(MSXMemDevice& device,
	                         int primSl, int secSL, int pages);

	// TODO implement unregister methods
	
	/**
	 * Reset (the slot state)
	 */
	void reset();

	// CPUInterface //

	/**
	 * This reads a byte from the currently selected device
	 */
	virtual byte readMem(word address, const EmuTime& time);

	/**
	 * This writes a byte to the currently selected device
	 */
	virtual void writeMem(word address, byte value, const EmuTime& time);

	/**
	 * This read a byte from the given IO-port
	 * @see MSXMemDevice::readIO()
	 */
	virtual byte readIO(word port, const EmuTime& time);

	/**
	 * This writes a byte to the given IO-port
	 * @see MSXMemDevice::writeIO()
	 */
	virtual void writeIO(word port, byte value, const EmuTime& time);

	/**
	 * Gets read cache
	 * @see MSXMemDevice::getReadCacheLine()
	 */
	virtual const byte* getReadCacheLine(word start) const;
	
	/**
	 * Gets write cache
	 * @see MSXMemDevice::getWriteCacheLine()
	 */
	virtual byte* getWriteCacheLine(word start) const;

	/*
	 * Should only be used by PPI
	 *  TODO: make private / friend
	 */
	void setPrimarySlots(byte value);

	struct SlotSelection {
		byte primary [4];
		byte secondary [4];
		bool isSubSlotted [4];
	};
	SlotSelection* getCurrentSlots();
	
	/**
	 * Peek memory location
	 * @see MSXMemDevice::peekMem()
	 */
	byte peekMem(word address) const;
	byte peekSlottedMem(unsigned address) const;
	void writeSlottedMem(unsigned address, byte value,
	                     const EmuTime& time);

	void setExpanded(int ps, bool expanded);

protected:
	MSXCPUInterface();
	virtual ~MSXCPUInterface();

private:
	void registerSlot(MSXMemDevice* device,
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
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		MSXCPUInterface& parent;
	} slotMapCmd;

	class SlotSelectCmd : public SimpleCommand {
	public:
		SlotSelectCmd(MSXCPUInterface& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		MSXCPUInterface& parent;
	} slotSelectCmd;

	/** Updated visibleDevices for a given page and clears the cache
	  * on changes.
	  * Should be called whenever PrimarySlotState or SecondarySlotState
	  * was modified.
	  * @param page page [0..3] to update visibleDevices for.
	  */
	void updateVisible(int page);
	void setSubSlot(byte primSlot, byte value);

	void printSlotMapPages(ostream&, const MSXMemDevice* const*) const;
	string getSlotMap() const;
	string getSlotSelection() const;
	
	
	MSXIODevice* IO_In [256];
	MSXIODevice* IO_Out[256];
	set<byte> multiIn;
	set<byte> multiOut;

	MSXMemDevice* slotLayout[4][4][4];
	byte subSlotRegister[4];
	byte primarySlotState[4];
	byte secondarySlotState[4];
	bool isSubSlotted[4];
	MSXMemDevice* visibleDevices[4];

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

	virtual void register_IO_In(byte port, MSXIODevice* device);
	virtual void unregister_IO_In(byte port, MSXIODevice* device);
	virtual void register_IO_Out(byte port, MSXIODevice* device);
	virtual void unregister_IO_Out(byte port, MSXIODevice* device);

private:
	MSXIODevice* getDelayDevice(MSXIODevice& device);
	
	auto_ptr<VDPIODelay> delayDevice;
};

} // namespace openmsx

#endif //__MSXCPUINTERFACE_HH__

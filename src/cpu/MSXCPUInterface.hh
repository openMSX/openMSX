// $Id$

#ifndef __MSXCPUINTERFACE_HH__
#define __MSXCPUINTERFACE_HH__

#include "CPUInterface.hh"
#include "Command.hh"

// forward declarations
class MSXIODevice;
class MSXMemDevice;


class MSXCPUInterface : public CPUInterface
{
	public:
		/**
		 * Destructor
		 */
		~MSXCPUInterface();
		
		/**
		 * this is a singleton class
		 */
		static MSXCPUInterface* instance();
		
		/**
		 * Devices can register their In ports. This is normally done
		 * in their constructor. Once device are registered, their
		 * readIO() method can get called.
		 * TODO: implement automatic registration for MSXIODevice
		 */
		void register_IO_In(byte port, MSXIODevice *device);

		/**
		 * Devices can register their Out ports. This is normally done
		 * in their constructor. Once device are registered, their
		 * writeIO() method can get called.
		 * TODO: implement automatic registration for MSXIODevice
		 */
		void register_IO_Out(byte port, MSXIODevice *device);

		/**
		 * Devices can register themself in the MSX slotstructure.
		 * This is normally done in their constructor. Once devices
		 * are registered their readMem() / writeMem() methods can
		 * get called.
		 * Note: if a MSXDevice inherits from MSXMemDevice, it gets
		 *       automatically registered
		 */
		void registerSlottedDevice(MSXMemDevice *device,
		                           int PrimSl, int SecSL, int Page);

		/**
		 * Reset (the slot state)
		 */
		void reset();

		// CPUInterface //

		/**
		 * This reads a byte from the currently selected device
		 */
		byte readMem(word address, const EmuTime &time);

		/**
		 * This writes a byte to the currently selected device
		 */
		void writeMem(word address, byte value, const EmuTime &time);

		/**
		 * This read a byte from the given IO-port
		 */
		byte readIO(word port, const EmuTime &time);

		/**
		 * This writes a byte to the given IO-port
		 */
		void writeIO(word port, byte value, const EmuTime &time);

		virtual byte* getReadCacheLine(word start);
		virtual byte* getWriteCacheLine(word start);


		/*
		 * Should only be used by PPI
		 *  TODO: make private / friend
		 */
		void setPrimarySlots(byte value);

	private:
		MSXCPUInterface();
		
		class SlotMapCmd : public Command {
			virtual void execute(const std::vector<std::string> &tokens);
			virtual void help   (const std::vector<std::string> &tokens);
		};
		class SlotSelectCmd : public Command {
			virtual void execute(const std::vector<std::string> &tokens);
			virtual void help   (const std::vector<std::string> &tokens);
		};

		/** Updated visibleDevices for a given page and clears the cache
		  * on changes.
		  * Should be called whenever PrimarySlotState or SecondarySlotState
		  * was modified.
		  * @param page page [0..3] to update visibleDevices for.
		  */
		void updateVisible(int page);

		/** Used by getSlotMap to print the contents of a single slot.
		  */
		void printSlotMapPages(std::ostream &, MSXMemDevice *[]);
		
		/** Gets a string representation of the slot map of the emulated MSX.
		  * @return a multi-line string describing which slot holds which
		  *     device.
		  */
		std::string getSlotMap();

		/** Gets a string representation of the currently selected slots.
		  * @return a multi-line string describing which slot are currently
		  *     selected.
		  */
		std::string getSlotSelection();


		// Commands
		SlotMapCmd slotMapCmd;
		SlotSelectCmd slotSelectCmd;

		MSXIODevice* IO_In [256];
		MSXIODevice* IO_Out[256];

		MSXMemDevice* slotLayout[4][4][4];
		byte subSlotRegister[4];
		byte primarySlotState[4];
		byte secondarySlotState[4];
		bool isSubSlotted[4];
		MSXMemDevice* visibleDevices[4];
};
#endif //__MSXCPUINTERFACE_HH__

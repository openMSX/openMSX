// $Id$

#ifndef __MSXCPUINTERFACE_HH__
#define __MSXCPUINTERFACE_HH__

#include "CPUInterface.hh"
#include "MSXConfig.hh"
#include "MSXIODevice.hh"
#include "MSXMemDevice.hh"
#include "ConsoleSource/Command.hh"


class MSXCPUInterface : public CPUInterface
{
	public:
		/** Helper class for doing interrupt request (IRQ) administration.
		  * IRQ is either enabled or disabled; when enabled it contributes
		  * one to the motherboard IRQ count, when disabled zero.
		  * Calling set() in enabled state does nothing;
		  * neither does calling reset() in disabled state.
		  */
		class IRQHelper
		{
		public:
			/** Create a new IRQHelper.
			  * Initially there is no interrupt request on the bus.
			  */
			IRQHelper() {
				request = false;
			}

			/** Destroy this IRQHelper.
			  * Make sure interrupt is released.
			  */
			~IRQHelper() {
				reset();
			}

			/** Set the interrupt request on the bus.
			  */
			inline void set() {
				if (!request) {
					request = true;
					MSXCPUInterface::raiseIRQ();
				}
			}

			/** Reset the interrupt request on the bus.
			  */
			inline void reset() {
				if (request) {
					request = false;
					MSXCPUInterface::lowerIRQ();
				}
			}

			/** Get the interrupt state.
			  * @return true iff interrupt request is active.
			  */
			inline bool getState() {
				return request;
			}

		private:
			bool request;

		}; // end of IRQHelper

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

		/**
		 * This returns the current IRQ status.
		 * @return true iff IRQ pending.
		 */
		bool IRQStatus();

		/**
		 * This method raises an interrupt. A device may call this
		 * method more than once. If the device wants to lower the
		 * interrupt again it must call the lowerIRQ() method exactly as
		 * many times.
		 * Before using this method take a look at MSXMotherBoard::IRQHelper
		 */
		static void raiseIRQ();

		/**
		 * This methods lowers the interrupt again. A device may never
		 * call this method more often than it called the method
		 * raiseIRQ().
		 * Before using this method take a look at MSXMotherBoard::IRQHelper
		 */
		static void lowerIRQ();

		virtual byte* getReadCacheLine(word start);
		virtual byte* getWriteCacheLine(word start);


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

		/*
		 * Should only be used by PPI
		 *  TODO: make private / friend
		 */
		void setPrimarySlots(byte value);

	protected:
		MSXCPUInterface(MSXConfig::Config *config);

		void resetIRQLine();

	private:
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

		// Commands
		SlotMapCmd slotMapCmd;
		SlotSelectCmd slotSelectCmd;

		MSXIODevice* IO_In[256];
		MSXIODevice* IO_Out[256];

		MSXMemDevice* SlotLayout[4][4][4];
		byte SubSlot_Register[4];
		byte PrimarySlotState[4];
		byte SecondarySlotState[4];
		bool isSubSlotted[4];
		MSXMemDevice* visibleDevices[4];

		static int IRQLine;
};
#endif //__MSXCPUINTERFACE_HH__

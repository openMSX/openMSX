// $Id$

#ifndef __MSXMOTHERBOARD_HH__
#define __MSXMOTHERBOARD_HH__

#include <fstream>
#include <vector>
#include "CPUInterface.hh"
#include "MSXConfig.hh"
#include "ConsoleSource/ConsoleCommand.hh"

// forward declarations
class MSXDevice;
class MSXIODevice;
class MSXMemDevice;
class EmuTime;


class MSXMotherBoard : public CPUInterface
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
				motherboard = MSXMotherBoard::instance();
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
					motherboard->raiseIRQ();
				}
			}

			/** Reset the interrupt request on the bus.
			  */
			inline void reset() {
				if (request) {
					request = false;
					motherboard->lowerIRQ();
				}
			}

			/** Get the interrupt state.
			  * @return true iff interrupt request is active.
			  */
			inline bool getState() {
				return request;
			}

		private:
			MSXMotherBoard *motherboard;
			bool request;

		}; // end of IRQHelper

		/**
		 * Destructor
		 */
		virtual ~MSXMotherBoard();

		/**
		 * this class is a singleton class
		 * usage: MSXConfig::instance()->method(args);
		 */
		static MSXMotherBoard *instance();

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
		 * All MSXDevices should be registered by the MotherBoard.
		 * This method should only be called at start-up
		 */
		void addDevice(MSXDevice *device);
		
		/**
		 * To remove a device completely from configuration
		 * fe. yanking a cartridge out of the msx
		 *
		 * TODO this method is unimplemented!!
		 */
		void removeDevice(MSXDevice *device);

		/**
		 * This will initialize all MSXDevices (the init() method of
		 * all registered MSXDevices is called)
		 */
		void InitMSX();

		/**
		 * This starts the Scheduler.
		 */
		void startMSX();

		/**
		 * This will reset all MSXDevices (the reset() method of
		 * all registered MSXDevices is called)
		 */
		void resetMSX(const EmuTime &time);

		/**
		 * This will destroy all MSXDevices (the destructor of
		 * all registered MSXDevices is called)
		 */
		void destroyMSX();


		/**
		 * TODO
		 */
		void restoreMSX();	// TODO unimplemented!!
		/**
		 * TODO
		 */
		void saveStateMSX(std::ofstream &savestream);


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
		void raiseIRQ();

		/**
		 * This methods lowers the interrupt again. A device may never
		 * call this method more often than it called the method
		 * raiseIRQ().
		 * Before using this method take a look at MSXMotherBoard::IRQHelper
		 */
		void lowerIRQ();

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

	private:
		class ResetCmd : public ConsoleCommand {
			virtual void execute(char *commandLine);
			virtual void help(char *commandLine);
		};
		class SlotMapCmd : public ConsoleCommand {
			virtual void execute(char *commandLine);
			virtual void help(char *commandLine);
		};
		class SlotSelectCmd : public ConsoleCommand {
			virtual void execute(char *commandLine);
			virtual void help(char *commandLine);
		};

		MSXMotherBoard();

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

		// ConsoleCommands
		ResetCmd resetCmd;
		SlotMapCmd slotMapCmd;
		SlotSelectCmd slotSelectCmd;

		MSXIODevice* IO_In[256];
		MSXIODevice* IO_Out[256];
		std::vector<MSXDevice*> availableDevices;

		MSXMemDevice* SlotLayout[4][4][4];
		byte SubSlot_Register[4];
		byte PrimarySlotState[4];
		byte SecondarySlotState[4];
		bool isSubSlotted[4];
		MSXMemDevice* visibleDevices[4];

		int IRQLine;

		static MSXMotherBoard *oneInstance;

		MSXConfig::Config *config;
};
#endif //__MSXMOTHERBOARD_HH__

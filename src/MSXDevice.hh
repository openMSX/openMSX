// $Id$

#ifndef __MSXDEVICE_HH__
#define __MSXDEVICE_HH__

#include <iostream>
#include <fstream>
#include <string>

#include "msxconfig.hh"
#include "EmuTime.hh"
#include "openmsx.hh"
#include "Scheduler.hh"

class MSXDevice : public Schedulable
{
	public:
		/**
		 * Destructor
		 */
		virtual ~MSXDevice();

		//
		// interaction with CPU
		//
		/**
		 * Read a byte from a location at a certain time from this
		 * device.
		 * The deafult implementation returns 255.
		 */
		virtual byte readMem(word address, EmuTime &time);

		/**
		 * Write a given byte to a given location at a certain time 
		 * to this device.
		 * The default implementation ignores the write (does nothing).
		 */
		virtual void writeMem(word address, byte value, EmuTime &time);

		/**
		 * Read a byte from an IO port at a certain time from this device.
		 * The default implementation returns 255.
		 */
		virtual byte readIO(byte port, EmuTime &time);

		/**
		 * Write a byte to a given IO port at a certain time to this
		 * device.
		 * The default implementation ignores the write (does nothing)
		 */
		virtual void writeIO(byte port, byte value, EmuTime &time);

		/**
		 * Emulates this device until a given time.
		 * The Default implementation does nothing.
		 */
		virtual void executeUntilEmuTime(const EmuTime &time);
		
		/**
		 * This method is called when all devices are instantiated
		 * Default implementation does nothing.
		 */
		virtual void init();

		/**
		 * This method is called on power-up.
		 * Default implementation calls reset().
		 */
		virtual void start();

		/**
		 * This method is called on power-down.
		 * Default implementation does nothing.
		 */
		virtual void stop();

		/**
		 * This method is called on reset.
		 * Default implementation resets internal IRQ flag (every
		 * re-implementation should also call this method).
		 */
		virtual void reset();
		
		
		/**
		 * Save all state-information for this device
		 * Default implementation does nothing (nothing needs to be saved).
		 *
		 * Note: save mechanisme not implemented yet
		 */
		virtual void saveState(std::ofstream &writestream);

		/**
		 * Restore all state-information for this device
		 * Default implementation does nothing (nothing needs to be restored).
		 * 
		 * Note: save mechanisme not implemented yet
		 */
		virtual void restoreState(std::string &devicestring, std::ifstream &readstream);
		
		
		/**
		 * Returns a human-readable name for this device. The name is set
		 * in the setConfigDevice() method. This method is mostly used to 
		 * print debug info.
		 * Default implementation is normally ok.
		 */
		virtual const std::string &getName();

	protected:
		/**
		 * Every MSXDevice has an entry in the config file, this
		 * constructor fills in some config-related variables
		 * All subclasses must call this super-constructor.
		 * The devices are instantiated in a random order, so you
		 * can't relay on other devices already being instantiated, 
		 * if you need this you have to use the init() method.
		 */
		MSXDevice(MSXConfig::Device *config);
		
		/*
		 * Only execption is the DummyDevice, this has no config-entry
		 */
		MSXDevice();
		
		//These are used for save/restoreState see note over
		//savefile-structure
		//bool writeSaveStateHeader(std::ofstream &writestream);
		//bool checkSaveStateHeader(std::string &devicestring);
		//char* deviceVersion;
		
		/**
		 * To ease the burden of keeping IRQ state.
		 * This function raises an interrupt if this device does not
		 * already have an active interrupt line.
		 * Note: this is only a helper function
		 */
		void setInterrupt();
		/**
		 * To ease the burden of keeping IRQ state.
		 * This function lowers this device its interrupt line if it
		 * was active.
		 * Note: this is only a helper function
		 */
		void resetInterrupt();
		/**
		 * The current state of this device its interrupt line
		 *  true -> active   false -> not active
		 */
		bool isIRQset;

		/**
		 * Register this device in all the slots that where specified
		 * in its config file
		 * Note: this is only a helper function, you do not have to use
		 *       this to register the device
		 */
		void registerSlots();

		MSXConfig::Device *deviceConfig;
		const std::string* deviceName;
};

#endif //__MSXDEVICE_HH__


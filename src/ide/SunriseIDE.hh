// $Id$

#ifndef __SunriseIDE_HH__
#define __SunriseIDE_HH__

#include "MSXMemDevice.hh"
#include "MSXRomDevice.hh"

class IDEDevice;


class SunriseIDE : public MSXMemDevice, MSXRomDevice
{
	public:
		/**
		 * Constructor
		 */
		SunriseIDE(MSXConfig::Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~SunriseIDE();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual byte* getReadCacheLine(word start);

	private:
		void writeControl(byte value);
		byte reverse(byte a);
		
		byte SunriseIDE::readDataLow(const EmuTime &time);
		byte SunriseIDE::readDataHigh(const EmuTime &time);
		word SunriseIDE::readData(const EmuTime &time);
		byte SunriseIDE::readReg(nibble reg, const EmuTime &time);
		void SunriseIDE::writeDataLow(byte value, const EmuTime &time);
		void SunriseIDE::writeDataHigh(byte value, const EmuTime &time);
		void SunriseIDE::writeData(word value, const EmuTime &time);
		void writeReg(nibble reg, byte value, const EmuTime &time);

		byte* internalBank;
		bool ideRegsEnabled;
		byte readLatch;
		byte writeLatch;
		byte selectedDevice;
		IDEDevice* device[2];
};
#endif

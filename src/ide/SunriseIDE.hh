// $Id$

#ifndef __SunriseIDE_HH__
#define __SunriseIDE_HH__

#include "MSXMemDevice.hh"
#include "MSXRomDevice.hh"

class IDEDevice;


class SunriseIDE : public MSXMemDevice
{
	public:
		SunriseIDE(MSXConfig::Device *config, const EmuTime &time);
		virtual ~SunriseIDE();
		
		virtual void reset(const EmuTime &time);
		
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);  
		virtual byte* getReadCacheLine(word start);

	private:
		void writeControl(byte value);
		byte reverse(byte a);
		
		byte readDataLow(const EmuTime &time);
		byte readDataHigh(const EmuTime &time);
		word readData(const EmuTime &time);
		byte readReg(nibble reg, const EmuTime &time);
		void writeDataLow(byte value, const EmuTime &time);
		void writeDataHigh(byte value, const EmuTime &time);
		void writeData(word value, const EmuTime &time);
		void writeReg(nibble reg, byte value, const EmuTime &time);

		MSXRomDevice rom;
		byte* internalBank;
		bool ideRegsEnabled;
		bool softReset;
		byte readLatch;
		byte writeLatch;
		byte selectedDevice;
		IDEDevice* device[2];
};
#endif

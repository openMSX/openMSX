// $Id$

#ifndef __SunriseIDE_HH__
#define __SunriseIDE_HH__

#include <memory>
#include "MSXMemDevice.hh"
#include "Rom.hh"

using std::auto_ptr;

namespace openmsx {

class IDEDevice;

class SunriseIDE : public MSXMemDevice
{
public:
	SunriseIDE(const XMLElement& config, const EmuTime& time);
	virtual ~SunriseIDE();
	
	virtual void reset(const EmuTime& time);
	
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);  
	virtual const byte* getReadCacheLine(word start) const;

private:
	void writeControl(byte value);
	
	byte readDataLow(const EmuTime& time);
	byte readDataHigh(const EmuTime& time);
	word readData(const EmuTime& time);
	byte readReg(nibble reg, const EmuTime& time);
	void writeDataLow(byte value, const EmuTime& time);
	void writeDataHigh(byte value, const EmuTime& time);
	void writeData(word value, const EmuTime& time);
	void writeReg(nibble reg, byte value, const EmuTime& time);

	Rom rom;
	const byte* internalBank;
	bool ideRegsEnabled;
	bool softReset;
	byte readLatch;
	byte writeLatch;
	byte selectedDevice;
	auto_ptr<IDEDevice> device[2];
};

} // namespace openmsx
#endif

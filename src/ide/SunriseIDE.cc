// $Id$

#include "SunriseIDE.hh"
#include "IDEDevice.hh"
#include "DummyIDEDevice.hh"


SunriseIDE::SunriseIDE(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time),
	  MSXRomDevice(config, time)
{
	device[0] = new DummyIDEDevice();
	device[1] = new DummyIDEDevice();

	writeControl(0xFF);
	reset(time);
}

SunriseIDE::~SunriseIDE()
{
	delete device[0];
	delete device[1];
}


void SunriseIDE::reset(const EmuTime &time)
{
	selectedDevice = 0;
	device[0]->reset(time);
	device[1]->reset(time);
}


byte SunriseIDE::readMem(word address, const EmuTime &time)
{
	// TODO is it possible to read the control register
	
	if (ideRegsEnabled && ((address & 0x3E00) == 0x3C00)) {
		// 0x7C00 - 0x7DFF   ide data register
		if ((address & 1) == 0) {
			readDataLow(time);
		} else {
			readDataHigh(time);
		}
	}
	if (ideRegsEnabled && ((address & 0x3F00) == 0x3E00)) {
		// 0x7E00 - 0x7EFF   ide registers
		readReg(address & 0xF, time);
	}
	if ((0x4000 <= address) && (address < 0x8000)) {
		// read normal (flash) rom
		return internalBank[address & 0x3FFF];
	}
	// read nothing
	return 0xFF;
}

byte* SunriseIDE::getReadCacheLine(word start)
{
	return NULL;	// TODO
}

void SunriseIDE::writeMem(word address, byte value, const EmuTime &time)
{
	if ((address & 0xBF04) == 0x0104) {
		// control register
		writeControl(value);
	}
	if (ideRegsEnabled && ((address & 0x3E00) == 0x3C00)) {
		// 0x7C00 - 0x7DFF   ide data register
		if ((address & 1) == 0) {
			writeDataLow(value, time);
		} else {
			writeDataHigh(value, time);
		}
	}
	if (ideRegsEnabled && ((address & 0x3F00) == 0x3E00)) {
		// 0x7E00 - 0x7EFF   ide registers
		writeReg(address & 0xF, value, time);
	}
	// all other writes ignored
}


void SunriseIDE::writeControl(byte value)
{
	ideRegsEnabled = value & 1; // TODO cache

	byte bank = reverse(value & 0xF8);
	if (bank >= (romSize / 0x4000)) {
		bank &= ((romSize / 0x4000) - 1);
	}
	internalBank = &romBank[0x4000 * bank];
}

byte SunriseIDE::reverse(byte a)
{
	a = ((a & 0xF0) >> 4) | ((a & 0x0F) << 4);
	a = ((a & 0xCC) >> 2) | ((a & 0x33) << 2);
	a = ((a & 0xAA) >> 1) | ((a & 0x55) << 1);
	return a;
}


byte SunriseIDE::readDataLow(const EmuTime &time)
{
	word temp = readData(time);
	readLatch = temp >> 8;
	return temp & 0xFF;
}
byte SunriseIDE::readDataHigh(const EmuTime &time)
{
	return readLatch;
}
word SunriseIDE::readData(const EmuTime &time)
{
	return device[selectedDevice]->readData(time);
}

byte SunriseIDE::readReg(nibble reg, const EmuTime &time)
{
	if (reg == 0) {
		return readData(time) & 0xFF;
	} else {
		byte temp = device[selectedDevice]->readReg(reg, time);
		if (reg == 6) {
			temp &= 0xEF;
			temp |= selectedDevice ? 0x10 : 0x00;
		}
		return temp;
	}
}


void SunriseIDE::writeDataLow(byte value, const EmuTime &time)
{
	writeLatch = value;
}
void SunriseIDE::writeDataHigh(byte value, const EmuTime &time)
{
	word temp = (value << 8) | writeLatch;
	writeData(temp, time);
}
void SunriseIDE::writeData(word value, const EmuTime &time)
{
	device[selectedDevice]->writeData(value, time);
}

void SunriseIDE::writeReg(nibble reg, byte value, const EmuTime &time)
{
	if (reg == 0) {
		writeData((value << 8) | value, time);
	} else {
		if (reg == 6) {
			selectedDevice = (value & 0x10) ? 1 : 0;
			value &= 0xEF;
		}
		device[selectedDevice]->writeReg(reg, value, time);
	}
}

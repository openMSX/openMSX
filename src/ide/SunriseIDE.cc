// $Id$

#include "SunriseIDE.hh"
#include "IDEDevice.hh"
#include "DummyIDEDevice.hh"
#include "IDEHD.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "IDEDeviceFactory.hh"
#include "xmlx.hh"

namespace openmsx {

SunriseIDE::SunriseIDE(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time), MSXMemDevice(config, time),
	  rom(getName() + " ROM", "rom", config)
{
	const XMLElement* masterElem = config.getChild("master");
	const XMLElement* slaveElem  = config.getChild("slave");
	device[0].reset(masterElem 
	          ? IDEDeviceFactory::create(masterElem->getData(), time)
	          : new DummyIDEDevice());
	device[1].reset(slaveElem
	          ? IDEDeviceFactory::create(slaveElem->getData(), time)
	          : new DummyIDEDevice());

	// make valgrind happy
	internalBank = 0;
	ideRegsEnabled = false;
	
	writeControl(0xFF);
	reset(time);
}

SunriseIDE::~SunriseIDE()
{
}


void SunriseIDE::reset(const EmuTime& time)
{
	selectedDevice = 0;
	softReset = false;
	device[0]->reset(time);
	device[1]->reset(time);
}


byte SunriseIDE::readMem(word address, const EmuTime& time)
{
	if (ideRegsEnabled && ((address & 0x3E00) == 0x3C00)) {
		// 0x7C00 - 0x7DFF   ide data register
		if ((address & 1) == 0) {
			return readDataLow(time);
		} else {
			return readDataHigh(time);
		}
	}
	if (ideRegsEnabled && ((address & 0x3F00) == 0x3E00)) {
		// 0x7E00 - 0x7EFF   ide registers
		return readReg(address & 0xF, time);
	}
	if ((0x4000 <= address) && (address < 0x8000)) {
		// read normal (flash) rom
		return internalBank[address & 0x3FFF];
	}
	// read nothing
	return 0xFF;
}

const byte* SunriseIDE::getReadCacheLine(word start) const
{
	if (ideRegsEnabled && ((start & 0x3E00) == 0x3C00)) {
		return NULL;
	}
	if (ideRegsEnabled && ((start & 0x3F00) == 0x3E00)) {
		return NULL;
	}
	if ((0x4000 <= start) && (start < 0x8000)) {
		return &internalBank[start & 0x3FFF];
	}
	return unmappedRead;
}

void SunriseIDE::writeMem(word address, byte value, const EmuTime& time)
{
	if ((address & 0xBF04) == 0x0104) {
		// control register
		writeControl(value);
		return;
	}
	if (ideRegsEnabled && ((address & 0x3E00) == 0x3C00)) {
		// 0x7C00 - 0x7DFF   ide data register
		if ((address & 1) == 0) {
			writeDataLow(value, time);
		} else {
			writeDataHigh(value, time);
		}
		return;
	}
	if (ideRegsEnabled && ((address & 0x3F00) == 0x3E00)) {
		// 0x7E00 - 0x7EFF   ide registers
		writeReg(address & 0xF, value, time);
		return;
	}
	// all other writes ignored
}


static byte reverse(byte a)
{
	a = ((a & 0xF0) >> 4) | ((a & 0x0F) << 4);
	a = ((a & 0xCC) >> 2) | ((a & 0x33) << 2);
	a = ((a & 0xAA) >> 1) | ((a & 0x55) << 1);
	return a;
}

void SunriseIDE::writeControl(byte value)
{
	PRT_DEBUG("IDE write control: " << (int)value);
	
	if (ideRegsEnabled != (value & 1)) {
		ideRegsEnabled = value & 1;
		MSXCPU::instance().invalidateCache(0x3C00, 0x0300/CPU::CACHE_LINE_SIZE);
		MSXCPU::instance().invalidateCache(0x7C00, 0x0300/CPU::CACHE_LINE_SIZE);
		MSXCPU::instance().invalidateCache(0xBC00, 0x0300/CPU::CACHE_LINE_SIZE);
		MSXCPU::instance().invalidateCache(0xFC00, 0x0300/CPU::CACHE_LINE_SIZE);
	}

	byte bank = reverse(value & 0xF8);
	if (bank >= (rom.getSize() / 0x4000)) {
		bank &= ((rom.getSize() / 0x4000) - 1);
	}
	if (internalBank != &rom[0x4000 * bank]) {
		internalBank = &rom[0x4000 * bank];
		MSXCPU::instance().invalidateCache(0x4000, 0x4000/CPU::CACHE_LINE_SIZE);
	}
}


byte SunriseIDE::readDataLow(const EmuTime& time)
{
	word temp = readData(time);
	readLatch = temp >> 8;
	return temp & 0xFF;
}
byte SunriseIDE::readDataHigh(const EmuTime& time)
{
	return readLatch;
}
word SunriseIDE::readData(const EmuTime& time)
{
	word result = device[selectedDevice]->readData(time);
	PRT_DEBUG("IDE read data: 0x" << hex << int(result) << dec);
	return result;
}

byte SunriseIDE::readReg(nibble reg, const EmuTime& time)
{
	PRT_DEBUG("IDE read reg: " << (int)reg);
	byte result;
	if (reg == 14) {
		// alternate status register
		reg = 7;
	}
	if (softReset) {
		if (reg == 7) {
			// read status
			result = 0xFF;	// BUSY
		} else {
			// all others 
			result = 0x7F;	// don't care
		}
	} else {
		if (reg == 0) {
			result = readData(time) & 0xFF;
		} else {
			result = device[selectedDevice]->readReg(reg, time);
			if (reg == 6) {
				result &= 0xEF;
				result |= selectedDevice ? 0x10 : 0x00;
			}
		}
	}
	PRT_DEBUG("IDE read reg: " << (int)reg << " 0x" << hex << (int)result << dec);
	return result;
}


void SunriseIDE::writeDataLow(byte value, const EmuTime& time)
{
	writeLatch = value;
}
void SunriseIDE::writeDataHigh(byte value, const EmuTime& time)
{
	word temp = (value << 8) | writeLatch;
	writeData(temp, time);
}
void SunriseIDE::writeData(word value, const EmuTime& time)
{
	PRT_DEBUG("IDE write data: 0x" << hex << int(value) << dec);
	device[selectedDevice]->writeData(value, time);
}

void SunriseIDE::writeReg(nibble reg, byte value, const EmuTime& time)
{
	PRT_DEBUG("IDE write reg: " << (int)reg << " 0x" << hex << (int)value << dec);
	if (softReset) {
		if ((reg == 14) && !(value & 0x04)) {
			// clear SRST
			softReset = false;
		}
		// ignore all other writes
	} else {
		if (reg == 0) {
			writeData((value << 8) | value, time);
		} else {
			if ((reg == 14) && (value & 0x04)) {
				// set SRST
				softReset = true;
				device[0]->reset(time);
				device[1]->reset(time);
			} else {
				if (reg == 6) {
					selectedDevice = (value & 0x10) ? 1 : 0;
				}
				device[selectedDevice]->writeReg(reg, value, time);
			}
		}
	}
}

} // namespace openmsx

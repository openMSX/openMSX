// $Id$

#include "MSXS1990.hh"
#include "MSXCPUInterface.hh"
#include "MSXCPU.hh"


MSXS1990::MSXS1990(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXIODevice(config, time)
{
	MSXCPUInterface::instance()->register_IO_In (0xE4,this);
	MSXCPUInterface::instance()->register_IO_In (0xE5,this);
	MSXCPUInterface::instance()->register_IO_Out(0xE4,this);
	MSXCPUInterface::instance()->register_IO_Out(0xE5,this);
	reset(time);
	//frontSwitch = 0;	// doesn't change on reset
	frontSwitch = 64;	// doesn't change on reset
}

MSXS1990::~MSXS1990()
{
}
 
void MSXS1990::reset(const EmuTime &time)
{
	registerSelect = 0;	// TODO check this
	setCPUStatus(96);
}

byte MSXS1990::readIO(byte port, const EmuTime &time)
{
	switch (port) {
		case 0xE4:
			return registerSelect;
		case 0xE5:
			PRT_DEBUG("S1990: read reg "<<(int)registerSelect);
			switch (registerSelect) {
				case 5:
					return frontSwitch;
				case 6:
					return cpuStatus;
				case 13:
					return 3;	//TODO 
				case 14:
					return 47;	//TODO
				case 15:
					return 139;	//TODO
				default:
					return 255;
			}
		default:
			assert(false);
			return 255;	// avoid warning
	}
}

void MSXS1990::writeIO(byte port, byte value, const EmuTime &time)
{
	switch (port) {
		case 0xE4:
			registerSelect = value;
			break;
		case 0xE5:
			switch (registerSelect) {
				case 6:
					setCPUStatus(value);
					break;
			}
	}
}

void MSXS1990::setCPUStatus(byte value)
{
	cpuStatus = value & 0x60;
	MSXCPU::instance()->setActiveCPU((cpuStatus & 0x20) ? MSXCPU::CPU_Z80 : MSXCPU::CPU_R800);
	// TODO bit 6 -> (0) DRAM  (1) no DRAM
	// TODO bit 7 -> reset MSX ?????
}


// TODO implement "frontSwitch" command

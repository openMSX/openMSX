// $Id$

#include "MSXSCCPlusCart.hh"

#include "SCC.hh"
#include "File.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "MSXConfig.hh"


MSXSCCPlusCart::MSXSCCPlusCart(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	short volume = (short)config->getParameterAsInt("volume");
	cartridgeSCC = new SCC(volume);

	// allocate buffer
	memoryBank = new byte[131072];
	memset(memoryBank, 255, 131072);

	if (deviceConfig->hasParameter("filename")) {
		// read the rom file
		std::string filename = deviceConfig->getParameter("filename");
		try {
			File file(config->getContext(), filename);
			int romSize = file.size();
			file.read(memoryBank, romSize);
		} catch (FileException &e) {
			PRT_ERROR("Error reading " << filename);
		}
	}
	reset(time);
}

MSXSCCPlusCart::~MSXSCCPlusCart()
{
	delete[] memoryBank;
	delete cartridgeSCC;
}

void MSXSCCPlusCart::reset(const EmuTime &time)
{
	setMapper(0, 0);
	setMapper(1, 1);
	setMapper(2, 2);
	setMapper(3, 3);
	setModeRegister(0);
}


byte MSXSCCPlusCart::readMem(word address, const EmuTime &time)
{
	//PRT_DEBUG("SCC+ read "<< std::hex << address << std::dec);
	
	// modeRegister can not be read!
	if (((enable == EN_SCC)     && (0x9800 <= address) && (address < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= address) && (address < 0xC000))) {
		// SCC  visible in 0x9800 - 0x9FFF
		// SCC+ visible in 0xB800 - 0xBFFF
		return cartridgeSCC->readMemInterface(address & 0xFF, time);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// SCC(+) enabled/disabled but not requested so memory stuff
		return internalMemoryBank[(address >> 13) - 2][address & 0x1FFF];
	} else {
		// outside memory range
		return 0xFF;
	}
}

const byte* MSXSCCPlusCart::getReadCacheLine(word start) const
{
	if (((enable == EN_SCC)     && (0x9800 <= start) && (start < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= start) && (start < 0xC000))) {
		// SCC  visible in 0x9800 - 0x9FFF
		// SCC+ visible in 0xB800 - 0xBFFF
		return NULL;
	} else if ((0x4000 <= start) && (start < 0xC000)) {
		// SCC(+) enabled/disabled but not requested so memory stuff
		return &internalMemoryBank[(start >> 13) - 2][start & 0x1FFF];
	} else {
		// outside memory range
		return NULL;
	}
}


void MSXSCCPlusCart::writeMem(word address, byte value, const EmuTime &time)
{
	//PRT_DEBUG("SCC+ write "<< std::hex << address << " " << (int)value << std::dec);
	
	if ((address < 0x4000) || (0xc000 <= address)) {
		// outside memory range
		return;
	}
	
	// Mode register is mapped upon 0xBFFE and 0xBFFF
	if ((address | 0x0001) == 0xBFFF) {
		setModeRegister(value);
		return;
	}
	
	// Write to RAM
	int regio = (address >> 13) - 2;
	if (isRamSegment[regio]) {
		//According to Sean Young
		// when the regio's are in RAM mode you can read from 
		// the SCC(+) but not write to them 
		// => we assume a write to the memory but maybe
		//    they are just discarded
		// TODO check this out => ask Sean...
		internalMemoryBank[regio][address & 0x1FFF] = value;
		return;
	}

	/* Write to bankswitching registers
	 * The address to change banks:
	 *   bank 1: 0x5000 - 0x57FF (0x5000 used)
	 *   bank 2: 0x7000 - 0x77FF (0x7000 used)
	 *   bank 3: 0x9000 - 0x97FF (0x9000 used)
	 *   bank 4: 0xB000 - 0xB7FF (0xB000 used)
	 */
	if ((address & 0x1800) == 0x1000) {
		setMapper(regio, value);
		return;
	}
	
	// call writeMemInterface of SCC if needed
	switch (enable) {
	case EN_NONE:
		// do nothing
		break;
	case EN_SCC:
		if ((0x9800 <= address) && (address < 0xA000)) {
			cartridgeSCC->writeMemInterface(address & 0xFF, value, time);
		}
		break;
	case EN_SCCPLUS:
		if ((0xB800 <= address) && (address < 0xC000)) {
			cartridgeSCC->writeMemInterface(address & 0xFF, value, time);
		}
		break;
	}
}

byte* MSXSCCPlusCart::getWriteCacheLine(word start) const
{
	if ((0x4000 <= start) && (start < 0xC000)) {
		if (start == (0xBFFF & CPU::CACHE_LINE_HIGH)) {
			return NULL;
		}
		int regio = (start >> 13) - 2;
		if (isRamSegment[regio]) {
			return &internalMemoryBank[regio][start & 0x1FFF];
		}
	}
	return NULL;
}


void MSXSCCPlusCart::setMapper(int regio, byte value)
{
	mapper[regio] = value;
	checkEnable();
	internalMemoryBank[regio] = memoryBank + (0x2000 * (value & 0x0F));
	MSXCPU::instance()->invalidateCache(0x4000 + regio*0x2000, 0x2000/CPU::CACHE_LINE_SIZE);
}

void MSXSCCPlusCart::setModeRegister(byte value)
{
	modeRegister = value;
	checkEnable();
	
	if (modeRegister & 0x20) {
		cartridgeSCC->setChipMode(SCC::SCC_plusmode);
	} else {
		cartridgeSCC->setChipMode(SCC::SCC_Compatible);
	}
	
	if (modeRegister & 0x10) {
		isRamSegment[0] = true;
		isRamSegment[1] = true;
		isRamSegment[2] = true;
		isRamSegment[3] = true;
	} else {
		if (modeRegister & 0x01) {
			isRamSegment[0] = true;
		} else {
			if (isRamSegment[0]) {
				MSXCPU::instance()->invalidateCache(0x4000, 0x2000/CPU::CACHE_LINE_SIZE);
				isRamSegment[0] = false;
			}
		}
		if (modeRegister & 0x02) {
			isRamSegment[1] = true;
		} else {
			if (isRamSegment[1]) {
				MSXCPU::instance()->invalidateCache(0x6000, 0x2000/CPU::CACHE_LINE_SIZE);
				isRamSegment[1] = false;
			}
		}
		if ((modeRegister & 0x24) == 0x24) {
			// extra requirement for this bank: SCC+ mode
			isRamSegment[2] = true;
		} else {
			if (isRamSegment[2]) {
				MSXCPU::instance()->invalidateCache(0x8000, 0x2000/CPU::CACHE_LINE_SIZE);
				isRamSegment[2] = false;
			}
		}
		if (isRamSegment[3]) {
			MSXCPU::instance()->invalidateCache(0xA000, 0x2000/CPU::CACHE_LINE_SIZE);
			isRamSegment[3] = false;
		}
	}
}

void MSXSCCPlusCart::checkEnable()
{
	if ((modeRegister & 0x20) && (mapper[3] & 0x80)) {
		enable = EN_SCCPLUS;
	} else if ((!(modeRegister & 0x20)) && ((mapper[2] & 0x3F) == 0x3F)) {
		enable = EN_SCC;
	} else {
		enable = EN_NONE;
	}
}

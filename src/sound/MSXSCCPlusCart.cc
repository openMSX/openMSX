// $Id$

// Note: this device is actually called SCC-II. But this would take a lot of
// renaming, which isn't worth it right now. TODO rename this :)

#include "MSXSCCPlusCart.hh"
#include "SCC.hh"
#include "Ram.hh"
#include "File.hh"
#include "FileContext.hh"
#include "FileException.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "XMLElement.hh"

using std::string;

namespace openmsx {

MSXSCCPlusCart::MSXSCCPlusCart(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
	, ram(new Ram(getName() + " RAM", "SCC+ RAM", 0x20000))
{
	const XMLElement* fileElem = config.findChild("filename");
	if (fileElem) {
		// read the rom file
		const string& filename = fileElem->getData();
		try {
			File file(config.getFileContext().resolve(filename));
			int romSize = file.getSize();
			file.read(&(*ram)[0], romSize);
		} catch (FileException& e) {
			throw FatalError("Error reading file: " + filename);
		}
	}
	const string subtype = config.getChildData("subtype", "expanded");
	if (subtype == "Snatcher") {
		mapperMask = 0x0F;
		lowRAM  = true;
		highRAM = false;
	} else if (subtype == "SD-Snatcher") {
		mapperMask = 0x0F;
		lowRAM  = false;
		highRAM = true;
	} else if (subtype == "mirrored") {
		mapperMask = 0x07;
		lowRAM = highRAM = true;
	} else {
		// subtype "expanded", and all others
		mapperMask = 0x0F;
		lowRAM = highRAM = true;
	}

	// make valgrind happy
	for (int i = 0; i < 4; ++i) {
		isRamSegment[i] = true;
		mapper[i] = 0;
	}

	scc.reset(new SCC(getName(), config, time, SCC::SCC_Compatible));

	reset(time);
}

MSXSCCPlusCart::~MSXSCCPlusCart()
{
}

void MSXSCCPlusCart::reset(const EmuTime& time)
{
	setModeRegister(0);
	setMapper(0, 0);
	setMapper(1, 1);
	setMapper(2, 2);
	setMapper(3, 3);
	scc->reset(time);
}


byte MSXSCCPlusCart::readMem(word address, const EmuTime& time)
{
	byte result;
	// modeRegister can not be read!
	if (((enable == EN_SCC)     && (0x9800 <= address) && (address < 0xA000)) ||
	    ((enable == EN_SCCPLUS) && (0xB800 <= address) && (address < 0xC000))) {
		// SCC  visible in 0x9800 - 0x9FFF
		// SCC+ visible in 0xB800 - 0xBFFF
		result = scc->readMemInterface(address & 0xFF, time);
	} else if ((0x4000 <= address) && (address < 0xC000)) {
		// SCC(+) enabled/disabled but not requested so memory stuff
		result = internalMemoryBank[(address >> 13) - 2][address & 0x1FFF];
	} else {
		// outside memory range
		result = 0xFF;
	}
	//PRT_DEBUG("SCC+ read "<< hex << (int)address << " " << (int)result << dec);
	return result;
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
		return unmappedRead;
	}
}


void MSXSCCPlusCart::writeMem(word address, byte value, const EmuTime& time)
{
	//PRT_DEBUG("SCC+ write "<< hex << address << " " << (int)value << dec);
	
	if ((address < 0x4000) || (0xC000 <= address)) {
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
		if (isMapped[regio]) {
			internalMemoryBank[regio][address & 0x1FFF] = value;
		}
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
			scc->writeMemInterface(address & 0xFF, value, time);
		}
		break;
	case EN_SCCPLUS:
		if ((0xB800 <= address) && (address < 0xC000)) {
			scc->writeMemInterface(address & 0xFF, value, time);
		}
		break;
	}
}

byte* MSXSCCPlusCart::getWriteCacheLine(word start) const
{
	//return NULL;
	if ((0x4000 <= start) && (start < 0xC000)) {
		if (start == (0xBFFF & CPU::CACHE_LINE_HIGH)) {
			return NULL;
		}
		int regio = (start >> 13) - 2;
		if (isRamSegment[regio] && isMapped[regio]) {
			return &internalMemoryBank[regio][start & 0x1FFF];
		}
		return NULL;
	}
	return unmappedWrite;
}


void MSXSCCPlusCart::setMapper(int regio, byte value)
{
	mapper[regio] = value;
	value &= mapperMask;
	
	byte* block;
	if ((!lowRAM  && (value <  8)) ||
	    (!highRAM && (value >= 8))) {
		block = unmappedRead;
		isMapped[regio] = false;
	} else {
		block = &(*ram)[0x2000 * value];
		isMapped[regio] = true;
	}
	
	checkEnable();
	internalMemoryBank[regio] = block;
	MSXCPU::instance().invalidateMemCache(0x4000 + regio * 0x2000, 0x2000);
}

void MSXSCCPlusCart::setModeRegister(byte value)
{
	modeRegister = value;
	checkEnable();
	
	if (modeRegister & 0x20) {
		scc->setChipMode(SCC::SCC_plusmode);
	} else {
		scc->setChipMode(SCC::SCC_Compatible);
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
				MSXCPU::instance().invalidateMemCache(0x4000, 0x2000);
				isRamSegment[0] = false;
			}
		}
		if (modeRegister & 0x02) {
			isRamSegment[1] = true;
		} else {
			if (isRamSegment[1]) {
				MSXCPU::instance().invalidateMemCache(0x6000, 0x2000);
				isRamSegment[1] = false;
			}
		}
		if ((modeRegister & 0x24) == 0x24) {
			// extra requirement for this bank: SCC+ mode
			isRamSegment[2] = true;
		} else {
			if (isRamSegment[2]) {
				MSXCPU::instance().invalidateMemCache(0x8000, 0x2000);
				isRamSegment[2] = false;
			}
		}
		if (isRamSegment[3]) {
			MSXCPU::instance().invalidateMemCache(0xA000, 0x2000);
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

} // namespace openmsx

// $Id$

#include "MSXSCCPlusCart.hh"

#ifndef DONT_WANT_SCC

#include "MSXConfig.hh"
#include "SCC.hh"

#include <iostream>


MSXSCCPlusCart::MSXSCCPlusCart(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time)
{
	PRT_DEBUG("instantiating an MSXSCCPlusCart object");

	short volume = (short)config->getParameterAsInt("volume");
	cartridgeSCC = new SCC(volume);
	cartridgeSCC->setChipMode(SCC::SCC_Compatible);

	// allocate buffer
	memoryBank = new byte[131072];
	if (memoryBank == NULL)
		PRT_ERROR("Couldn't create SCC+ rambank!");
	memset(memoryBank, 255, 131072);

	PRT_DEBUG("SCC+ readromfile" << deviceConfig->getParameterAsBool("readromfile"));

	if (deviceConfig->getParameterAsBool("readromfile")) {
		std::string filename=deviceConfig->getParameter("filename");
		PRT_DEBUG("SCC+ romfile" << filename);

		// read the rom file
		// dynamically determine romSize if needed
		file = FileOpener::openFileRO(filename);
		file->seekg(0,std::ios::end);
		int romSize = file->tellg();
		PRT_DEBUG("SCC+ MegaRom: rom size is " << romSize);

		file->seekg(0,std::ios::beg);
		file->read(memoryBank, romSize);
		if (file->fail())
			PRT_ERROR("Error reading " << filename);
		delete file;
	}
	// set internalMemoryBank
	internalMemoryBank[0]=memoryBank+0x0000;
	internalMemoryBank[1]=memoryBank+0x2000;
	internalMemoryBank[2]=memoryBank+0x4000;
	internalMemoryBank[3]=memoryBank+0x6000;
}

MSXSCCPlusCart::~MSXSCCPlusCart()
{
	PRT_DEBUG("Destructing an MSXSCCPlusCart object");
	delete cartridgeSCC;
}


byte MSXSCCPlusCart::readMem(word address, const EmuTime &time)
{
	// ModeRegister can not be read!;

	if ((address<0x4000) || (address>=0xc000))
		return;
	//SCC(+) not visible !!!
	if (enable == 0) 
		return internalMemoryBank[(address>>13)-2][address & 0x1fff];
	//SCC visible in 9800 - 0x9FDF
	if ((enable == 1) && (0x9800<=address)&&(address<0xA000)) {
		return cartridgeSCC->readMemInterface(address & 0xFF, time);
	} else 
		//SCC+ visible in B800 - 0xBFDF
		if ((enable == 2) && (0xB800<=address) && (address<0xC000)) {
			return cartridgeSCC->readMemInterface(address & 0xFF, time);
		}
	// SCC(+) enabled but not requested so memory stuff
	return internalMemoryBank[(address>>13)-2][address & 0x1fff];
}

void MSXSCCPlusCart::writeMem(word address, byte value, const EmuTime &time)
{
	short regio;

	/*the normal 8Kb bankswiching routines!!!
	 * The address to change banks:
	 bank 1: 0x5000 - 0x57FF (0x5000 used)
	 bank 2: 0x7000 - 0x77FF (0x7000 used)
	 bank 3: 0x9000 - 0x97FF (0x9000 used)
	 bank 4: 0xB000 - 0xB7FF (0xB000 used)
	 */
	if ((address<0x5000) || (address>=0xb800))
		return;
	regio = (address-0x5000)>>13;
	if (isRamSegment[regio]) {
		//According to Sean Young
		// when the regio's are in RAM mode you can read from 
		// the SCC(+) but not write to them 
		// => whe assume a write to the memory but maybe
		//     they are just discarded
		// TODO check this out => ask Sean...
		internalMemoryBank[regio][address&0x1FFF] = value;
		return;
	}

	if (!((address < 0x5000) || (address >0xB000) || ((address & 0x1800)!=0x1000))) {
		// 0x9000: Here the SCC is enabled when writing 0x3F
		// bit 6 and 7 are ignored
		// type konami5/8b 
		if (regio == (ModeRegister & 0x20 ? 3 : 2)) {
			// make SCC(+) registers visible
			scc_banked = value;
			checkEnable();
		} else {
			/* change internal mapper*/
			value &= 15;
			internalMemoryBank[regio] = memoryBank+(0x2000*value);
			internalMapper[regio] = value;
		}
		return;
	}

	// Mode register is mapped upon 0xBFFE and 0xBFFF
	if ((address | 0x0001) == 0xBFFF) {
		setModeRegister(value);
	}
	// call writememinterface of SCC if needed
	switch (enable){
	case 1:
		if ((0x9800<=address)&&(address<0xA000)) {
			cartridgeSCC->writeMemInterface(address & 0xFF, value, time);
		}
		break;
	case 2:
		if ((0xB800<=address)&&(address<0xC000)) {
			cartridgeSCC->writeMemInterface(address & 0xFF, value, time);
		}
	}
	return;
}

void MSXSCCPlusCart::setModeRegister(byte value)
{
	ModeRegister = value;
	if (ModeRegister & 0x20) {
		cartridgeSCC->setChipMode(SCC::SCC_plusmode);
	} else {
		cartridgeSCC->setChipMode(SCC::SCC_Compatible);
	}
	checkEnable();
	if (ModeRegister & 16) {
		isRamSegment[0] = true;
		isRamSegment[1] = true;
		isRamSegment[2] = true;
		isRamSegment[3] = true;
	} else if (ModeRegister & 1) {
		isRamSegment[0] = true;
	} else if (ModeRegister & 2) {
		isRamSegment[1] = true;
	} else if ((ModeRegister & 36) == 36) {
		//extra requirement for this bank: SCC+ mode, and not SCC mode
		isRamSegment[2] = true;
	}
}

void MSXSCCPlusCart::reset(const EmuTime &time)
{
	PRT_DEBUG ("Resetting " << getName());

	scc_banked = 0x3f;
	setModeRegister(0);
}

void MSXSCCPlusCart::checkEnable()
{
	if ((ModeRegister & 0x20)&&(scc_banked & 0x80)) 
		enable = 2;
	else if ((!(ModeRegister & 0x20))&&((scc_banked & 0x3F) == 0x3F))
		enable = 1;
	else 
		enable = 0;
}

#endif // ndef DONT_WANT_SCC

// $Id$

#include "MSXGameCartridge.hh" 
#include "MSXMotherBoard.hh"
#include <string>
#include <iostream>
#include <fstream>
#include "SCC.hh"
#include "DACSound.hh"
#include "MSXCPU.hh"
#include "CPU.hh"

#include "config.h"

MSXGameCartridge::MSXGameCartridge(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXGameCartridge object");
	
	try {
		romSize = deviceConfig->getParameterAsInt("filesize");
		// filesize zero is specifiying to autodetermine size
		if (romSize == 0){
			romSize = loadFile(&memoryBank);
		} else {
			loadFile(&memoryBank, romSize);
		}
	} catch(MSXConfig::Exception e) {
		// filesize was not specified
		romSize = loadFile(&memoryBank);
	}
	
	// Calculate mapperMask
	int nrblocks = romSize>>13;	//number of 8kB pages
	for (mapperMask=1; mapperMask<nrblocks; mapperMask<<=1);
	mapperMask--;
	
	mapperType = retriefMapperType();
	//only instanciate SCC if needed
	if (mapperType==2) {
		short volume = (short)config->getParameterAsInt("volume");
		cartridgeSCC = new SCC(volume);
	} else {
		cartridgeSCC = NULL;
	}
	if (mapperType==64) {
		short volume = (short)config->getParameterAsInt("volume");
		dac = new DACSound(volume, 16000, time);
	} else {
		dac = NULL;
	}

	reset(time);
}

MSXGameCartridge::~MSXGameCartridge()
{
	PRT_DEBUG("Destructing an MSXGameCartridge object");
	if (dac)
		delete dac;
	if (cartridgeSCC)
		delete cartridgeSCC;
}

void MSXGameCartridge::reset(const EmuTime &time)
{
	if (cartridgeSCC) {
		cartridgeSCC->reset();
	}
	if (dac) {
		dac->reset(time);
	}
	if (mapperType < 128 ) {
		// TODO: mirror if number of 8kB blocks not fully filled ?
		setBank(0, 0);			// unused
		setBank(1, 0);			// unused
		setBank(2, memoryBank);		// 0x4000 - 0x5fff
		setBank(3, memoryBank+0x2000);	// 0x6000 - 0x7fff
		setBank(4, memoryBank+0x4000);	// 0x8000 - 0x9fff
		setBank(5, memoryBank+0x6000);	// 0xa000 - 0xbfff
		setBank(6, 0);			// unused
		setBank(7, 0);			// unused
	} else {
		// this is a simple gamerom less then 64 kB
		byte* ptr;
		switch (romSize>>14) { // blocks of 16kB
		case 0:
			// An 8 Kb game ????
			for (int i=0; i<8; i++) {
				setBank(i, memoryBank);
			}
			break;
		case 1:
			for (int i=0; i<8; i+=2) {
				setBank(i,   memoryBank);
				setBank(i+1, memoryBank+0x2000);
			}
			break;
		case 2:
			setBank(0, memoryBank);		// 0x0000 - 0x1fff
			setBank(1, memoryBank+0x2000);	// 0x2000 - 0x3fff
			setBank(2, memoryBank);		// 0x4000 - 0x5fff
			setBank(3, memoryBank+0x2000);	// 0x6000 - 0x7fff
			setBank(4, memoryBank+0x4000);	// 0x8000 - 0x9fff
			setBank(5, memoryBank+0x6000);	// 0xa000 - 0xbfff
			setBank(6, memoryBank+0x4000);	// 0xc000 - 0xdfff
			setBank(7, memoryBank+0x6000);	// 0xe000 - 0xffff
			break;
		case 3:
			// TODO 48kb, is this possible?
			assert (false);
			break;
		case 4:
			ptr = memoryBank;
			for (int i=0; i<8; i++) {
				setBank(i, ptr);
				ptr += 0x2000;
			}
			break;
		default: 
			// not possible
			assert (false);
		}
	}
}

int MSXGameCartridge::retriefMapperType()
{
	try {
		if (deviceConfig->getParameterAsBool("automappertype")) {
			return guessMapperType();
		} else {
			int type = deviceConfig->getParameterAsInt("mappertype");
			PRT_DEBUG("Using mapper type " << type);
			return type;
		}
	} catch (MSXConfig::Exception e) {
		// missing parameter
		return guessMapperType();
	}
}

int MSXGameCartridge::guessMapperType()
{
	//  GameCartridges do their bankswitching by using the Z80
	//  instruction ld(nn),a in the middle of program code. The
	//  adress nn depends upon the GameCartridge mappertype used.
	//  To guess which mapper it is, we will look how much writes
	//  with this instruction to the mapper-registers-addresses
	//  occure.

	if (romSize <= 0x10000) {
		if (romSize == 0x8000) {
			//TODO: Autodetermine for KonamiSynthesiser
			// works for now since most 32 KB cartridges don't
			// write to themselves
			return 64;
		} else {
			// if != 32kB it must be a simple rom so we return 128
			return 128;
		}
	} else {
		unsigned int typeGuess[]={0,0,0,0,0,0};
		for (int i=0; i<romSize-2; i++) {
			if (memoryBank[i] == 0x32) {
				int value = memoryBank[i+1]+(memoryBank[i+2]<<8);
				switch (value) {
				case 0x5000:
				case 0x9000:
				case 0xB000:
					typeGuess[2]++;
					break;
				case 0x4000:
				case 0x8000:
				case 0xA000:
					typeGuess[3]++;
					break;
				case 0x6800:
				case 0x7800:
					typeGuess[4]++;
					break;
				case 0x6000:
					typeGuess[3]++;
					typeGuess[4]++;
					typeGuess[5]++;
					break;
				case 0x7000:
					typeGuess[2]++;
					typeGuess[4]++;
					typeGuess[5]++;
					break;
				case 0x77FF:
					typeGuess[5]++;
				}
			}
		}
		// in case of doubt we go for type 0
		typeGuess[0]++;
		// in case of even type 5 and 4 we would prefer 5 
		// but we would still prefer 0 above 4 or 5 so no increment
		if (typeGuess[4]) typeGuess[4]--; // -1 -> max_int
		int type = 0;
		for (int i=0; i<6; i++) {
			if (typeGuess[i]>typeGuess[type]) 
				type = i;
		}
		PRT_DEBUG("I Guess this is a nr " << type << " GameCartridge mapper type.")
		return type;
	}
}

byte MSXGameCartridge::readMem(word address, const EmuTime &time)
{
	//TODO optimize this!!!
	if ((mapperType == 2) && enabledSCC) {
		if ((address-0x5000)>>13 == 2) {
			return cartridgeSCC->readMemInterface(address & 0xFF, time);
		}
	}
	return internalMemoryBank[address>>13][address&0x1fff];
}

byte* MSXGameCartridge::getReadCacheLine(word start)
{
	// assumes CACHE_LINE_SIZE <= 0x2000
	return &internalMemoryBank[start>>13][start&0x1fff];
}

void MSXGameCartridge::writeMem(word address, byte value, const EmuTime &time)
{
	byte regio;
  
	switch (mapperType) {
	case 0: 
		//TODO check the SCC in here, is this correct for this type?
		// if so, implement writememinterface
		// also in the memRead

		//--==>> Generic 8kB cartridges <<==--
		if ((address<0x4000) || (address>0xBFFF))
			return;
		if ((address&0xe000) == 0x8000 ) {
			// 0x8000..0x9FFF is used as SCC switch
			enabledSCC=((value&0x3F)==0x3F)?true:false;
		}
		// change internal mapper
		value &= mapperMask;
		regio = (address>>13);	// 0-7
		setBank(regio, memoryBank+(value<<13));
		break;
	case 1: 
		//--==**>> Generic 16kB cartridges (MSXDOS2, Hole in one special) <<**==--
		if ((address<0x4000) || (address>0xBFFF))
			return;
		regio = (address&0xc000)>>13;	// 0, 2, 4, 6
		value &= (2*value)&mapperMask;
		setBank(regio,   memoryBank+(value<<13));
		setBank(regio+1, memoryBank+(value<<13)+0x2000);
		break;
	case 2: 
		//--==**>> KONAMI5 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do have an SCC and some others
		// example of catrridges: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow
		// Solid Snake, Quarth, Ashguine 1, Animal, Arkanoid 2, ...
		// 
		// The address to change banks:
		//  bank 1: 0x5000 - 0x57FF (0x5000 used)
		//  bank 2: 0x7000 - 0x77FF (0x7000 used)
		//  bank 3: 0x9000 - 0x97FF (0x9000 used)
		//  bank 4: 0xB000 - 0xB7FF (0xB000 used)
      
		if ((address<0x5000) || (address>0xc000) )
			return;
		// Enable/disable SCC
		if ((address&0xF800) == 0x9000 )  {
			// 0x9000-0x97ff is used as SCC switch
			enabledSCC = ((value&0x3F)==0x3F) ? true : false;
			if (enabledSCC)
				// TODO find out how it changes the page in  0x9000-0x97ff
				return; 
		}
		// change internal mapper
		regio = (address-0x1000)>>13;
		if ( (regio==4) && enabledSCC && (address >= 0x9800)){
		  cartridgeSCC->writeMemInterface(address & 0xFF, value , time);
		  PRT_DEBUG("GameCartridge: writeMemInterface (" << address <<","<<(int)value<<")"<<time );
		} else {
		  if ((address & 0x1800)!=0x1000) return;
		  value &= mapperMask;
		  setBank(regio, memoryBank+(value<<13));
		  //internalMapper[regio]=value;
		}
		break;
	case 3: 
		//--==**>> KONAMI4 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do not have an SCC and some others
		// example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom, 
		// The Maze of Galious, Aleste 1, 1942,Heaven, Mystery World, ...
		//
		// page at 4000 is fixed, other banks are switched
		// by writting at 0x6000,0x8000 and 0xA000
		 
		if ((address<0x6000) || (address>0xa000) || (address&0x1FFF))
			return;
		regio = (address>>13);
		value &= mapperMask;
		setBank(regio, memoryBank+(value<<13));
		break;
	case 4: 
		//--==**>> ASCII 8kB cartridges <<**==--
		// this type is used in many japanese-only cartridges.
		// example of cartridges: Valis(Fantasm Soldier), Dragon Slayer, Outrun, Ashguine 2, ...
		//
		// The address to change banks:
		//  bank 1: 0x6000 - 0x67FF (0x5000 used)
		//  bank 2: 0x6800 - 0x6FFF (0x7000 used)
		//  bank 3: 0x7000 - 0x77FF (0x9000 used)
		//  bank 4: 0x7800 - 0x7FFF (0xB000 used)
      
		if ((address<0x6000) || (address>0x7fff))
			return;
		regio = ((address>>11)&3)+2;
		value &= mapperMask;
		setBank(regio, memoryBank+(value<<13));
		break;
	case 5:
		//--==**>> ASCII 16kB cartridges <<**==--
		// this type is used in a few cartridges.
		// example of cartridges: Xevious, Fantasy Zone 2, Return of Ishitar, Androgynus, ...
		//
		// Gallforce is a special case after a reset the second 16kb has to start with 
		// the first 16kb after a reset
		//
		// The address to change banks:
		//  first  16kb: 0x6000 - 0x67FF (0x6000 used)
		//  second 16kb: 0x7000 - 0x77FF (0x7000 and 0x77FF used)
      
		if ((address<0x6000) || (address>0x77ff) || (address&0x800))
			return;
		regio = ((address>>11)&2)+2;
		value &= (2*value)&mapperMask;
		setBank(regio,   memoryBank+(value<<13));
		setBank(regio+1, memoryBank+(value<<13)+0x2000);
		break;
	case 6: 
		//--==**>> GameMaster2+SRAM cartridge <<**==--
		//// GameMaster2 mayi become an independend MSXDevice
		assert(false);
		break;
	case 64:
		//--==**>> <<**==--
		// Konami Synthezier cartridge
		// Should be only for 0x4000, but since this isn't confirmed on a real
		// cratridge we will simply use every write.
		dac->writeDAC(value,time);
		
	case 128: 
		//--==**>> Simple romcartridge <= 64 KB <<**==--
		// No extra writable hardware
		break;
	default:
		//// Unknow mapper type for GameCartridge cartridge!!!
		assert(false);
	}
}

void MSXGameCartridge::setBank(int regio, byte* value)
{
	internalMemoryBank[regio] = value;
	MSXCPU::instance()->invalidateCache(regio*0x2000, 0x2000/CPU::CACHE_LINE_SIZE);
}

// $Id$

#include "MSXRom.hh"
#include "SCC.hh"
#include "DACSound.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include <map>


MSXRom::MSXRom(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time), MSXRomDevice(config, time)
{
	PRT_DEBUG("Creating an MSXRom object");
	try {
		try {
			romSize = deviceConfig->getParameterAsInt("filesize");
			if (romSize == 0) {
				// filesize zero is specifying to autodetermine size
				throw MSXConfig::Exception("auto detect");
			}
			loadFile(&memoryBank, romSize);
		} catch (MSXConfig::Exception& e) {
			// filesize was not specified or 0
			romSize = loadFile(&memoryBank);
		}
	} catch (MSXException &e) {
		// TODO why isn't exception thrown beyond constructor
		PRT_ERROR(e.desc);
	}
	retrieveMapperType();

	// only if needed reserve memory for SRAM
	if (mapperType & 16) {
		memorySRAM = new byte[0x2000];
		memset(memorySRAM, 255, 0x2000);
		try {
			if (deviceConfig->getParameterAsBool("loadsram")) {
				std::string filename = deviceConfig->getParameter("sramname");
				PRT_DEBUG("Trying to read "<<filename<<" as SRAM of the cartrdige");
				IFILETYPE* file = FileOpener::openFileRO(filename);
				file->read((char*)memorySRAM, 0x2000);
				file->close();
				delete file;
			}
		} catch (MSXException &e) {
			// do nothing
		}
	} else {
		memorySRAM = NULL;
	}

#ifndef DONT_WANT_SCC
	// only instantiate SCC if needed
	if (mapperType == 2) {
		short volume = (short)config->getParameterAsInt("volume");
		cartridgeSCC = new SCC(volume);
	} else {
		cartridgeSCC = NULL;
	}
#endif

	// only instantiate DACSound if needed
	if (mapperType == 64) {
		short volume = (short)config->getParameterAsInt("volume");
		dac = new DACSound(volume, 16000, time);
	} else {
		dac = NULL;
	}

	// to emulate non-present memory
	unmapped = new byte[0x4000];
	memset(unmapped, 255, 0x4000);
	
	reset(time);
}

MSXRom::~MSXRom()
{
	PRT_DEBUG("Destructing an MSXRom object");
	if (dac)
		delete dac;
#ifndef DONT_WANT_SCC
	if (cartridgeSCC)
		delete cartridgeSCC;
#endif
	if ((mapperType & 16) && deviceConfig->getParameterAsBool("savesram")) {
		std::string filename = deviceConfig->getParameter("sramname");
		PRT_DEBUG("Trying to save to "<<filename<<" for SRAM of the cartrdige");
		IOFILETYPE* file = FileOpener::openFileTruncate(filename);
		file->write((char*)memorySRAM, 0x2000);
		file->close();
		delete file;
	}
	delete[] memorySRAM;
	delete[] unmapped;
}

void MSXRom::reset(const EmuTime &time)
{
#ifndef DONT_WANT_SCC
	if (cartridgeSCC) {
		cartridgeSCC->reset();
	}
	enabledSCC = false;
#endif
	if (dac) {
		dac->reset(time);
	}
	// After a reset SRAM is not selected in all known cartrdiges
	regioSRAM = 0;

	if (mapperType < 128) {
		setBank16kB(0, unmapped);
		setROM16kB (1, 0);
		setROM16kB (2, (mapperType == 16 || mapperType == 5) ? 0 : 1);
		setBank16kB(3, unmapped);
	} else {
		// this is a simple gamerom less or equal then 64 kB
		switch (romSize >> 14) { // blocks of 16kB
			case 0:	//  8kB
				for (int i=0; i<8; i++)
					setROM8kB(i, 0);
				break;
			case 1:	// 16kB
				setROM16kB(0, 0);
				setROM16kB(1, 0);
				setROM16kB(2, 0);
				setROM16kB(3, 0);
				break;
			case 2:	// 32kB
				setROM16kB(0, 0);
				setROM16kB(1, 0);
				setROM16kB(2, 1);
				setROM16kB(3, 1);
				break;
			case 3:	// 48kB   TODO is this possible
				setROM16kB(0, 0);
				setROM16kB(1, 1);
				setROM16kB(2, 2);
				setROM16kB(3, 2);	// TODO check this
				break;
			case 4:	// 64kB
				setROM16kB(0, 0);
				setROM16kB(1, 1);
				setROM16kB(2, 2);
				setROM16kB(3, 3);
				break;
			default:
				// not possible
				assert(false);
		}
	}
}

struct ltstr {
	bool operator()(const std::string s1, const std::string s2) const {
		return strcasecmp(s1.c_str(), s2.c_str()) < 0;
	}
};

void MSXRom::retrieveMapperType()
{
	try {
		if (deviceConfig->getParameterAsBool("automappertype")) {
			mapperType = guessMapperType();
		} else {
			std::map<const std::string, int, ltstr> mappertype;

			mappertype["0"]=0;
			mappertype["8kB"]=0;

			mappertype["1"]=1;
			mappertype["16kB"]=1;

			mappertype["2"]=2;
			mappertype["KONAMI5"]=2;
			mappertype["SCC"]=2;

			mappertype["3"]=3;
			mappertype["KONAMI4"]=3;

			mappertype["4"]=4;
			mappertype["ASCII8"]=4;

			mappertype["5"]=5;
			mappertype["ASCII16"]=5;

			//Not implemented yet
			mappertype["7"]=7;
			mappertype["RTYPE"]=7;
			
			//Cartridges with sram
			mappertype["16"]=16;
			mappertype["HYDLIDE2"]=16;

			mappertype["17"]=17;
			mappertype["XANADU"]=17;
			mappertype["ASCII8SRAM"]=17;

			mappertype["18"]=18;
			mappertype["ASCII8SRAM2"]=18;
			mappertype["ROYALBLOOD"]=18;

			mappertype["19"]=19;
			mappertype["GAMEMASTER2"]=19;
			mappertype["RC755"]=19;

			mappertype["64"]=64;
			mappertype["KONAMIDAC"]=64;

			mappertype["128"]=128;
			mappertype["64kB"]=128;

			//TODO: catch wrong options passed
			std::string  type = deviceConfig->getParameter("mappertype");
			mapperType = mappertype[type];
		}
	} catch (MSXConfig::Exception& e) {
		// missing parameter
		mapperType = guessMapperType();
	}
	
	PRT_DEBUG("mapperType: " << mapperType);
}

int MSXRom::guessMapperType()
{
	//  GameCartridges do their bankswitching by using the Z80
	//  instruction ld(nn),a in the middle of program code. The
	//  adress nn depends upon the GameCartridge mappertype used.
	//  To guess which mapper it is, we will look how much writes
	//  with this instruction to the mapper-registers-addresses
	//  occure.

	if (romSize <= 0x10000) {
		if (romSize == 0x10000) {
			// There are some programs convert from tape to 64kB rom cartridge
			// these 'fake'roms are from the ASCII16 type
			return 5;
		} else if (romSize == 0x8000) {
			//TODO: Autodetermine for KonamiSynthesiser
			// works for now since most 32 KB cartridges don't
			// write to themselves
			return 64;
		} else {
			// if != 32kB it must be a simple rom so we return 128
			return 128;
		}
	} else {
		unsigned int typeGuess[] = {0,0,0,0,0,0,0};
		for (int i=0; i<romSize-3; i++) {
			if (memoryBank[i] == 0x32) {
				word value = memoryBank[i+1]+(memoryBank[i+2]<<8);
				switch (value) {
				case 0x5000:
				case 0x9000:
				case 0xb000:
					typeGuess[2]++;
					break;
				case 0x4000:
					typeGuess[3]++;
					break;
				case 0x8000:
				case 0xa000:
					typeGuess[3]++;
					typeGuess[6]++;
					break;
				case 0x6800:
				case 0x7800:
					typeGuess[4]++;
					break;
				case 0x6000:
					typeGuess[3]++;
					typeGuess[4]++;
					typeGuess[5]++;
					typeGuess[6]++;
					break;
				case 0x7000:
					typeGuess[2]++;
					typeGuess[4]++;
					typeGuess[5]++;
					break;
				case 0x77ff:
					typeGuess[5]++;
					break;
				default:
					if (value>0xb000 && value<0xc000) typeGuess[6]++;
				}
			}
		}
		if (typeGuess[4]) typeGuess[4]--; // -1 -> max_int
		if (typeGuess[6] != 75) typeGuess[6] = 0; // There is only one Game Master 2
		int type = 0;
		for (int i=0; i<7; i++) {
			if ((typeGuess[i]) && (typeGuess[i]>=typeGuess[type])) {
				type = i;
			}
		}
		// in case of doubt we go for type 0
		// in case of even type 5 and 4 we would prefer 5
		// but we would still prefer 0 above 4 or 5
		if ((type==5) && (typeGuess[0] == typeGuess[5]))
			type=0;
		for (int i=0; i<7; i++)
			PRT_DEBUG("MSXRom: typeGuess["<<i<<"]="<<typeGuess[i]);
		return type == 6 ? 19 : type;
	}
}

byte MSXRom::readMem(word address, const EmuTime &time)
{
	//TODO optimize this (Necessary? We have read cache now)
	// One way to optimise would be to register an SCC supporting
	// device only if mapperType is 2 and only in 8000..Bfff.
	// That way, there is no SCC overhead in non-SCC pages.
	// If MSXMotherBoard would support hot-plugging of devices,
	// it would be possible to insert an SCC supporting device
	// only when the SCC is enabled.
#ifndef DONT_WANT_SCC
	if (enabledSCC && 0x9800<=address && address<0xa000) {
		return cartridgeSCC->readMemInterface(address&0xff, time);
	}
#endif
	return internalMemoryBank[address>>12][address&0x0fff];
}

byte* MSXRom::getReadCacheLine(word start)
{
#ifndef DONT_WANT_SCC
	if (enabledSCC && 0x9800<=start && start<0xa000) {
		// don't cache SCC
		return NULL;
	}
#endif
	if (CPU::CACHE_LINE_SIZE <= 0x1000) {
		return &internalMemoryBank[start>>12][start&0x0fff];
	} else {
		return NULL;
	}
}

void MSXRom::writeMem(word address, byte value, const EmuTime &time)
{
	switch (mapperType) {
	case 0:
		//--==**>> Generic 8kB cartridges <<**==--
		if (0x4000<=address && address<0xc000) {
			setROM8kB(address>>13, value);
		}
		break;
		
	case 1:
		//--==**>> Generic 16kB cartridges <<**==--
		// examples: MSXDOS2, Hole in one special
		
		if (0x4000<=address && address<0xc000) {
			byte region = (address&0xc000)>>14;	// 0..3
			setROM16kB(region, value);
		}
		break;
	
	case 2:
		//--==**>> KONAMI5 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do have an SCC and some others
		// examples of cartridges: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow
		// Solid Snake, Quarth, Ashguine 1, Animal, Arkanoid 2, ...
		//
		// The address to change banks:
		//  bank 1: 0x5000 - 0x57ff (0x5000 used)
		//  bank 2: 0x7000 - 0x77ff (0x7000 used)
		//  bank 3: 0x9000 - 0x97ff (0x9000 used)
		//  bank 4: 0xB000 - 0xB7ff (0xB000 used)

		if (address<0x5000 || address>=0xc000) break;
#ifndef DONT_WANT_SCC
		// Write to SCC?
		if (enabledSCC && 0x9800<=address && address<0xa000) {
			cartridgeSCC->writeMemInterface(address&0xff, value, time);
			// No page selection in this memory range.
			break;
		}
		// SCC enable/disable?
		if ((address & 0xf800) == 0x9000) {
			enabledSCC = ((value & 0x3f) == 0x3f);
			MSXCPU::instance()->invalidateCache(0x9800, 0x0800/CPU::CACHE_LINE_SIZE);
		}
#endif
		// Page selection?
		if ((address & 0x1800) == 0x1000)
			setROM8kB(address>>13, value);
		break;
	
	case 3:
		//--==**>> KONAMI4 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do not have an SCC and some others
		// example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
		// The Maze of Galious, Aleste 1, 1942,Heaven, Mystery World, ...
		//
		// page at 4000 is fixed, other banks are switched
		// by writting at 0x6000,0x8000 and 0xa000

		if (address==0x6000 || address==0x8000 || address==0xa000)
			setROM8kB(address>>13, value);
		break;
		
	case 4:
		//--==**>> ASCII 8kB cartridges <<**==--
		// this type is used in many japanese-only cartridges.
		// example of cartridges: Valis(Fantasm Soldier), Dragon Slayer, Outrun, 
		//                        Ashguine 2, ...
		// The address to change banks:
		//  bank 1: 0x6000 - 0x67ff (0x6000 used) 
		//  bank 2: 0x6800 - 0x6fff (0x6800 used)
		//  bank 3: 0x7000 - 0x77ff (0x7000 used)
		//  bank 4: 0x7800 - 0x7fff (0x7800 used)

		if (0x6000<=address && address<0x8000) {
			byte region = ((address>>11)&3)+2;
			setROM8kB(region, value);
		}
		break;
		
	case 5:
		//--==**>> ASCII 16kB cartridges <<**==--
		// this type is used in a few cartridges.
		// example of cartridges: Xevious, Fantasy Zone 2,
		// Return of Ishitar, Androgynus, Gallforce ...
		//
		// The address to change banks:
		//  first  16kb: 0x6000 - 0x67ff (0x6000 used)
		//  second 16kb: 0x7000 - 0x77ff (0x7000 and 0x77ff used)
		
		if (0x6000<=address && address<0x7800 && !(address&0x0800)) {
			byte region = ((address>>12)&1)+1;
			setROM16kB(region, value);
		}
		break;
		
	case 16:
		//--==**>> HYDLIDE2 cartridges <<**==--
		// this type is is almost completely a ASCII16 cartrdige
		// However, it has 2kB of SRAM (and 128 kB ROM)
		// Use value 0x10 to select the SRAM.
		// SRAM in page 1 => read-only
		// SRAM in page 2 => read-write
		// The 2Kb SRAM (0x800 bytes) are mirrored in the 16 kB block
		//
		// The address to change banks (from ASCII16):
		//  first  16kb: 0x6000 - 0x67ff (0x6000 used)
		//  second 16kb: 0x7000 - 0x77ff (0x7000 and 0x77ff used)

		if (0x6000<=address && address<0x7800 && !(address&0x0800)) {
			byte region = ((address>>12)&1)+1;
			if (value == 0x10) {
				// SRAM block
				setBank8kB(2*region,   memorySRAM);
				setBank8kB(2*region+1, memorySRAM);
				regioSRAM |= (region==1 ?  0x0c :  0x30);
			} else {
				// Normal 16 kB ROM page
				setROM16kB(region, value);
				regioSRAM &= (region==1 ? ~0x0c : ~0x30);
			}
		} else {
			// Writting to SRAM?
			if ((1 << (address>>13)) & regioSRAM & 0x0c) { 
				for (word adr=address&0x7ff; adr<0x2000; adr+=0x800)
					memorySRAM[adr] = value;
			}
		}
		break;
		
	case 17:
	case 18:
		//--==**>> ASCII 8kB cartridges with 8kB SRAM<<**==--
		// this type is used in Xanadu and Royal Blood
		//
		// The address to change banks:
		//  bank 1: 0x6000 - 0x67ff (0x6000 used)
		//  bank 2: 0x6800 - 0x6fff (0x6800 used)
		//  bank 3: 0x7000 - 0x77ff (0x7000 used)
		//  bank 4: 0x7800 - 0x7fff (0x7800 used)
		//
		//  To select SRAM set bit 5/7 of the bank.
		//  The SRAM can only be written to if selected in bank 3 or 4.

		if (0x6000<=address && address<0x8000) {
			byte region = ((address>>11)&3)+2;
			byte SRAMEnableBit = (mapperType==17) ? 0x20 : 0x80; 
			if (value & SRAMEnableBit) {
				//bit 7 for Royal Blood
				//bit 5 for Xanadu
				setBank8kB(region, memorySRAM);
				regioSRAM |=  (1 << region);
			} else {
				setROM8kB(region, value);
				regioSRAM &= ~(1 << region);
			}
		} else {
			// Writting to SRAM?
			if ((1 << (address>>13)) & regioSRAM & 0x0c) { 
				memorySRAM[address & 0x1fff] = value;
			}
		}
		break;
		
	case 19:
		// Konami Game Master 2, 8KB cartridge with 8KB sram
		if (0x6000<=address && address<0xc000) {
			if (address >= 0xb000) {
				// SRAM write
				if (regioSRAM & 0x20)	// 0xa000 - 0xbfff
					internalMemoryBank[0xb][address&0x0fff] = value;
			}
			if (!(address & 0x1000)) {
				byte region = address>>13;	// 0..7
				if (value & 0x10) {
					// switch sram in page
					regioSRAM |=  (1 << region);
					if (value & 0x20) {
						setBank4kB(2*region,   memorySRAM+0x1000);
						setBank4kB(2*region+1, memorySRAM+0x1000);
					} else {
						setBank4kB(2*region,   memorySRAM+0x0000);
						setBank4kB(2*region+1, memorySRAM+0x0000);
					}
				} else {
					// switch normal memory
					regioSRAM &= ~(1 << region);
					setROM8kB(region, value);
				}
			}
		}
		break;
	
	case 64:
		//--==**>> <<**==--
		// Konami Synthezier cartridge
		// Should be only for 0x4000, but since this isn't confirmed on a real
		// cratridge we will simply use every write.
		dac->writeDAC(value,time);
		break;
		
	case 128:
		//--==**>> Simple romcartridge <= 64 KB <<**==--
		// No extra writable hardware
		break;
		
	default:
		// Unknow mapper type
		assert(false);
	}
}

void MSXRom::setBank4kB(int region, byte* adr)
{
	internalMemoryBank[region] = adr;
	MSXCPU::instance()->invalidateCache(region*0x1000, 0x1000/CPU::CACHE_LINE_SIZE);
}
void MSXRom::setBank8kB(int region, byte* adr)
{
	internalMemoryBank[2*region+0] = adr+0x0000;
	internalMemoryBank[2*region+1] = adr+0x1000;
	MSXCPU::instance()->invalidateCache(region*0x2000, 0x2000/CPU::CACHE_LINE_SIZE);
}
void MSXRom::setBank16kB(int region, byte* adr)
{
	internalMemoryBank[4*region+0] = adr+0x0000;
	internalMemoryBank[4*region+1] = adr+0x1000;
	internalMemoryBank[4*region+2] = adr+0x2000;
	internalMemoryBank[4*region+3] = adr+0x3000;
	MSXCPU::instance()->invalidateCache(region*0x4000, 0x4000/CPU::CACHE_LINE_SIZE);
}

void MSXRom::setROM4kB(int region, int block)
{
	int nrBlocks = romSize >> 12;
	block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
	setBank4kB(region, memoryBank + (block<<12));
}
void MSXRom::setROM8kB(int region, int block)
{
	int nrBlocks = romSize >> 13;
	block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
	setBank8kB(region, memoryBank + (block<<13));
}
void MSXRom::setROM16kB(int region, int block)
{
	int nrBlocks = romSize >> 14;
	block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
	setBank16kB(region, memoryBank + (block<<14));
}

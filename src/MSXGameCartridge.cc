// $Id$

#include "MSXGameCartridge.hh"
#include "SCC.hh"
#include "DACSound.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include <map>


const byte MSXGameCartridge::adr2pag[];

MSXGameCartridge::MSXGameCartridge(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time), MSXRom(config, time)
{
	PRT_DEBUG("Creating an MSXGameCartridge object");
	try {
		try {
			romSize = deviceConfig->getParameterAsInt("filesize");
			if (romSize == 0) {
				// filesize zero is specifiying to autodetermine size
				throw MSXConfig::Exception("auto detect");
			}
			loadFile(&memoryBank, romSize);
		} catch(MSXConfig::Exception& e) {
			// filesize was not specified or 0
			romSize = loadFile(&memoryBank);
		}
	} catch (MSXException &e) {
		// TODO why isn't exception thrown beyond constructor
		PRT_ERROR(e.desc);
	}
	
	// Calculate mapperMask
	int nrblocks = romSize>>13;	//number of 8kB pages
	for (mapperMask=1; mapperMask<nrblocks; mapperMask<<=1);
	mapperMask--;
	
	retrieveMapperType();

	// only if needed reserve memory for SRAM
	if (mapperType & 16) {
		regioSRAM=0;
		enabledSRAM = true;
		memorySRAM = new byte[0x2000];
		memset(memorySRAM, 255, 0x2000);
		try {
			if (deviceConfig->getParameterAsBool("loadsram")) {
				std::string filename = deviceConfig->getParameter("sramname");
				PRT_INFO("Trying to read "<<filename<<" as SRAM of the cartrdige");
				IFILETYPE* file = FileOpener::openFileRO(filename);
				file->read(memorySRAM, 0x2000);
				file->close();
				delete file;
			}
		} catch (MSXException &e) {
			// do nothing
		}
	} else {
		enabledSRAM= false;
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

	unmapped = NULL;
	reset(time);
}

MSXGameCartridge::~MSXGameCartridge()
{
	PRT_DEBUG("Destructing an MSXGameCartridge object");
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
		file->write(memorySRAM, 0x2000);
		file->close();
		delete file;
	}
	delete[] memorySRAM;
	delete[] unmapped;
}

void MSXGameCartridge::reset(const EmuTime &time)
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
	regioSRAM=0;

	if (mapperType < 128 ) {
		if (unmapped == NULL) {
			unmapped = new byte[0x1000];
			memset(unmapped, 255, 0x1000);
		}
		// TODO: mirror if number of 8kB blocks not fully filled ?
		setBank8kB(2, memoryBank);			// 0x4000 - 0x5fff
		setBank8kB(3, memoryBank+0x2000);		// 0x6000 - 0x7fff
		if (mapperType == 16 || mapperType == 5) {
			setBank8kB(4, memoryBank+0x0000);	// 0x8000 - 0x9fff
			setBank8kB(5, memoryBank+0x2000);	// 0xa000 - 0xbfff
		} else {
			setBank8kB(4, memoryBank+0x4000);	// 0x8000 - 0x9fff
			setBank8kB(5, memoryBank+0x6000);	// 0xa000 - 0xbfff
		}
		setBank4kB(0,  unmapped);	// 0x0000 - 0x3fff
		setBank4kB(1,  unmapped);
		setBank4kB(2,  unmapped);
		setBank4kB(3,  unmapped);
		setBank4kB(12, unmapped);	// 0xc000 - 0xffff
		setBank4kB(13, unmapped);
		setBank4kB(14, unmapped);
		setBank4kB(15, unmapped);
	} else {
		// this is a simple gamerom less then 64 kB
		byte* ptr;
		switch (romSize >> 14) { // blocks of 16kB
		case 0:	// An 8 kB game ????
			for (int i=0; i<8; i++) {
				setBank8kB(i, memoryBank);
			}
			break;
		case 1:	// 16kB
			for (int i=0; i<8; i+=2) {
				setBank8kB(i,   memoryBank+0x0000);
				setBank8kB(i+1, memoryBank+0x2000);
			}
			break;
		case 2:	// 32kB
			setBank8kB(0, memoryBank+0x0000);	// 0x0000 - 0x1fff
			setBank8kB(1, memoryBank+0x2000);	// 0x2000 - 0x3fff
			setBank8kB(2, memoryBank+0x0000);	// 0x4000 - 0x5fff
			setBank8kB(3, memoryBank+0x2000);	// 0x6000 - 0x7fff
			setBank8kB(4, memoryBank+0x4000);	// 0x8000 - 0x9fff
			setBank8kB(5, memoryBank+0x6000);	// 0xa000 - 0xbfff
			setBank8kB(6, memoryBank+0x4000);	// 0xc000 - 0xdfff
			setBank8kB(7, memoryBank+0x6000);	// 0xe000 - 0xffff
			break;
		case 3:	// 48kB, is this possible?
		case 4:	// 64kB
			ptr = memoryBank;
			for (int i=0; i<8; i++) {
				setBank8kB(i, ptr);
				ptr += 0x2000;
			}
			break;
		default:
			// not possible
			assert (false);
		}
	}
}

struct ltstr {
	bool operator()(const std::string s1, const std::string s2) const {
		return strcasecmp(s1.c_str(), s2.c_str()) < 0;
	}
};

void MSXGameCartridge::retrieveMapperType()
{
	try {
		if (deviceConfig->getParameterAsBool("automappertype")) {
			mapperType = guessMapperType();
		} else {
			std::string  type = deviceConfig->getParameter("mappertype");

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
			mapperType = mappertype[type];
		}
	} catch (MSXConfig::Exception& e) {
		// missing parameter
		mapperType = guessMapperType();
	}
	
	if (mapperType == 17) {
		SRAMEnableBit = 0x20;
	} else if (mapperType == 18) {
		SRAMEnableBit=0x80;
	}
	PRT_DEBUG("mapperType: " << mapperType);
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
					typeGuess[3]++;
					break;
				case 0x8000:
				case 0xA000:
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
				case 0x77FF:
					typeGuess[5]++;
					break;
				default:
					if (value > 0xB000 && value < 0xC000) typeGuess[6]++;
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
			PRT_DEBUG("MSXGameCartridge: typeGuess["<<i<<"]="<<typeGuess[i]);
		return type == 6 ? 19 : type;
	}
}

byte MSXGameCartridge::readMem(word address, const EmuTime &time)
{
	//TODO optimize this!!!
	// One way to optimise would be to register an SCC supporting
	// device only if mapperType is 2 and only in 8000..BFFF.
	// That way, there is no SCC overhead in non-SCC pages.
	// If MSXMotherBoard would support hot-plugging of devices,
	// it would be possible to insert an SCC supporting device
	// only when the SCC is enabled.
#ifndef DONT_WANT_SCC
	if (enabledSCC && 0x9800 <= address && address < 0xA000) {
		return cartridgeSCC->readMemInterface(address & 0xFF, time);
	}
#endif
	PRT_DEBUG(std::hex << "GameCartridge read [" << address << "] := " << (int)internalMemoryBank[address>>12][address&0x0fff] << std::dec );
	return internalMemoryBank[address>>12][address&0x0fff];
}

byte* MSXGameCartridge::getReadCacheLine(word start)
{
	// extra check for SRAM: (enabledSRAM && adr2pag[start>>13]&regioSRAM)
	// not needed since SRAM may be cached.
#ifndef DONT_WANT_SCC
	if ( (enabledSRAM && adr2pag[start>>13]&regioSRAM)
	    || (enabledSCC && 0x9800<=start && start<0xA000)) {
		// don't cache SRAM and SCC
		PRT_DEBUG("No caching for " << std::hex << start << std::dec);
		return NULL;
	} else {
#endif
		PRT_DEBUG("Caching for " << std::hex << start << std::dec);
		if (CPU::CACHE_LINE_SIZE <= 0x1000) {
			return &internalMemoryBank[start>>12][start&0x0fff];
		} else {
			return NULL;
		}
#ifndef DONT_WANT_SCC
	}
#endif
}

void MSXGameCartridge::writeMem(word address, byte value, const EmuTime &time)
{
	/*
	cerr << std::hex << "GameCart[" << address << "] := "
		<< (int)value << std::dec << "\n";
	*/
	switch (mapperType) {
	byte region;
	case 0:
		//--==>> Generic 8kB cartridges <<==--
		// Unlike fMSX, our generic 8kB mapper does not have an SCC.
		// After all, such hardware does not exist in reality.
		// If you want an SCC, use the Konami5 mapper type.

		if (address<0x4000 || address>=0xC000) return;
		// change internal mapper
		value &= mapperMask;
		setBank8kB(address>>13, memoryBank+(value<<13));
		break;
	case 1:
		//--==**>> Generic 16kB cartridges (MSXDOS2, Hole in one special) <<**==--
		if (address<0x4000 || address>=0xC000)
			return;
		region = (address&0xC000)>>13;	// 0, 2, 4, 6
		value = (2*value)&mapperMask;
		setBank8kB(region,   memoryBank+(value<<13));
		setBank8kB(region+1, memoryBank+(value<<13)+0x2000);
		break;
	case 2:
		//--==**>> KONAMI5 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do have an SCC and some others
		// examples of cartridges: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow
		// Solid Snake, Quarth, Ashguine 1, Animal, Arkanoid 2, ...
		//
		// The address to change banks:
		//  bank 1: 0x5000 - 0x57FF (0x5000 used)
		//  bank 2: 0x7000 - 0x77FF (0x7000 used)
		//  bank 3: 0x9000 - 0x97FF (0x9000 used)
		//  bank 4: 0xB000 - 0xB7FF (0xB000 used)

		if (address<0x5000 || address>=0xC000) return;
#ifndef DONT_WANT_SCC
		// Write to SCC?
		if (enabledSCC && 0x9800<=address && address<0xA000) {
			cartridgeSCC->writeMemInterface(address & 0xFF, value , time);
			PRT_DEBUG("GameCartridge: writeMemInterface (" << address <<","<<(int)value<<")"<<time );
			// No page selection in this memory range.
			return;
		}
		// SCC enable/disable?
		if ((address & 0xF800) == 0x9000) {
			enabledSCC = ((value&0x3F)==0x3F);
			MSXCPU::instance()->invalidateCache(0x9800, 0x0800/CPU::CACHE_LINE_SIZE);
		}
#endif
		// Page selection?
		if ((address & 0x1800) != 0x1000) return;
		value &= mapperMask;
		setBank8kB(address>>13, memoryBank+(value<<13));
		break;
	case 3:
		//--==**>> KONAMI4 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do not have an SCC and some others
		// example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
		// The Maze of Galious, Aleste 1, 1942,Heaven, Mystery World, ...
		//
		// page at 4000 is fixed, other banks are switched
		// by writting at 0x6000,0x8000 and 0xA000

		if (address&0x1FFF || address<0x6000 || address>=0xC000) return;
		value &= mapperMask;
		setBank8kB(address>>13, memoryBank+(value<<13));
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

		if (address<0x6000 || address>=0x8000) return;
		region = ((address>>11)&3)+2;
		value &= mapperMask;
		setBank8kB(region, memoryBank+(value<<13));
		break;
	case 5:
		//--==**>> ASCII 16kB cartridges <<**==--
		// this type is used in a few cartridges.
		// example of cartridges: Xevious, Fantasy Zone 2,
		// Return of Ishitar, Androgynus, Gallforce ...
		//
		// The address to change banks:
		//  first  16kb: 0x6000 - 0x67FF (0x6000 used)
		//  second 16kb: 0x7000 - 0x77FF (0x7000 and 0x77FF used)

		if (address<0x6000 || address>=0x7800 || (address&0x800)) return;
		region = ((address>>11)&2)+2;
		value = (2*value)&mapperMask;
		setBank8kB(region,   memoryBank+(value<<13));
		setBank8kB(region+1, memoryBank+(value<<13)+0x2000);
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
		//  first  16kb: 0x6000 - 0x67FF (0x6000 used)
		//  second 16kb: 0x7000 - 0x77FF (0x7000 and 0x77FF used)

		if (address<0x6000 || address>=0x7800 || (address&0x800)){
			//Normal ASCII16 would return but maybe 
			//we are writting to the SRAM?
			if (adr2pag[address>>13]&regioSRAM&0x0C){ 
				for (word adr=address&0x7ff; adr<0x2000; adr+=0x800)
					memorySRAM[adr]=value;
			}
		} else {
			region = ((address>>11)&2)+2;
			if (value == 0x10){
				// SRAM block
				setBank8kB(region,   memorySRAM);
				setBank8kB(region+1, memorySRAM);
				regioSRAM|=(region==2?0x03:0x0c);
			} else {
				// Normal 16 kB ROM page
				value = (2*value)&mapperMask;
				setBank8kB(region,   memoryBank+(value<<13));
				setBank8kB(region+1, memoryBank+(value<<13)+0x2000);
				regioSRAM&=(region==2?0xFD:0xF4);
			}
		};
		break;
	case 17:
	case 18:
		//--==**>> ASCII 8kB cartridges with 8kB SRAM<<**==--
		// this type is used in Xanadu and Royal Blood
		//
		// The address to change banks:
		//  bank 1: 0x6000 - 0x67FF (0x5000 used)
		//  bank 2: 0x6800 - 0x6FFF (0x7000 used)
		//  bank 3: 0x7000 - 0x77FF (0x9000 used)
		//  bank 4: 0x7800 - 0x7FFF (0xB000 used)

		if (address<0x6000 || address>=0x8000){
			//Normal ASCII8 would return but maybe 
			//we are writting to the SRAM?
			PRT_DEBUG(std::hex << "regioSRAM "<< (int)regioSRAM << std::dec << std::hex << "adr2pag[address>>13] "<< (int)adr2pag[address>>13] << std::dec );
			if (adr2pag[address>>13]&regioSRAM&0x0C){ 
				memorySRAM[address & 0x1fff]=value;
				PRT_DEBUG(std::hex << "SRAM write  [" << address << "] := " << (int)value << std::dec );
			}
		} else {
			region = ((address>>11)&3)+2;
			PRT_DEBUG(std::hex << "write  GameCart[" << address << "] := " << (int)value << std::dec);
			if (value & SRAMEnableBit){
				//bit 7 for Royal Blood
				//bit 5 for Xanadu
				setBank8kB(region, memorySRAM);
				regioSRAM|=adr2pag[region];
				PRT_DEBUG("SRAM mapped in (region "<<(int)region<<"): regioSRAM="<<(int)regioSRAM<<"\n");
			} else {
				value &= mapperMask;
				setBank8kB(region, memoryBank+(value<<13));
				regioSRAM&=(~adr2pag[region]);
			}
		};
		break;
	case 19:
		// Konami Game Master 2, 8KB cartridge with 8KB sram
		if (address<0x6000 || address>=0xC000) {
			PRT_DEBUG ("GM2 Discarded write: out of range:"<<std::hex<<address<<", val="<<(int)value<<std::dec<<std::endl);
			return;
		}
		if (address >= 0xB000) {
			// SRAM write
			if (!(regioSRAM & 8)) {	// not in writable page
				PRT_DEBUG ("GM2 Discarded write of "<<std::hex<<(int)value<<" to "<<address<<": SRAM not in correct page, regioSRAM="<<(int)regioSRAM<<std::dec<<std::endl);
				return; 
			}
//			PRT_DEBUG (std::hex<<"GM2 SRAM write to addr "<<address<<", val "<<(int)value<<std::dec<<std::endl);
			memorySRAM[address & 0xFFF + 0x1000 * !!(pageSRAM & (1<<5))] = value;
			break;
		}
		if (address & 0x1000) {
			PRT_DEBUG ("Discarded mapping: illegal address "<<std::hex<<address<<", val="<<value<<std::dec<<std::endl);
			return;
		}
		if (value & (1 << 4)) {
			// switch sram in page
			regioSRAM |= 1 << ((address >> 13) - 2);
			int page = value & (1 << 5);
			if (page) {
				pageSRAM |= 1 << (address >> 13);
				setBank4kB((address>>12)&0xe,     memorySRAM + 0x1000);
				setBank4kB(((address>>12)&0xe)+1, memorySRAM + 0x1000);
			} else {
				pageSRAM &= ~(1 << (address >> 13));
				setBank4kB((address>>12)&0xe,     memorySRAM);
				setBank4kB(((address>>12)&0xe)+1, memorySRAM);
			}
//			PRT_DEBUG (std::hex<<"GM2 Switched SRAM page="<<page<<", value="<<(int)value<<", addr="<<address<<std::dec<<std::endl);
		} else {
			// switch normal memory
			regioSRAM &= ~(1 << ((address >> 13) - 2));
			value &= mapperMask;
			setBank8kB(address>>13, memoryBank+(value<<13));
//			PRT_DEBUG (std::hex<<"GM2 Switched bank="<<(address>>13)<<", value="<<(int)value<<", addr="<<address<<std::dec<<std::endl);
		}
		break;
	case 64:
		//--==**>> <<**==--
		// Konami Synthezier cartridge
		// Should be only for 0x4000, but since this isn't confirmed on a real
		// cratridge we will simply use every write.
		dac->writeDAC(value,time);
		// fall through:
	case 128:
		//--==**>> Simple romcartridge <= 64 KB <<**==--
		// No extra writable hardware
		break;
	default:
		//// Unknow mapper type for GameCartridge cartridge!!!
		assert(false);
	}
}

void MSXGameCartridge::setBank8kB(int region, byte* value)
{
	setBank4kB(region*2,   value);
	setBank4kB(region*2+1, value + 0x1000);
}

void MSXGameCartridge::setBank4kB(int region, byte* value)
{
	internalMemoryBank[region] = value;
	MSXCPU::instance()->invalidateCache(region*0x1000, 0x1000/CPU::CACHE_LINE_SIZE);
}

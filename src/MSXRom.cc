// $Id$

#include "MSXConfig.hh"
#include "MSXRom.hh"
#include "SCC.hh"
#include "DACSound8U.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include "libxmlx/xmlx.hh"
#include "CartridgeSlotManager.hh"
#include "SRAM.hh"
#include "FileContext.hh"

// TODO fix PANASONIC
// TODO NATIONAL seems to work, but needs some more testing


MSXRomCLI msxRomCLI;

MSXRomCLI::MSXRomCLI()
{
	CommandLineParser::instance()->registerOption("-cart", this);
	CommandLineParser::instance()->registerOption("-carta", this);
	CommandLineParser::instance()->registerOption("-cartb", this);
	CommandLineParser::instance()->registerOption("-cartc", this);
	CommandLineParser::instance()->registerOption("-cartd", this);
	CommandLineParser::instance()->registerFileType("rom", this);
}

void MSXRomCLI::parseOption(const std::string &option,
                         std::list<std::string> &cmdLine)
{
	std::string arg = cmdLine.front();
	cmdLine.pop_front();
	if (option.length() == 6) {
		int slot = option[5] - 'a';
		CartridgeSlotManager::instance()->reserveSlot(slot);
		CommandLineParser::instance()->registerPostConfig(new MSXRomPostName(slot, arg));
	} else {
		CommandLineParser::instance()->registerPostConfig(new MSXRomPostNoName(arg));
	}
}
const std::string& MSXRomCLI::optionHelp() const
{
	static const std::string text("Insert the ROM file (cartridge) specified in argument");
	return text;
}
MSXRomPostName::MSXRomPostName(int slot_, const std::string &arg_)
	: MSXRomCLIPost(arg_), slot(slot_)
{
}
void MSXRomPostName::execute(MSXConfig *config)
{
	CartridgeSlotManager::instance()->getSlot(slot, ps, ss);
	MSXRomCLIPost::execute(config);
}

void MSXRomCLI::parseFileType(const std::string &arg)
{
	CommandLineParser::instance()->registerPostConfig(new MSXRomPostNoName(arg));
}
const std::string& MSXRomCLI::fileTypeHelp() const
{
	static const std::string text("ROM image of a cartridge");
	return text;
}
MSXRomPostNoName::MSXRomPostNoName(const std::string &arg_)
	: MSXRomCLIPost(arg_)
{
}
void MSXRomPostNoName::execute(MSXConfig *config)
{
	CartridgeSlotManager::instance()->getSlot(ps, ss);
	MSXRomCLIPost::execute(config);
}

MSXRomCLIPost::MSXRomCLIPost(const std::string &arg_)
	: arg(arg_)
{
}
void MSXRomCLIPost::execute(MSXConfig *config)
{
	std::string filename, mapper;
	int pos = arg.find_last_of(',');
	int pos2 = arg.find_last_of('.');
	if ((pos != -1) && (pos > pos2)) {
		filename = arg.substr(0, pos);
		mapper = arg.substr(pos + 1);
	} else {
		filename = arg;
		mapper = "auto";
	}
	
	XML::Escape(filename);
	std::ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << "<device id=\"MSXRom"<<ps<<"-"<<ss<<"\">";
	s << "<type>Rom</type>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>0</page></slotted>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>1</page></slotted>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>2</page></slotted>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>3</page></slotted>";
	s << "<parameter name=\"filename\">"<<filename<<"</parameter>";
	s << "<parameter name=\"filesize\">auto</parameter>";
	s << "<parameter name=\"volume\">9000</parameter>";
	s << "<parameter name=\"mappertype\">"<<mapper<<"</parameter>";
	s << "<parameter name=\"loadsram\">true</parameter>";
	s << "<parameter name=\"savesram\">true</parameter>";
	s << "<parameter name=\"sramname\">"<<filename<<".SRAM</parameter>";
	s << "</device>";
	s << "</msxconfig>";
	config->loadStream(new UserFileContext(), s);
	delete this;
}


MSXRom::MSXRom(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time), rom(config, time)
{
	retrieveMapperType();

	// only instantiate SRAM if needed
	if (mapperType & HAS_SRAM) {
		switch (mapperType) {
			case NATIONAL:
				sram = new SRAM(0x1000, config);
				break;
			default:
				sram = new SRAM(0x2000, config);
				break;
		}
	} else {
		sram = NULL;
	}

	// only instantiate SCC if needed
	if (mapperType == KONAMI5) {
		short volume = (short)config->getParameterAsInt("volume");
		cartridgeSCC = new SCC(volume);
	} else {
		cartridgeSCC = NULL;
	}

	// only instantiate DACSound if needed
	if (mapperType & HAS_DAC) {
		short volume = (short)config->getParameterAsInt("volume");
		dac = new DACSound8U(volume, time);
	} else {
		dac = NULL;
	}

	// to emulate non-present memory
	unmapped = new byte[0x4000];
	memset(unmapped, 255, 0x4000);
	
	reset(time);
}

void MSXRom::retrieveMapperType()
{
	std::string type;
	if (deviceConfig->hasParameter("mappertype")) {
		type = deviceConfig->getParameter("mappertype");
	} else {
		// no type specified, perform auto detection
		type = "auto";
	}
	if (type == "auto") {
		// automatically detect type
		try {
			// first look in database
			mapperType = RomTypes::searchDataBase(rom.getBlock(), rom.getSize());
		} catch (NotInDataBaseException &e) {
			// not in database, try to guess
			mapperType = RomTypes::guessMapperType(rom.getBlock(), rom.getSize());
		}
	} else {
		// explicitly specified type
		mapperType = RomTypes::nameToMapperType(type);
	}
	PRT_DEBUG("MapperType: " << mapperType);
}


MSXRom::~MSXRom()
{
	delete dac;
	delete cartridgeSCC;
	delete sram;
	delete[] unmapped;
}

void MSXRom::reset(const EmuTime &time)
{
	if (cartridgeSCC)
		cartridgeSCC->reset();
	enabledSCC = false;
	if (dac)
		dac->reset(time);
	
	// After a reset SRAM is not selected in all known cartrdiges
	regioSRAM = 0;

	switch (mapperType) {
	case PLAIN:
		// this is a simple gamerom less or equal then 64 kB
		switch (rom.getSize() >> 14) { // blocks of 16kB
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
				if (guessLocation() & 0x4000) {
					setROM16kB(0, 0);
					setROM16kB(1, 0);
					setROM16kB(2, 1);
					setROM16kB(3, 1);
				} else {
					setROM16kB(0, 0);
					setROM16kB(1, 1);
					setROM16kB(2, 0);
					setROM16kB(3, 1);
				}
				break;
			case 3:	// 48kB
				if (guessLocation() & 0x4000) {
					setROM16kB(0, 0);
					setROM16kB(1, 0);
					setROM16kB(2, 1);
					setROM16kB(3, 2);
				} else {
					setROM16kB(0, 0);
					setROM16kB(1, 1);
					setROM16kB(2, 2);
					setROM16kB(3, 2);
				}
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
		break;

	             case PAGE0:   case PAGE1:   case PAGE01:
	case PAGE2:  case PAGE02:  case PAGE12:  case PAGE012:
	case PAGE3:  case PAGE03:  case PAGE13:  case PAGE013:
	case PAGE23: case PAGE023: case PAGE123: case PAGE0123: {
		int bank = 0;
		for (int page = 0; page < 4; page++) {
			if (mapperType & (1 << page)) {
				setROM16kB(page, bank++);
			} else {
				setBank16kB(page, unmapped);
			}
		}
		mapperType = PLAIN;
		break;
	}
	case PANASONIC:
		control = 0;
		for (int region = 0; region < 8; region++) {
			bankSelect[region] = 0;
			setROM8kB(region, 0);
		}
		break;
	
	case NATIONAL:
		control = 0;
		for (int region = 0; region < 4; region++) {
			bankSelect[region] = 0;
			setROM16kB(region, 0);
		}
		break;
	
	default:
		setBank16kB(0, unmapped);
		setROM16kB (1, (mapperType == R_TYPE) ? 0x17 : 0);
		setROM16kB (2, (mapperType == HYDLIDE2 || mapperType == ASCII_16KB) ? 0 : 1);
		setBank16kB(3, unmapped);
	}
}



void MSXRom::guessHelper(word offset, int* pages)
{
	if ((rom.read(offset++) == 'A') && (rom.read(offset++) =='B')) {
		for (int i = 0; i < 4; i++) {
			word addr = rom.read(offset++) +
			            rom.read(offset++) * 256;
			if (addr) {
				int page = (addr >> 14) - (offset >> 14);
				if ((0 <= page) && (page <= 2)) {
					pages[page]++;
				}
			}
		}
	}
}

word MSXRom::guessLocation()
{
	int pages[3] = { 0, 0, 0 };

	// count number of possible routine pointers
	if (rom.getSize() >= 0x0010) {
		guessHelper(0x0000, pages);
	}
	if (rom.getSize() >= 0x4010) {
		guessHelper(0x4000, pages);
	}
	PRT_DEBUG("MSXRom: location " << pages[0] << 
	                          " " << pages[1] <<
				  " " << pages[2]);
	// we prefer 0x4000, then 0x000 and then 0x8000
	if (pages[1] && (pages[1] >= pages[0]) && (pages[1] >= pages[2])) {
		return 0x4000;
	} else if (pages[0] && pages[0] >= pages[2]) {
		return 0x0000;
	} else if (pages[2]) {
		return 0x8000;
	}

	int lowest = 4;
	std::list<Device::Slotted*>::const_iterator i;
	for (i =  deviceConfig->slotted.begin(); 
	     i != deviceConfig->slotted.end();
	     i++) {
		int page = (*i)->getPage();
		if (page < lowest) lowest = page;
	}
	return (lowest * 0x4000) & 0xFFFF;
}

byte MSXRom::readMem(word address, const EmuTime &time)
{
	//TODO optimize this (Necessary? We have read cache now)
	// One way to optimise would be to register an SCC supporting
	// device only if mapperType is 2 and only in 8000..BFFF.
	// That way, there is no SCC overhead in non-SCC pages.
	// If MSXCPUInterface would support hot-plugging of devices,
	// it would be possible to insert an SCC supporting device
	// only when the SCC is enabled.
	if (enabledSCC && (0x9800 <= address) && (address < 0xA000)) {
		return cartridgeSCC->readMemInterface(address&0xff, time);
	}
	switch (mapperType) {
	case PANASONIC:
		if ((control & 0x04) && (0x7FF0 <= address) && (address < 0x7FF7)) {
			// read mapper state (lower 8 bit)
			return bankSelect[address & 7] & 0xFF;
		}
		if ((control & 0x10) && (address == 0x7FF8)) {
			// read mapper state (9th bit)
			byte res = 0;
			for (int i = 7; i >= 0; i--) {
				if (bankSelect[i] & 0x100) {
					res++;
				}
				res <<= 1;
			}
			return res;
		}
		if ((control & 0x08) && (address == 0x7FF9)) {
			// read control byte
			return control;
		}
		break;
		
	case NATIONAL:
		if ((control & 0x04) && ((address & 0x7FF9) == 0x7FF0)) {
			// TODO check mirrored
			// 7FF0 7FF2 7FF4 7FF6   bank select read back
			int bank = (address & 6) / 2;
			return bankSelect[bank];
		}
		if ((control & 0x02) && ((address & 0x3FFF) == 0x3FFD)) {
			// SRAM read
			return sram->read(sramAddr++ & 0x0FFF);
		}
		break;
	
	default:
		// do nothing
		break;
	}
	return internalMemoryBank[address>>12][address&0x0FFF];
}

const byte* MSXRom::getReadCacheLine(word start) const
{
	if (enabledSCC && (0x9800 <= start) && (start < 0xA000)) {
		// don't cache SCC
		return NULL;
	}
	
	switch (mapperType) {
	case PANASONIC:
		if ((0x7FF0 & CPU::CACHE_LINE_HIGH) == start) {
			// TODO check mirrored 
			return NULL;
		}
		break;
	
	case NATIONAL:
		if ((0x3FF0 & CPU::CACHE_LINE_HIGH) == (start & 0x3FFF)) {
			return NULL;
		}
		break;

	default:
		// do nothing
		break;
	}
	
	if (CPU::CACHE_LINE_SIZE <= 0x1000) {
		return &internalMemoryBank[start>>12][start&0x0FFF];
	} else {
		return NULL;
	}
}

void MSXRom::writeMem(word address, byte value, const EmuTime &time)
{
	switch (mapperType) {
	case GENERIC_8KB:
		//--==**>> Generic 8kB cartridges <<**==--
		if ((0x4000 <= address) && (address < 0xC000)) {
			setROM8kB(address >> 13, value);
		}
		break;
		
	case GENERIC_16KB:
		//--==**>> Generic 16kB cartridges <<**==--
		// examples: MSXDOS2, Hole in one special
		
		if ((0x4000 <= address) && (address < 0xC000)) {
			byte region = (address & 0xC000) >> 14;	// 0..3
			setROM16kB(region, value);
		}
		break;
	
	case KONAMI5:
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

		if ((address < 0x5000) || (address >= 0xC000)) break;
		// Write to SCC?
		if (enabledSCC && (0x9800 <= address) && (address < 0xA000)) {
			cartridgeSCC->writeMemInterface(address & 0xFF, value, time);
			// No page selection in this memory range.
			break;
		}
		// SCC enable/disable?
		if ((address & 0xF800) == 0x9000) {
			enabledSCC = ((value & 0x3F) == 0x3F);
			MSXCPU::instance()->invalidateCache(0x9800, 0x0800/CPU::CACHE_LINE_SIZE);
		}
		// Page selection?
		if ((address & 0x1800) == 0x1000)
			setROM8kB(address >> 13, value);
		break;
	
	case KONAMI4:
		//--==**>> KONAMI4 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do not have an SCC and some others
		// example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
		// The Maze of Galious, Aleste 1, 1942,Heaven, Mystery World, ...
		//
		// page at 4000 is fixed, other banks are switched
		// by writting at 0x6000,0x8000 and 0xa000

		if (address==0x6000 || address==0x8000 || address==0xA000)
			setROM8kB(address>>13, value);
		break;
		
	case ASCII_8KB:
		//--==**>> ASCII 8kB cartridges <<**==--
		// this type is used in many japanese-only cartridges.
		// example of cartridges: Valis(Fantasm Soldier), Dragon Slayer, Outrun, 
		//                        Ashguine 2, ...
		// The address to change banks:
		//  bank 1: 0x6000 - 0x67ff (0x6000 used) 
		//  bank 2: 0x6800 - 0x6fff (0x6800 used)
		//  bank 3: 0x7000 - 0x77ff (0x7000 used)
		//  bank 4: 0x7800 - 0x7fff (0x7800 used)

		if ((0x6000 <= address) && (address < 0x8000)) {
			byte region = ((address >> 11) & 3) + 2;
			setROM8kB(region, value);
		}
		break;
		
	case ASCII_16KB:
		//--==**>> ASCII 16kB cartridges <<**==--
		// this type is used in a few cartridges.
		// example of cartridges: Xevious, Fantasy Zone 2,
		// Return of Ishitar, Androgynus, Gallforce ...
		//
		// The address to change banks:
		//  first  16kb: 0x6000 - 0x67ff (0x6000 used)
		//  second 16kb: 0x7000 - 0x77ff (0x7000 and 0x77ff used)
		
		if (0x6000<=address && address<0x7800 && !(address&0x0800)) {
			byte region = ((address >> 12) & 1) + 1;
			setROM16kB(region, value);
		}
		break;
		
	case R_TYPE:
		//--==**>> R-Type cartridges <<**==--
		//
		// The address to change banks:
		//  first  16kb: fixed at 0x0f or 0x17
		//  second 16kb: 0x7000 - 0x7FFF (0x7000 and 0x7800 used)
		//               bit 4 selects ROM chip,
		//                if low  bit 3-0 select page
		//                   high     2-0 

		if ((0x7000 <= address) && (address < 0x8000)) {
			value &= (value & 0x10) ? 0x17 : 0x1F;
			setROM16kB(2, value);
		}
		break;

	case PLAIN:
		//--==**>> Simple romcartridge <= 64 KB <<**==--
		// No extra writable hardware
		break;
		
		
	case HYDLIDE2:
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

		if (0x6000<=address && address<0x7800 && !(address&0x0800)) {
			byte region = ((address >> 12) & 1) + 1;
			if (value == 0x10) {
				// SRAM block
				setBank8kB(2 * region,     sram->getBlock());
				setBank8kB(2 * region + 1, sram->getBlock());
				regioSRAM |= (region==1 ?  0x0C :  0x30);
			} else {
				// Normal 16 kB ROM page
				setROM16kB(region, value);
				regioSRAM &= (region==1 ? ~0x0C : ~0x30);
			}
		} else {
			// Writting to SRAM?
			if ((1 << (address>>13)) & regioSRAM & 0x0C) {
				// TODO use real mirroring instead
				for (word adr = address & 0x7FF; adr < 0x2000; adr += 0x800) {
					sram->write(adr, value);
				}
			}
		}
		break;
		
	case ASCII8_8:
		//--==**>> ASCII 8kB cartridges with 8kB SRAM<<**==--
		// this type is used in Xanadu and Royal Blood
		//
		// The address to change banks:
		//  bank 1: 0x6000 - 0x67ff (0x6000 used)
		//  bank 2: 0x6800 - 0x6fff (0x6800 used)
		//  bank 3: 0x7000 - 0x77ff (0x7000 used)
		//  bank 4: 0x7800 - 0x7fff (0x7800 used)
		//
		//  To select SRAM set bit 5/6/7 (depends on size) of the bank.
		//  The SRAM can only be written to if selected in bank 3 or 4.

		if ((0x6000 <= address) && (address < 0x8000)) {
			byte region = ((address >> 11) & 3) + 2;
			byte SRAMEnableBit = rom.getSize() / 8192;
			if (value & SRAMEnableBit) {
				setBank8kB(region, sram->getBlock());
				regioSRAM |=  (1 << region);
			} else {
				setROM8kB(region, value);
				regioSRAM &= ~(1 << region);
			}
		} else {
			// Writting to SRAM?
			if ((1 << (address>>13)) & regioSRAM & 0x30) { 
				// 0x8000 - 0xBFFF
				sram->write(address & 0x1FFF, value);
			}
		}
		break;
		
	case GAME_MASTER2:
		// Konami Game Master 2, 8KB cartridge with 8KB sram
		if ((0x6000 <= address) && (address < 0xC000)) {
			if (address >= 0xB000) {
				// SRAM write
				if (regioSRAM & 0x20)	// 0xA000 - 0xBFFF
					internalMemoryBank[0xB][address&0x0FFF] = value;
			}
			if (!(address & 0x1000)) {
				byte region = address >> 13;	// 0..7
				if (value & 0x10) {
					// switch sram in page
					regioSRAM |=  (1 << region);
					if (value & 0x20) {
						setBank4kB(2*region,   sram->getBlock(0x1000));
						setBank4kB(2*region+1, sram->getBlock(0x1000));
					} else {
						setBank4kB(2*region,   sram->getBlock(0x0000));
						setBank4kB(2*region+1, sram->getBlock(0x0000));
					}
				} else {
					// switch normal memory
					regioSRAM &= ~(1 << region);
					setROM8kB(region, value);
				}
			}
		}
		break;
	
	case SYNTHESIZER:
		// Konami Synthesizer
		if (address == 0x4000) {
			dac->writeDAC(value,time);
		}
		break;

	case MAJUTSUSHI:
		// Konami Majutsushi
		if ((0x6000 <= address) && (address < 0xC000)) {
			setROM8kB(address >> 13, value);
		} else if ((0x5000 <= address) && (address < 0x6000)) {
			dac->writeDAC(value, time);
		}
		break;
	
	case CROSS_BLAIM:
		// Cross Blaim
		if (address == 0x4045) {
			setROM16kB(2, value);
		}
		break;

	case PANASONIC:
		// Panasonic mapper (for example used in Turbo-R)
		if ((0x6000 <= address) && (address < 0x7FF0)) {
			// set mapper state (lower 8 bits)
			int region = (address & 0x1C00) >> 10;
			if ((region == 5) || (region == 6)) region ^= 3;
			int bank = bankSelect[region];
			int newBank = (bank & ~0xFF) | value;
			if (newBank != bank) {
				bankSelect[region] = newBank;
				setROM8kB(region, newBank);
			}
		} else if (address == 0x7FF8) {
			// set mapper state (9th bit)
			for (int region = 0; region < 8; region++) {
				if (value & 1) {
					if (!(bankSelect[region] & 0x100)) {
						bankSelect[region] |= 0x100;
						setROM8kB(region, bankSelect[region]);
					}
				} else {
					if (bankSelect[region] & 0x100) {
						bankSelect[region] &= ~0x100;
						setROM8kB(region, bankSelect[region]);
					}
				}
				value >>= 1;
			}
		} else if (address == 0x7FF9) {
			// write control byte
			control = value;
		} else if ((0x8000 <= address) && (address < 0xC000)) {
			// SRAM TODO check
			int region = (address & 0x1C00) >> 10;
			if ((0x80 <= region) && (region < 0x84)) {
				PRT_DEBUG("PANASONIC: SRAM");
				internalMemoryBank[address>>12][address&0x0FFF] = value;
			}
		}
		break;

	case NATIONAL:
		// National 16KB mapper (for example used in FS-4600F)
		// TODO bank switch address mirrored?
		if (address == 0x6000) {
			bankSelect[1] = value;
			setROM16kB(1, value);
		} else if (address == 0x6400) {
			bankSelect[0] = value;
			setROM16kB(0, value);
		} else if (address == 0x7000) {
			bankSelect[2] = value;
			setROM16kB(2, value);
		} else if (address == 0x7400) {
			bankSelect[3] = value;
			setROM16kB(3, value);
		} else if (address == 0x7FF9) {
			// write control byte
			control = value;
		} else if (control & 0x02) {
			address &= 0x3FFF;
			if (address == 0x3FFA) {
				// SRAM address bits 23-16
				sramAddr = (sramAddr & 0x00FFFF) | value << 16;
			} else if (address == 0x3FFB) {
				// SRAM address bits 15-8
				sramAddr = (sramAddr & 0xFF00FF) | value << 8;
			} else if (address == 0x3FFC) {
				// SRAM address bits 7-0
				sramAddr = (sramAddr & 0xFFFF00) | value;
			} else if (address == 0x3FFD) {
				sram->write(sramAddr++ & 0x0FFF, value);
			}
		}
		break;

	default:
		// Unknown mapper type
		assert(false);
	}
}

void MSXRom::setBank4kB(int region, byte* adr)
{
	internalMemoryBank[region] = adr;
	MSXCPU::instance()->invalidateCache(region * 0x1000,
	                                    0x1000 / CPU::CACHE_LINE_SIZE);
}
void MSXRom::setBank8kB(int region, byte* adr)
{
	internalMemoryBank[2 * region + 0] = adr + 0x0000;
	internalMemoryBank[2 * region + 1] = adr + 0x1000;
	MSXCPU::instance()->invalidateCache(region * 0x2000,
	                                    0x2000 / CPU::CACHE_LINE_SIZE);
}
void MSXRom::setBank16kB(int region, byte* adr)
{
	internalMemoryBank[4 * region + 0] = adr + 0x0000;
	internalMemoryBank[4 * region + 1] = adr + 0x1000;
	internalMemoryBank[4 * region + 2] = adr + 0x2000;
	internalMemoryBank[4 * region + 3] = adr + 0x3000;
	MSXCPU::instance()->invalidateCache(region * 0x4000,
	                                    0x4000 / CPU::CACHE_LINE_SIZE);
}

void MSXRom::setROM4kB(int region, int block)
{
	int nrBlocks = rom.getSize() >> 12;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank4kB(region, const_cast<byte*>(rom.getBlock(block << 12)));
	} else {
		setBank4kB(region, unmapped);
	}
}
void MSXRom::setROM8kB(int region, int block)
{
	int nrBlocks = rom.getSize() >> 13;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank8kB(region, const_cast<byte*>(rom.getBlock(block << 13)));
	} else {
		setBank8kB(region, unmapped);
	}
}
void MSXRom::setROM16kB(int region, int block)
{
	int nrBlocks = rom.getSize() >> 14;
	if (nrBlocks != 0) {
		block = (block < nrBlocks) ? block : block & (nrBlocks - 1);
		setBank16kB(region, const_cast<byte*>(rom.getBlock(block << 14)));
	} else {
		setBank16kB(region, unmapped);
	}
}

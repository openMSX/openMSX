// $Id$

#include <map>
#include "RomTypes.hh"
#include "md5.hh"
#include "libxmlx/xmlx.hh"
#include "FileContext.hh"
#include "File.hh"


struct caseltstr {
	bool operator()(const std::string s1, const std::string s2) const {
		return strcasecmp(s1.c_str(), s2.c_str()) < 0;
	}
};

MapperType RomTypes::nameToMapperType(const std::string &name)
{
	static std::map<std::string, MapperType, caseltstr> mappertype;
	static bool init = false;

	if (!init) {
		init = true;
		
		mappertype["0"]           = GENERIC_8KB;
		mappertype["8kB"]         = GENERIC_8KB;

		mappertype["1"]           = GENERIC_16KB;
		mappertype["16kB"]        = GENERIC_16KB;
		mappertype["MSXDOS2"]     = GENERIC_16KB; /* For now...*/

		mappertype["2"]           = KONAMI5;
		mappertype["KONAMI5"]     = KONAMI5;
		mappertype["SCC"]         = KONAMI5;

		mappertype["3"]           = KONAMI4;
		mappertype["KONAMI4"]     = KONAMI4;

		mappertype["4"]           = ASCII_8KB;
		mappertype["ASCII8"]      = ASCII_8KB;
 
		mappertype["5"]           = ASCII_16KB;
		mappertype["ASCII16"]     = ASCII_16KB;

		mappertype["RTYPE"]       = R_TYPE;

		mappertype["CROSSBLAIM"]  = CROSS_BLAIM;
		
		mappertype["PANASONIC"]   = PANASONIC;
		
		mappertype["NATIONAL"]    = NATIONAL;
	
		mappertype["MSX-AUDIO"]   = MSX_AUDIO;
		
		mappertype["HARRYFOX"]    = HARRY_FOX;
		
		mappertype["HALNOTE"]     = HALNOTE;

		mappertype["64kB"]        = PLAIN;
		mappertype["PLAIN"]       = PLAIN;
		
		// SRAM
		mappertype["HYDLIDE2"]    = HYDLIDE2;
		mappertype["ASCII16-2"]   = HYDLIDE2;

		// TODO: proper support for ASCII 8kB with a certain
		// amount of SRAM (8 - 32kB)
		mappertype["ASCII8-8"]    = ASCII8_8; 

		mappertype["GAMEMASTER2"] = GAME_MASTER2;
		mappertype["RC755"]       = GAME_MASTER2;

		// DAC
		mappertype["MAJUTSUSHI"]  = MAJUTSUSHI;
		
		mappertype["SYNTHESIZER"] = SYNTHESIZER;
		
		mappertype["PAGE0"]       = PAGE0;
		mappertype["PAGE01"]      = PAGE01;
		mappertype["PAGE012"]     = PAGE012;
		mappertype["PAGE0123"]    = PAGE0123;
		mappertype["PAGE1"]       = PAGE1;
		mappertype["PAGE12"]      = PAGE12;
		mappertype["PAGE123"]     = PAGE123;
		mappertype["PAGE2"]       = PAGE2;
		mappertype["ROMBAS"]      = PAGE2;
		mappertype["PAGE23"]      = PAGE23;
		mappertype["PAGE3"]       = PAGE3;
	}

	std::map<std::string, MapperType, caseltstr>::const_iterator
		it = mappertype.find(name);
	if (it == mappertype.end()) {
		return UNKNOWN;
	}
	return it->second;
}


MapperType RomTypes::guessMapperType(const byte* data, int size)
{
	if (size <= 0x10000) {
		if (size == 0x10000) {
			// There are some programs convert from tape to 64kB rom cartridge
			// these 'fake'roms are from the ASCII16 type
			return ASCII_16KB;
		} else if ((size <= 0x4000) &&
		           (data[0] == 'A') && (data[1] == 'B')) {
			word initAddr = data[2] + 256 * data[3];
			word textAddr = data[8] + 256 * data[9];
			if ((initAddr == 0) && ((textAddr & 0xC000) == 0x8000)) {
				return PAGE2;
			}
		}
		// not correct for Konami-DAC, but does this really need
		// to be correct for _every_ rom?
		return PLAIN;
	} else {
		//  GameCartridges do their bankswitching by using the Z80
		//  instruction ld(nn),a in the middle of program code. The
		//  adress nn depends upon the GameCartridge mappertype used.
		//  To guess which mapper it is, we will look how much writes
		//  with this instruction to the mapper-registers-addresses
		//  occur.

		unsigned int typeGuess[] = {0,0,0,0,0,0};
		for (int i=0; i<size-3; i++) {
			if (data[i] == 0x32) {
				word value = data[i+1]+(data[i+2]<<8);
				switch (value) {
					case 0x5000:
					case 0x9000:
					case 0xb000:
						typeGuess[KONAMI5]++;
						break;
					case 0x4000:
						typeGuess[KONAMI4]++;
						break;
					case 0x8000:
					case 0xa000:
						typeGuess[KONAMI4]++;
						break;
					case 0x6800:
					case 0x7800:
						typeGuess[ASCII_8KB]++;
						break;
					case 0x6000:
						typeGuess[KONAMI4]++;
						typeGuess[ASCII_8KB]++;
						typeGuess[ASCII_16KB]++;
						break;
					case 0x7000:
						typeGuess[KONAMI5]++;
						typeGuess[ASCII_8KB]++;
						typeGuess[ASCII_16KB]++;
						break;
					case 0x77ff:
						typeGuess[ASCII_16KB]++;
						break;
				}
			}
		}
		if (typeGuess[ASCII_8KB]) typeGuess[ASCII_8KB]--; // -1 -> max_int
		MapperType type = GENERIC_8KB; // 0
		for (int i=GENERIC_8KB; i <= ASCII_16KB; i++) {
			if ((typeGuess[i]) && (typeGuess[i]>=typeGuess[type])) {
				type = (MapperType)i;
			}
		}
		// in case of doubt we go for type 0
		// in case of even type 5 and 4 we would prefer 5
		// but we would still prefer 0 above 4 or 5
		if ((type == ASCII_16KB) && (typeGuess[GENERIC_8KB] == typeGuess[ASCII_16KB]))
			type = GENERIC_8KB;
		return type;
	}
}

MapperType RomTypes::searchDataBase(const byte* data, int size)
{
	static std::map<std::string, RomInfo*> romDB;
	static bool init = false;

	if (!init) {
		init = true;
		try {
			SystemFileContext context;
			File file(context.resolve("etc/romdb.xml"));
			XML::Document doc(file.getLocalName().c_str());
			std::list<XML::Element*>::iterator it1 = doc.root->children.begin();
			for ( ; it1 != doc.root->children.end(); it1++) {
				RomInfo* romInfo = new RomInfo();
				romInfo->romType=(*it1)->getElementPcdata("romtype");
				romInfo->id=(*it1)->getAttribute("id");
				romInfo->year=(*it1)->getElementPcdata("year");
				romInfo->company=(*it1)->getElementPcdata("company");
				romInfo->remark=(*it1)->getElementPcdata("remark");
				std::list<XML::Element*>::iterator it2 = (*it1)->children.begin();
				for ( ; it2 != (*it1)->children.end(); it2++) {
					if ((*it2)->name == "md5") {
						if (romDB.find((*it2)->pcdata) == romDB.end()) {
							romDB[(*it2)->pcdata] = romInfo;
						} else {
							PRT_INFO("Warning duplicate romdb entry " << (*it2)->pcdata);
						}
					}
				}
			}
		} catch (MSXException &e) {
			PRT_INFO("Warning couldn't open romdb.xml.\n"
			         "Romtype detection might fail because of this.");
		}
	}
	
	MD5 md5;
	md5.update(data, size);
	md5.finalize();
	std::string digest(md5.hex_digest());
	
	if (romDB.find(digest) != romDB.end()) {
		std::string year(romDB[digest]->year);
		if (year.length()==0) year="(info not available)";
		std::string company(romDB[digest]->company);
		if (company.length()==0) company="(info not available)";
		PRT_INFO("Found this ROM in the database:\n"
				"  Title (id):  " << romDB[digest]->id << "\n"
				"  Year:        " << year << "\n"
				"  Company:     " << company << "\n"
				"  Remark:      " << romDB[digest]->remark << "\n"
				"  Mapper type: " << romDB[digest]->romType);
		return nameToMapperType(romDB[digest]->romType);
	} else {
		// Rom is not in database
		return UNKNOWN;
	}
}

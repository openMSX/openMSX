// $Id$

#include <map>
#include "RomTypes.hh"
#include "md5.hh"
#include "libxmlx/xmlx.hh"


struct caseltstr {
	bool operator()(const std::string s1, const std::string s2) const {
		return strcasecmp(s1.c_str(), s2.c_str()) < 0;
	}
};

int RomTypes::nameToMapperType(const std::string &name)
{
	static std::map<const std::string, int, caseltstr> mappertype;
	static bool init = false;

	if (!init) {
		init = true;
		
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
		
		//Plain ROM
		mappertype["128"]=128;
		mappertype["64kB"]=128;
		mappertype["PLAIN"]=128;
	}

	if (mappertype.find(name) == mappertype.end())
		throw MSXException("Unknown mappertype");
	return mappertype[name];
}


int RomTypes::guessMapperType(byte* data, int size)
{
	//  GameCartridges do their bankswitching by using the Z80
	//  instruction ld(nn),a in the middle of program code. The
	//  adress nn depends upon the GameCartridge mappertype used.
	//  To guess which mapper it is, we will look how much writes
	//  with this instruction to the mapper-registers-addresses
	//  occure.

	if (size <= 0x10000) {
		if (size == 0x10000) {
			// There are some programs convert from tape to 64kB rom cartridge
			// these 'fake'roms are from the ASCII16 type
			return 5;
		} else {
			// not correct for Konami-DAC, but does this really need
			// to be correct for _every_ rom?
			return 128;
		}
	} else {
		unsigned int typeGuess[] = {0,0,0,0,0,0,0};
		for (int i=0; i<size-3; i++) {
			if (data[i] == 0x32) {
				word value = data[i+1]+(data[i+2]<<8);
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
		if ((type == 5) && (typeGuess[0] == typeGuess[5]))
			type = 0;
		return (type == 6) ? 19 : type;
	}
}

int RomTypes::searchDataBase(byte* data, int size)
{
	static std::map<const std::string, std::string, caseltstr> romDB;
	static bool init = false;

	if (!init) {
		init = true;
		XML::Document doc("romdb.xml");
		std::list<XML::Element*>::iterator it = doc.root->children.begin();
		for ( ; it != doc.root->children.end(); it++) {
			const std::string md5((*it)->getElementPcdata("md5"));
			const std::string romtype((*it)->getElementPcdata("romtype"));
			romDB[md5] = romtype;
		}
	}
	
	MD5 md5;
	md5.update(data, size);
	md5.finalize();
	std::string digest(md5.hex_digest());
	
	if (romDB.find(digest) == romDB.end())
		throw NotInDataBaseException("Rom is not in database");
	return nameToMapperType(romDB[digest]);
}

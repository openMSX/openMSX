// $Id$

#include "MSXRomCLI.hh"
#include "RomFactory.hh"
#include "MSXConfig.hh"
#include "RomTypes.hh"
#include "RomPageNN.hh"
#include "RomPlain.hh"
#include "RomGeneric8kB.hh"
#include "RomGeneric16kB.hh"
#include "RomKonami4.hh"
#include "RomKonami5.hh"
#include "RomAscii8kB.hh"
#include "RomAscii8_8.hh"
#include "RomAscii16kB.hh"
#include "RomHydlide2.hh"
#include "RomRType.hh"
#include "RomCrossBlaim.hh"
#include "RomMSXAudio.hh"
#include "RomHarryFox.hh"
#include "RomPanasonic.hh"
#include "RomNational.hh"
#include "RomMajutsushi.hh"
#include "RomSynthesizer.hh"
#include "RomGameMaster2.hh"
#include "Rom.hh"


MSXRomCLI msxRomCLI;


MSXRom* RomFactory::create(Device* config, const EmuTime &time)
{
	MapperType type = retrieveMapperType(config, time);
	switch (type) {
		case PAGE0:
		case PAGE1:
		case PAGE01:
		case PAGE2:
		case PAGE02:
		case PAGE12:
		case PAGE012:
		case PAGE3:
		case PAGE03:
		case PAGE13:
		case PAGE013:
		case PAGE23:
		case PAGE023:
		case PAGE123:
		case PAGE0123:
			return new RomPageNN(config, time, type & 0xf);
		case PLAIN:
			return new RomPlain(config, time);
		case GENERIC_8KB:
			return new RomGeneric8kB(config, time);
		case GENERIC_16KB:
			return new RomGeneric16kB(config, time);
		case KONAMI5:
			return new RomKonami5(config, time);
		case KONAMI4:
			return new RomKonami4(config, time);
		case ASCII_8KB:
			return new RomAscii8kB(config, time);
		case ASCII_16KB:
			return new RomAscii16kB(config, time);
		case R_TYPE:
			return new RomRType(config, time);
		case CROSS_BLAIM:
			return new RomCrossBlaim(config, time);
		case MSX_AUDIO:
			return new RomMSXAudio(config, time);
		case HARRY_FOX:
			return new RomHarryFox(config, time);
		case ASCII8_8:
			return new RomAscii8_8(config, time);
		case HYDLIDE2:
			return new RomHydlide2(config, time);
		case GAME_MASTER2:
			return new RomGameMaster2(config, time);
		case PANASONIC:
			return new RomPanasonic(config, time);
		case NATIONAL:
			return new RomNational(config, time);
		case MAJUTSUSHI:
			return new RomMajutsushi(config, time);
		case SYNTHESIZER:
			return new RomSynthesizer(config, time);
		default:
			PRT_DEBUG("Unknown mapper type");
			return NULL;
	}
}

MapperType RomFactory::retrieveMapperType(Device *config, const EmuTime &time)
{
	std::string type;
	if (config->hasParameter("mappertype")) {
		type = config->getParameter("mappertype");
	} else {
		// no type specified, perform auto detection
		type = "auto";
	}
	MapperType mapperType;
	if (type == "auto") {
		Rom rom(config, time);
		// automatically detect type, first look in database
		mapperType = RomTypes::searchDataBase(rom.getBlock(),
		                                      rom.getSize());
		if (mapperType == UNKNOWN) {
			// not in database, try to guess
			mapperType = RomTypes::guessMapperType(rom.getBlock(),
			                                       rom.getSize());
		}
	} else {
		// explicitly specified type
		mapperType = RomTypes::nameToMapperType(type);
	}
	PRT_DEBUG("MapperType: " << mapperType);
	return mapperType;
}

// $Id$

#include "MSXRomCLI.hh"
#include "RomFactory.hh"
#include "MSXConfig.hh"
#include "RomTypes.hh"
#include "RomInfo.hh"
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
#include "RomHalnote.hh"
#include "RomKorean80in1.hh"
#include "RomKorean90in1.hh"
#include "RomKorean126in1.hh"
#include "Rom.hh"


MSXRomCLI msxRomCLI;


MSXRom *RomFactory::create(Device *config, const EmuTime &time)
{
	Rom *rom = new Rom(config, time);

	MapperType type = rom->getInfo().getMapperType();
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
			return new RomPageNN(config, time, rom, type & 0xf);
		case PLAIN:
			return new RomPlain(config, time, rom);
		case GENERIC_8KB:
			return new RomGeneric8kB(config, time, rom);
		case GENERIC_16KB:
			return new RomGeneric16kB(config, time, rom);
		case KONAMI5:
			return new RomKonami5(config, time, rom);
		case KONAMI4:
			return new RomKonami4(config, time, rom);
		case ASCII_8KB:
			return new RomAscii8kB(config, time, rom);
		case ASCII_16KB:
			return new RomAscii16kB(config, time, rom);
		case R_TYPE:
			return new RomRType(config, time, rom);
		case CROSS_BLAIM:
			return new RomCrossBlaim(config, time, rom);
		case MSX_AUDIO:
			return new RomMSXAudio(config, time, rom);
		case HARRY_FOX:
			return new RomHarryFox(config, time, rom);
		case ASCII8_8:
			return new RomAscii8_8(config, time, rom);
		case HYDLIDE2:
			return new RomHydlide2(config, time, rom);
		case GAME_MASTER2:
			return new RomGameMaster2(config, time, rom);
		case PANASONIC:
			return new RomPanasonic(config, time, rom);
		case NATIONAL:
			return new RomNational(config, time, rom);
		case MAJUTSUSHI:
			return new RomMajutsushi(config, time, rom);
		case SYNTHESIZER:
			return new RomSynthesizer(config, time, rom);
		case HALNOTE:
			return new RomHalnote(config, time, rom);
		case KOREAN80IN1:
			return new RomKorean80in1(config, time, rom);
		case KOREAN90IN1:
			return new RomKorean90in1(config, time, rom);
		case KOREAN126IN1:
			return new RomKorean126in1(config, time, rom);
		default:
			PRT_ERROR("Unknown mapper type");
			return NULL;
	}
}


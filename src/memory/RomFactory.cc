// $Id$

#include "RomFactory.hh"
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
#include "RomHolyQuran.hh"
#include "RomFSA1FM.hh"
#include "Rom.hh"
#include "xmlx.hh"

namespace openmsx {

auto_ptr<MSXDevice> RomFactory::create(const XMLElement& config,
                                       const EmuTime& time)
{
	auto_ptr<Rom> rom(new Rom(config.getId(), "rom", config));

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
			return auto_ptr<MSXDevice>(new RomPageNN(config, time, rom, type & 0xF));
		case PLAIN:
			return auto_ptr<MSXDevice>(new RomPlain(config, time, rom));
		case GENERIC_8KB:
			return auto_ptr<MSXDevice>(new RomGeneric8kB(config, time, rom));
		case GENERIC_16KB:
			return auto_ptr<MSXDevice>(new RomGeneric16kB(config, time, rom));
		case KONAMI5:
			return auto_ptr<MSXDevice>(new RomKonami5(config, time, rom));
		case KONAMI4:
			return auto_ptr<MSXDevice>(new RomKonami4(config, time, rom));
		case ASCII_8KB:
			return auto_ptr<MSXDevice>(new RomAscii8kB(config, time, rom));
		case ASCII_16KB:
			return auto_ptr<MSXDevice>(new RomAscii16kB(config, time, rom));
		case R_TYPE:
			return auto_ptr<MSXDevice>(new RomRType(config, time, rom));
		case CROSS_BLAIM:
			return auto_ptr<MSXDevice>(new RomCrossBlaim(config, time, rom));
		case MSX_AUDIO:
			return auto_ptr<MSXDevice>(new RomMSXAudio(config, time, rom));
		case HARRY_FOX:
			return auto_ptr<MSXDevice>(new RomHarryFox(config, time, rom));
		case ASCII8_8:
			return auto_ptr<MSXDevice>(new RomAscii8_8(config, time, rom,
			                       RomAscii8_8::ASCII8_8));
		case KOEI_8:
			return auto_ptr<MSXDevice>(new RomAscii8_8(config, time, rom,
			                       RomAscii8_8::KOEI_8));
		case KOEI_32:
			return auto_ptr<MSXDevice>(new RomAscii8_8(config, time, rom,
			                       RomAscii8_8::KOEI_32));
		case WIZARDRY:
			return auto_ptr<MSXDevice>(new RomAscii8_8(config, time, rom,
			                       RomAscii8_8::WIZARDRY));
		case HYDLIDE2:
			return auto_ptr<MSXDevice>(new RomHydlide2(config, time, rom));
		case GAME_MASTER2:
			return auto_ptr<MSXDevice>(new RomGameMaster2(config, time, rom));
		case PANASONIC:
			return auto_ptr<MSXDevice>(new RomPanasonic(config, time, rom));
		case NATIONAL:
			return auto_ptr<MSXDevice>(new RomNational(config, time, rom));
		case MAJUTSUSHI:
			return auto_ptr<MSXDevice>(new RomMajutsushi(config, time, rom));
		case SYNTHESIZER:
			return auto_ptr<MSXDevice>(new RomSynthesizer(config, time, rom));
		case HALNOTE:
			return auto_ptr<MSXDevice>(new RomHalnote(config, time, rom));
		case KOREAN80IN1:
			return auto_ptr<MSXDevice>(new RomKorean80in1(config, time, rom));
		case KOREAN90IN1:
			return auto_ptr<MSXDevice>(new RomKorean90in1(config, time, rom));
		case KOREAN126IN1:
			return auto_ptr<MSXDevice>(new RomKorean126in1(config, time, rom));
		case HOLY_QURAN:
			return auto_ptr<MSXDevice>(new RomHolyQuran(config, time, rom));
		case FSA1FM1:
			return auto_ptr<MSXDevice>(new RomFSA1FM1(config, time, rom));
		case FSA1FM2:
			return auto_ptr<MSXDevice>(new RomFSA1FM2(config, time, rom));
		default:
			throw FatalError("Unknown mapper type");
	}
}

} // namespace openmsx


// $Id$

#include <string.h>
#include "MSXFmPac.hh"
#include "File.hh"
#include "CartridgeSlotManager.hh"
#include "MSXConfig.hh"


MSXFmPacCLI msxFmPacCLI;

MSXFmPacCLI::MSXFmPacCLI()
{
	CommandLineParser::instance()->registerOption("-fmpac", this);
}

void MSXFmPacCLI::parseOption(const std::string &option,
                              std::list<std::string> &cmdLine)
{
	CommandLineParser::instance()->registerPostConfig(this);
}
const std::string& MSXFmPacCLI::optionHelp() const
{
	static const std::string text("Inserts an FM-PAC into the MSX machine");
	return text;
}
void MSXFmPacCLI::execute(MSXConfig *config)
{
	int ps, ss;
	CartridgeSlotManager::instance()->getSlot(ps, ss);
	std::ostringstream s;
	s << "<?xml version=\"1.0\"?>";
	s << "<msxconfig>";
	s << "<device id=\"FM PAC\">";
	s << "<type>FM-PAC</type>";
	s << "<slotted><ps>"<<ps<<"</ps><ss>"<<ss<<"</ss><page>1</page></slotted>";
	s << "<parameter name=\"filename\">FMPAC.ROM</parameter>";
	s << "<parameter name=\"volume\">13000</parameter>";
	s << "<parameter name=\"mode\">mono</parameter>";
	s << "<parameter name=\"loadsram\">true</parameter>";
	s << "<parameter name=\"savesram\">true</parameter>";
	s << "<parameter name=\"sramname\">FMPAC.PAC</parameter>";
	s << "</device>";
	s << "</msxconfig>";
	config->loadStream("", s);
}



static const char* PAC_Header = "PAC2 BACKUP DATA";
//                               1234567890123456

MSXFmPac::MSXFmPac(Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMusic(config, time), 
	  sram(0x1FFE, config, PAC_Header)
{
	reset(time);
}

MSXFmPac::~MSXFmPac()
{
}


void MSXFmPac::reset(const EmuTime &time)
{
	MSXMusic::reset(time);
	enable = 0;
	sramEnabled = false;
	bank = 0;	// TODO check this
}


void MSXFmPac::writeIO(byte port, byte value, const EmuTime &time)
{
	if (enable & 1) {
		MSXMusic::writeIO(port, value, time);
	}
}


byte MSXFmPac::readMem(word address, const EmuTime &time)
{
	switch (address) {
		case 0x7FF4:
			// read from YM2413 register port
			return 0xFF;
		case 0x7FF5:
			// read from YM2413 data port
			return 0xFF;
		case 0x7FF6:
			return enable;
		case 0x7FF7:
			return bank;
		default:
			address &= 0x3FFF;
			if (sramEnabled && (address < 0x1FFE)) {
				return sram.read(address);
			} else {
				return rom.read(bank * 0x4000 + address);
			}
	}
}

void MSXFmPac::writeMem(word address, byte value, const EmuTime &time)
{
	switch (address) {
		case 0x5FFE:
			r5ffe = value;
			checkSramEnable();
			break;
		case 0x5FFF:
			r5fff = value;
			checkSramEnable();
			break;
		case 0x7FF4:
			if (enable & 1)	// TODO check this
				writeRegisterPort(value, time);
			break;
		case 0x7FF5:
			if (enable & 1)	// TODO check this
				writeDataPort(value, time);
			break;
		case 0x7FF6:
			enable = value & 0x11;
			break;
		case 0x7FF7:
			bank = value & 0x03;
			PRT_DEBUG("FmPac: bank " << (int)bank);
			break;
		default:
			if (sramEnabled && (address < 0x5FFE))
				sram.write(address - 0x4000, value);
	}
}

void MSXFmPac::checkSramEnable()
{
	sramEnabled = ((r5ffe == 0x4D) && (r5fff == 0x69)) ? true : false;
}

const byte* MSXFmPac::getReadCacheLine(word start) const
{
	// TODO
	return NULL;
}

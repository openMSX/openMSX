// $Id$

#include <string.h>
#include "MSXFmPac.hh"
#include "File.hh"
#include "CartridgeSlotManager.hh"
#include "SRAM.hh"


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
const std::string& MSXFmPacCLI::optionHelp()
{
	static const std::string text("Inserts an FM-PAC into the MSX machine");
	return text;
}
void MSXFmPacCLI::execute(MSXConfig::Backend *config)
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
	config->loadStream(s);
}



MSXFmPac::MSXFmPac(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXYM2413(config, time), 
	  MSXMemDevice(config, time), MSXRomDevice(config, time, 0x10000)
{
	sram = new SRAM(0x1FFE, config, PAC_Header);
	reset(time);
}

const char* MSXFmPac::PAC_Header = "PAC2 BACKUP DATA";
//                                  1234567890123456

MSXFmPac::~MSXFmPac()
{
	delete sram;
}

void MSXFmPac::reset(const EmuTime &time)
{
	MSXYM2413::reset(time);
	sramEnabled = false;
	bank = 0;	// TODO check this
}

byte MSXFmPac::readMem(word address, const EmuTime &time)
{
	switch (address) {
		case 0x7ff4:
			// read from YM2413 register port
			return 0xff;
		case 0x7ff5:
			// read from YM2413 data port
			return 0xff;
		case 0x7ff6:
			return enable;
		case 0x7ff7:
			return bank;
		default:
			address &= 0x3fff;
			if (sramEnabled && (address < 0x1ffe)) {
				return sram->read(address);
			} else {
				return romBank[bank * 0x4000 + address];
			}
	}
}

void MSXFmPac::writeMem(word address, byte value, const EmuTime &time)
{
	switch (address) {
		case 0x5ffe:
			r5ffe = value;
			checkSramEnable();
			break;
		case 0x5fff:
			r5fff = value;
			checkSramEnable();
			break;
		case 0x7ff4:
			if (enable & 1)	// TODO check this
				writeRegisterPort(value, time);
			break;
		case 0x7ff5:
			if (enable & 1)	// TODO check this
				writeDataPort(value, time);
			break;
		case 0x7ff6:
			enable = value & 0x11;
			break;
		case 0x7ff7:
			bank = value & 0x03;
			PRT_DEBUG("FmPac: bank " << (int)bank);
			break;
		default:
			if (sramEnabled && (address < 0x5ffe))
				sram->write(address - 0x4000, value);
	}
}

void MSXFmPac::checkSramEnable()
{
	sramEnabled = ((r5ffe == 0x4d) && (r5fff == 0x69)) ? true : false;
}


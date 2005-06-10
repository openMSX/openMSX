// $Id$

#include "StringOp.hh"
#include "XMLElement.hh"
#include "MSXRomCLI.hh"
#include "CartridgeSlotManager.hh"
#include "HardwareConfig.hh"
#include "FileOperations.hh"
#include "FileContext.hh"
#include "RomInfo.hh"
#include "CliComm.hh"
#include "MSXException.hh"

using std::auto_ptr;
using std::list;
using std::string;
using std::vector;

namespace openmsx {

MSXRomCLI::MSXRomCLI(CommandLineParser& cmdLineParser)
	: cartridgeNr(0)
{
	cmdLineParser.registerOption("-ips", &ipsOption);
	cmdLineParser.registerOption("-romtype", &romTypeOption);
	cmdLineParser.registerOption("-cart", this);
	cmdLineParser.registerOption("-carta", this);
	cmdLineParser.registerOption("-cartb", this);
	cmdLineParser.registerOption("-cartc", this);
	cmdLineParser.registerOption("-cartd", this);
	cmdLineParser.registerFileClass("romimage", this);
}

bool MSXRomCLI::parseOption(const string& option, list<string>& cmdLine)
{
	string arg = getArgument(option, cmdLine);
	string slotname;
	if (option.length() == 6) {
		int slot = option[5] - 'a';
		CartridgeSlotManager::instance().reserveSlot(slot);
		slotname = option[5];
	} else {
		slotname = "any";
	}
	parse(arg, slotname, cmdLine);
	return true;
}

const string& MSXRomCLI::optionHelp() const
{
	static const string text("Insert the ROM file (cartridge) specified in argument");
	return text;
}

void MSXRomCLI::parseFileType(const string& arg, list<string>& cmdLine)
{
	parse(arg, "any", cmdLine);
}

const string& MSXRomCLI::fileTypeHelp() const
{
	static const string text("ROM image of a cartridge");
	return text;
}


void MSXRomCLI::parse(const string& arg, const string& slotname,
                      list<string>& cmdLine)
{
	vector<string> ipsfiles;
	string mapper;
	string romfile = arg;

	// parse extra options  -ips  and  -romtype
	while (true) {
		string extra = peekArgument(cmdLine);
		if (extra == "-ips") {
			cmdLine.pop_front();
			ipsfiles.push_back(getArgument("-ips", cmdLine));
		} else if (extra == "-romtype") {
			cmdLine.pop_front();
			mapper = getArgument("-romtype", cmdLine);
		} else {
			break;
		}
	}

	string sramfile = FileOperations::getFilename(romfile);

	auto_ptr<XMLElement> primary(new XMLElement("primary"));
	primary->addAttribute("slot", slotname);
	auto_ptr<XMLElement> secondary(new XMLElement("secondary"));
	secondary->addAttribute("slot", slotname);
	auto_ptr<XMLElement> device(new XMLElement("ROM"));
	device->addAttribute("id", "MSXRom" + StringOp::toString(++cartridgeNr));
	auto_ptr<XMLElement> mem(new XMLElement("mem"));
	mem->addAttribute("base", "0x0000");
	mem->addAttribute("size", "0x10000");
	device->addChild(mem);
	auto_ptr<XMLElement> rom(new XMLElement("rom"));
	rom->addChild(auto_ptr<XMLElement>(
		new XMLElement("filename", romfile)));
	if (!ipsfiles.empty()) {
		auto_ptr<XMLElement> patches(new XMLElement("patches"));
		for (vector<string>::const_iterator it = ipsfiles.begin();
		     it != ipsfiles.end(); ++it) {
			patches->addChild(auto_ptr<XMLElement>(
				new XMLElement("ips", *it)));
		}
		rom->addChild(patches);
	}
	device->addChild(rom);
	auto_ptr<XMLElement> sound(new XMLElement("sound"));
	sound->addChild(auto_ptr<XMLElement>(
		new XMLElement("volume", "9000")));
	device->addChild(sound);
	device->addChild(auto_ptr<XMLElement>(
		new XMLElement("mappertype",
		               mapper.empty() ? "auto" : mapper)));
	device->addChild(auto_ptr<XMLElement>(
		new XMLElement("sramname", sramfile + ".SRAM")));
	device->setFileContext(auto_ptr<FileContext>(
		new UserFileContext("roms/" + sramfile)));
	
	secondary->addChild(device);
	primary->addChild(secondary);
	HardwareConfig::instance().getChild("devices").addChild(primary);
}


bool MSXRomCLI::IpsOption::parseOption(const string& /*option*/,
                                       list<string>& /*cmdLine*/)
{
	throw FatalError(
		"-ips options should immediately follow a ROM or disk image.");
}

const string& MSXRomCLI::IpsOption::optionHelp() const
{
	static const string text(
		"Apply the given IPS patch to the ROM or disk iamge in front.");
	return text;
}

bool MSXRomCLI::RomTypeOption::parseOption(const string& /*option*/,
                                       list<string>& /*cmdLine*/)
{
	throw FatalError("-romtype options should immediately follow a ROM.");
}

const string& MSXRomCLI::RomTypeOption::optionHelp() const
{
	static const string text("Specify the rom type for the ROM in front.");
	return text;
}

} // namespace openmsx

// $Id$

#include "StringOp.hh"
#include "xmlx.hh"
#include "MSXRomCLI.hh"
#include "CartridgeSlotManager.hh"
#include "HardwareConfig.hh"
#include "FileOperations.hh"
#include "FileContext.hh"

using std::ostringstream;

namespace openmsx {

MSXRomCLI::MSXRomCLI(CommandLineParser& cmdLineParser)
	: cartridgeNr(0)
{
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
	parse(arg, slotname);
	return true;
}

const string& MSXRomCLI::optionHelp() const
{
	static const string text("Insert the ROM file (cartridge) specified in argument");
	return text;
}

void MSXRomCLI::parseFileType(const string& arg)
{
	parse(arg, "any");
}

const string& MSXRomCLI::fileTypeHelp() const
{
	static const string text("ROM image of a cartridge");
	return text;
}


void MSXRomCLI::parse(const string& arg, const string& slotname)
{
	string romfile;
	string mapper;
	int pos = arg.find_last_of(',');
	int pos2 = arg.find_last_of('.');
	if ((pos != -1) && (pos > pos2)) {
		romfile = arg.substr(0, pos);
		mapper = arg.substr(pos + 1);
	} else {
		romfile = arg;
		mapper = "auto";
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
	rom->addChild(auto_ptr<XMLElement>(
		new XMLElement("mappertype", mapper)));
	device->addChild(rom);
	auto_ptr<XMLElement> sound(new XMLElement("sound"));
	sound->addChild(auto_ptr<XMLElement>(
		new XMLElement("volume", "9000")));
	device->addChild(sound);
	device->addChild(auto_ptr<XMLElement>(
		new XMLElement("sramname", sramfile + ".SRAM")));
	device->setFileContext(auto_ptr<FileContext>(
		new UserFileContext("roms/" + sramfile)));
	secondary->addChild(device);
	primary->addChild(secondary);
	HardwareConfig::instance().getChild("devices").addChild(primary);
}

} // namespace openmsx

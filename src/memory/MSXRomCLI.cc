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
	if (option.length() == 6) {
		int slot = option[5] - 'a';
		CartridgeSlotManager::instance().reserveSlot(slot);
		CommandLineParser::instance().registerPostConfig(new MSXRomPostName(slot, arg));
	} else {
		CommandLineParser::instance().registerPostConfig(new MSXRomPostNoName(arg));
	}
	return true;
}
const string& MSXRomCLI::optionHelp() const
{
	static const string text("Insert the ROM file (cartridge) specified in argument");
	return text;
}
MSXRomPostName::MSXRomPostName(int slot_, const string& arg_)
	: MSXRomCLIPost(arg_), slot(slot_)
{
}
void MSXRomPostName::execute()
{
	CartridgeSlotManager::instance().getSlot(slot, ps, ss);
	MSXRomCLIPost::execute();
}

void MSXRomCLI::parseFileType(const string &arg)
{
	CommandLineParser::instance().registerPostConfig(new MSXRomPostNoName(arg));
}
const string& MSXRomCLI::fileTypeHelp() const
{
	static const string text("ROM image of a cartridge");
	return text;
}
MSXRomPostNoName::MSXRomPostNoName(const string &arg_)
	: MSXRomCLIPost(arg_)
{
}
void MSXRomPostNoName::execute()
{
	CartridgeSlotManager::instance().getSlot(ps, ss);
	MSXRomCLIPost::execute();
}

MSXRomCLIPost::MSXRomCLIPost(const string& arg_)
	: arg(arg_)
{
}

void MSXRomCLIPost::execute()
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
	primary->addAttribute("slot", StringOp::toString(ps));
	auto_ptr<XMLElement> secondary(new XMLElement("secondary"));
	secondary->addAttribute("slot", StringOp::toString(ss));
	auto_ptr<XMLElement> device(new XMLElement("ROM"));
	device->addAttribute("id", "MSXRom" + StringOp::toString(ps) +
	                               "-" + StringOp::toString(ss));
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
	device->addChild(auto_ptr<XMLElement>(
		new XMLElement("volume", "9000")));
	device->addChild(auto_ptr<XMLElement>(
		new XMLElement("sramname", sramfile + ".SRAM")));
	device->setFileContext(auto_ptr<FileContext>(
		new UserFileContext("roms/" + sramfile)));
	secondary->addChild(device);
	primary->addChild(secondary);
	HardwareConfig::instance().getChild("devices").addChild(primary);

	delete this;
}

} // namespace openmsx

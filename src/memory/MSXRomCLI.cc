// $Id$

#include "StringOp.hh"
#include "xmlx.hh"
#include "MSXRomCLI.hh"
#include "CartridgeSlotManager.hh"
#include "HardwareConfig.hh"
#include "FileOperations.hh"
#include "FileContext.hh"

using std::auto_ptr;
using std::list;
using std::string;

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
	bool hasips=false;
	string ipsfile;
	string romfile;
	string mapper;

	int firstpos = arg.find_first_of(',');
	int lastpos = arg.find_last_of(',');
//	cout << "firstpos:" << firstpos <<endl;
//	cout << "lastpos:" << lastpos <<endl;
	if (firstpos != -1 ) {
		romfile = arg.substr(0, firstpos);
		if (firstpos != lastpos) {
			ipsfile = arg.substr(lastpos + 1);
			hasips = true;
		}
		if (firstpos == lastpos){
			mapper = arg.substr(lastpos + 1);
		} else if (firstpos == (lastpos-1)){ // a ',,' found
			mapper = "auto";
		} else {
			mapper = arg.substr(firstpos + 1, lastpos - firstpos -1);
		}
	} else {
		romfile = arg;
		mapper = "auto";
	}
//	cout << "mapper:" << mapper<<endl;
//	cout << "romfile:" << romfile <<endl;
//	cout << "ispfile:" << ipsfile<<endl;

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
	if ( hasips ) {
		auto_ptr<XMLElement> ips(new XMLElement("ips"));
		ips->addChild(auto_ptr<XMLElement>(
			new XMLElement("filename", ipsfile)));
		rom->addChild(ips);
	}
	device->addChild(rom);
	auto_ptr<XMLElement> sound(new XMLElement("sound"));
	sound->addChild(auto_ptr<XMLElement>(
		new XMLElement("volume", "9000")));
	device->addChild(sound);
	device->addChild(auto_ptr<XMLElement>(
		new XMLElement("mappertype", mapper)));
	device->addChild(auto_ptr<XMLElement>(
		new XMLElement("sramname", sramfile + ".SRAM")));
	device->setFileContext(auto_ptr<FileContext>(
		new UserFileContext("roms/" + sramfile)));
	
	secondary->addChild(device);
	primary->addChild(secondary);
	HardwareConfig::instance().getChild("devices").addChild(primary);
}

} // namespace openmsx

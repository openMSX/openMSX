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

MSXRomCLI::MSXRomCLI()
{
	CommandLineParser::instance().registerOption("-cart", this);
	CommandLineParser::instance().registerOption("-carta", this);
	CommandLineParser::instance().registerOption("-cartb", this);
	CommandLineParser::instance().registerOption("-cartc", this);
	CommandLineParser::instance().registerOption("-cartd", this);
	CommandLineParser::instance().registerFileClass("romimages", this);
}

bool MSXRomCLI::parseOption(const string &option,
                         list<string> &cmdLine)
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
MSXRomPostName::MSXRomPostName(int slot_, const string &arg_)
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

MSXRomCLIPost::MSXRomCLIPost(const string &arg_)
	: arg(arg_)
{
}


static XMLElement* createSlotted(int ps, int ss, int page)
{
	XMLElement* slotted = new XMLElement("slotted");
	slotted->addChild(new XMLElement("ps",   StringOp::toString(ps)));
	slotted->addChild(new XMLElement("ss",   StringOp::toString(ss)));
	slotted->addChild(new XMLElement("page", StringOp::toString(page)));
	return slotted;
}

static XMLElement* createParameter(const string& name, const string& value)
{
	XMLElement* parameter = new XMLElement("parameter", value);
	parameter->addAttribute("name", name);
	return parameter;
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
	string sramfile = FileOperations::getFilename(romfile) + ".SRAM";

	XMLElement device = XMLElement("device");
	device.addAttribute("id", "MSXRom" + StringOp::toString(ps) +
			               "-" + StringOp::toString(ss));
	device.addChild(new XMLElement("type", "Rom"));
	device.addChild(createSlotted(ps, ss, 0));
	device.addChild(createSlotted(ps, ss, 1));
	device.addChild(createSlotted(ps, ss, 2));
	device.addChild(createSlotted(ps, ss, 3));
	device.addChild(createParameter("filename", romfile));
	device.addChild(createParameter("volume", "9000"));
	device.addChild(createParameter("mappertype", mapper));
	device.addChild(createParameter("loadsram", "true"));
	device.addChild(createParameter("savesram", "true"));
	device.addChild(createParameter("sramname", sramfile + ".SRAM"));

	UserFileContext context("roms/" + sramfile);
	HardwareConfig::instance().loadConfig(device, context);
	delete this;
}

} // namespace openmsx

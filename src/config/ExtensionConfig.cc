// $Id$

#include "ExtensionConfig.hh"
#include "MSXMotherBoard.hh"
#include "FileContext.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "StringOp.hh"

using std::string;
using std::vector;
using std::auto_ptr;

namespace openmsx {

ExtensionConfig::ExtensionConfig(MSXMotherBoard& motherBoard,
                                 const string& extensionName)
	: HardwareConfig(motherBoard)
{
	load("extensions", extensionName);
	setName(extensionName);
}

ExtensionConfig::ExtensionConfig(
		MSXMotherBoard& motherBoard,
		const string& romfile, const string& slotname,
		const vector<string>& options)
	: HardwareConfig(motherBoard)
{
	vector<string> ipsfiles;
	string mapper;

	// parse options
	for (vector<string>::const_iterator it = options.begin();
	     it != options.end(); ++it) {
		const string& option = *it++;
		if (it == options.end()) {
			throw MSXException("Missing argument for option \"" +
			                   option + "\"");
		}
		if (option == "-ips") {
			ipsfiles.push_back(*it);
		} else if (option == "-romtype") {
			mapper = *it;
		} else {
			throw MSXException("Invalid option \"" + option + "\"");
		}
	}

	auto_ptr<XMLElement> extension(new XMLElement("extension"));
	auto_ptr<XMLElement> primary(new XMLElement("primary"));
	primary->addAttribute("slot", slotname);
	auto_ptr<XMLElement> secondary(new XMLElement("secondary"));
	secondary->addAttribute("slot", slotname);
	auto_ptr<XMLElement> device(new XMLElement("ROM"));
	device->addAttribute("id", "MSXRom");
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
	string sramfile = FileOperations::getFilename(romfile);
	device->addChild(auto_ptr<XMLElement>(
		new XMLElement("sramname", sramfile + ".SRAM")));
	device->setFileContext(auto_ptr<FileContext>(
		new UserFileContext(motherBoard.getCommandController(),
		                    "roms/" + sramfile)));

	secondary->addChild(device);
	primary->addChild(secondary);
	extension->addChild(primary);

	setConfig(extension);

	setName(romfile);
}

const XMLElement& ExtensionConfig::getDevices() const
{
	const XMLElement& config = getConfig();
	if (const XMLElement* devices = config.findChild("devices")) {
		return *devices;
	}
	// TODO in the future make <devices> tag obligatory
	//      (as it already is for MachineConfig)
	return config;
}

const string& ExtensionConfig::getName() const
{
	return name;
}

void ExtensionConfig::setName(const std::string& proposedName)
{
	if (!getMotherBoard().findExtension(proposedName)) {
		name = proposedName;
	} else {
		unsigned n = 0;
		do {
			name = proposedName + " (" + StringOp::toString(++n) + ")";
		} while (getMotherBoard().findExtension(name));
	}
}

} // namespace openmsx

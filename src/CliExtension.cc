// $Id$

#include "CliExtension.hh"
#include "HardwareConfig.hh"
#include "FileException.hh"
#include "ConfigException.hh"

namespace openmsx {

CliExtension::CliExtension(CommandLineParser& cmdLineParser)
{
	cmdLineParser.registerOption("-ext", this);
}

CliExtension::~CliExtension()
{
}

bool CliExtension::parseOption(const string &option,
                               list<string> &cmdLine)
{
	string extension = getArgument(option, cmdLine);
	XMLElement& devices = HardwareConfig::instance().getChild("devices");
	try {
		HardwareConfig::loadHardware(devices, "extensions", extension);
	} catch (FileException& e) {
		throw FatalError("Extension \"" + extension + "\" not found (" +
		                 e.getMessage() + ").");
	} catch (ConfigException& e) {
		throw FatalError("Error in \"" + extension + "\" extension (" +
		                 e.getMessage());
	}
	return true;
}

const string& CliExtension::optionHelp() const
{
	static string help("Insert the extension specified in argument");
	return help;
}

} // namespace openmsx

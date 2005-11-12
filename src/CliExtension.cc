// $Id$

#include "CliExtension.hh"
#include "HardwareConfig.hh"
#include "FileException.hh"
#include "ConfigException.hh"

using std::string;

namespace openmsx {

CliExtension::CliExtension(CommandLineParser& cmdLineParser_)
	: cmdLineParser(cmdLineParser_)
{
	cmdLineParser.registerOption("-ext", this);
}

bool CliExtension::parseOption(const string& option,
                               std::list<string>& cmdLine)
{
	string extension = getArgument(option, cmdLine);
	HardwareConfig& hardwareConfig = cmdLineParser.getHardwareConfig();
	XMLElement* devices = hardwareConfig.findChild("devices");
	if (!devices) {
		// TODO refactor command line parser
		//  this happens when -h or -v was given (no machine loaded)
		return true;
	}
	try {
		hardwareConfig.loadHardware(*devices, "extensions", extension);
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

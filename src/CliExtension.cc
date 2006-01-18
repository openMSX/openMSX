// $Id$

#include "CliExtension.hh"
#include "ExtensionConfig.hh"
#include "MSXMotherBoard.hh"
#include "FileException.hh"
#include "ConfigException.hh"

using std::string;

namespace openmsx {

CliExtension::CliExtension(CommandLineParser& cmdLineParser_)
	: cmdLineParser(cmdLineParser_)
{
	cmdLineParser.registerOption("-ext", this);
}

bool CliExtension::parseOption(const string& option, std::list<string>& cmdLine)
{
	string extension = getArgument(option, cmdLine);
	try {
		MSXMotherBoard& motherBoard = cmdLineParser.getMotherBoard();
		std::auto_ptr<ExtensionConfig> extConfig(
			new ExtensionConfig(motherBoard));
		extConfig->load("extensions", extension);
		motherBoard.addExtension(extConfig);
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

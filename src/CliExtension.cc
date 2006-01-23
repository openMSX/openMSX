// $Id$

#include "CliExtension.hh"
#include "MSXMotherBoard.hh"
#include "MSXException.hh"

using std::string;

namespace openmsx {

CliExtension::CliExtension(CommandLineParser& cmdLineParser_)
	: cmdLineParser(cmdLineParser_)
{
	cmdLineParser.registerOption("-ext", this);
}

bool CliExtension::parseOption(const string& option, std::list<string>& cmdLine)
{
	try {
		string extensionName = getArgument(option, cmdLine);
		cmdLineParser.getMotherBoard().loadExtension(extensionName);
	} catch (MSXException& e) {
		throw FatalError(e.getMessage());
	}
	return true;
}

const string& CliExtension::optionHelp() const
{
	static string help("Insert the extension specified in argument");
	return help;
}

} // namespace openmsx

// $Id$

#ifndef CLIEXTENSION_HH
#define CLIEXTENSION_HH

#include "CommandLineParser.hh"

namespace openmsx {

class CliExtension : public CLIOption
{
public:
	CliExtension(CommandLineParser& cmdLineParser);
	virtual ~CliExtension();
	
	virtual bool parseOption(const std::string& option,
	                         std::list<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;
};

} // namespace openmsx

#endif

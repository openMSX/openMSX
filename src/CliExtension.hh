// $Id$

#ifndef __CLIEXTENSION_HH__
#define __CLIEXTENSION_HH__

#include "CommandLineParser.hh"

namespace openmsx {

class CliExtension : public CLIOption
{
public:
	CliExtension(CommandLineParser& cmdLineParser);
	virtual ~CliExtension();
	
	virtual bool parseOption(const string& option, list<string>& cmdLine);
	virtual const string& optionHelp() const;
};

} // namespace openmsx

#endif

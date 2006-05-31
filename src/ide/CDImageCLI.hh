// $Id$

#ifndef CDIMAGECLI_HH
#define CDIMAGECLI_HH

#include "CommandLineParser.hh"

namespace openmsx {

class CommandController;

class CDImageCLI : public CLIOption
{
public:
	explicit CDImageCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::list<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;

private:
	CommandController& commandController;
};

} // namespace openmsx

#endif

// $Id$

#ifndef CDIMAGECLI_HH
#define CDIMAGECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;
class CommandController;

class CDImageCLI : public CLIOption
{
public:
	explicit CDImageCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::deque<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;

private:
	CommandController& commandController;
};

} // namespace openmsx

#endif

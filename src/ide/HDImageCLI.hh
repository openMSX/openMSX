// $Id$

#ifndef HDIMAGECLI_HH
#define HDIMAGECLI_HH

#include "CommandLineParser.hh"

namespace openmsx {

class CommandController;

class HDImageCLI : public CLIOption
{
public:
	explicit HDImageCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::list<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;

private:
	CommandController& commandController;
};

} // namespace openmsx

#endif

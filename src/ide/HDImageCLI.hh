// $Id$

#ifndef HDIMAGECLI_HH
#define HDIMAGECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;
class GlobalCommandController;

class HDImageCLI : public CLIOption
{
public:
	explicit HDImageCLI(CommandLineParser& cmdLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::deque<std::string>& cmdLine);
	virtual string_ref optionHelp() const;

private:
	GlobalCommandController& commandController;
};

} // namespace openmsx

#endif

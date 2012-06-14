// $Id$

#ifndef SAVESTATECLI_HH
#define SAVESTATECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;
class GlobalCommandController;

class SaveStateCLI : public CLIOption, public CLIFileType
{
public:
	explicit SaveStateCLI(CommandLineParser& commandLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::deque<std::string>& cmdLine);
	virtual string_ref optionHelp() const;
	virtual void parseFileType(const std::string& filename,
	                           std::deque<std::string>& cmdLine);
	virtual string_ref fileTypeHelp() const;

private:
	GlobalCommandController& commandController;
};

} // namespace openmsx

#endif

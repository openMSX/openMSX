// $Id$

#ifndef CASSETTEPLAYERCLI_HH
#define CASSETTEPLAYERCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;
class CommandController;

class CassettePlayerCLI : public CLIOption, public CLIFileType
{
public:
	explicit CassettePlayerCLI(CommandLineParser& commandLineParser);
	virtual bool parseOption(const std::string& option,
	                         std::deque<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;
	virtual void parseFileType(const std::string& filename,
	                           std::deque<std::string>& cmdLine);
	virtual const std::string& fileTypeHelp() const;

private:
	CommandController& commandController;
};

} // namespace openmsx

#endif

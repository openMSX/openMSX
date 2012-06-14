// $Id$

#ifndef CLIEXTENSION_HH
#define CLIEXTENSION_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class CliExtension : public CLIOption
{
public:
	explicit CliExtension(CommandLineParser& cmdLineParser);

	virtual bool parseOption(const std::string& option,
	                         std::deque<std::string>& cmdLine);
	virtual string_ref optionHelp() const;

private:
	CommandLineParser& cmdLineParser;
};

} // namespace openmsx

#endif

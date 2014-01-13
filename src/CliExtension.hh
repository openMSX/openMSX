#ifndef CLIEXTENSION_HH
#define CLIEXTENSION_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class CliExtension : public CLIOption
{
public:
	explicit CliExtension(CommandLineParser& cmdLineParser);

	virtual void parseOption(const std::string& option,
	                         array_ref<std::string>& cmdLine);
	virtual string_ref optionHelp() const;

private:
	CommandLineParser& cmdLineParser;
};

} // namespace openmsx

#endif

#ifndef CDIMAGECLI_HH
#define CDIMAGECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class CDImageCLI final : public CLIOption
{
public:
	explicit CDImageCLI(CommandLineParser& cmdLineParser);
	virtual void parseOption(const std::string& option,
	                         array_ref<std::string>& cmdLine);
	virtual string_ref optionHelp() const;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

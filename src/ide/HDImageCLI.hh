#ifndef HDIMAGECLI_HH
#define HDIMAGECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class HDImageCLI final : public CLIOption
{
public:
	explicit HDImageCLI(CommandLineParser& cmdLineParser);
	virtual void parseOption(const std::string& option,
	                         array_ref<std::string>& cmdLine);
	virtual string_ref optionHelp() const;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

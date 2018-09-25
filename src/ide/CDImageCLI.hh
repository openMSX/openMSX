#ifndef CDIMAGECLI_HH
#define CDIMAGECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class CDImageCLI final : public CLIOption
{
public:
	explicit CDImageCLI(CommandLineParser& parser);
	void parseOption(const std::string& option,
	                 array_ref<std::string>& cmdLine) override;
	string_view optionHelp() const override;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

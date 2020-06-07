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
	                 std::span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view optionHelp() const override;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

#ifndef CLIEXTENSION_HH
#define CLIEXTENSION_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class CliExtension final : public CLIOption
{
public:
	explicit CliExtension(CommandLineParser& cmdLineParser);

	void parseOption(const std::string& option,
	                 span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view optionHelp() const override;

private:
	CommandLineParser& cmdLineParser;
};

} // namespace openmsx

#endif

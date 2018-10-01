#ifndef REPLAYCLI_HH
#define REPLAYCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class ReplayCLI final : public CLIOption, public CLIFileType
{
public:
	explicit ReplayCLI(CommandLineParser& parser);
	void parseOption(const std::string& option,
	                 array_ref<std::string>& cmdLine) override;
	string_view optionHelp() const override;
	void parseFileType(const std::string& filename,
	                   array_ref<std::string>& cmdLine) override;
	string_view fileTypeHelp() const override;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

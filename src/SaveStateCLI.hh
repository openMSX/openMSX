#ifndef SAVESTATECLI_HH
#define SAVESTATECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class SaveStateCLI final : public CLIOption, public CLIFileType
{
public:
	explicit SaveStateCLI(CommandLineParser& parser);
	void parseOption(const std::string& option,
	                 std::span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view optionHelp() const override;
	void parseFileType(const std::string& filename,
	                   std::span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view fileTypeHelp() const override;
	[[nodiscard]] std::string_view fileTypeCategoryName() const override;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

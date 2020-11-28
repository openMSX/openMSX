#ifndef LASERDISCPLAYERCLI_HH
#define LASERDISCPLAYERCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class LaserdiscPlayerCLI final : public CLIOption, public CLIFileType
{
public:
	explicit LaserdiscPlayerCLI(CommandLineParser& parser);
	void parseOption(const std::string& option,
	                 span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view optionHelp() const override;
	void parseFileType(const std::string& filename,
	                   span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view fileTypeHelp() const override;
	[[nodiscard]] std::string_view fileTypeCategoryName() const override;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

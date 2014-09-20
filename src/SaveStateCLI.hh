#ifndef SAVESTATECLI_HH
#define SAVESTATECLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class SaveStateCLI final : public CLIOption, public CLIFileType
{
public:
	explicit SaveStateCLI(CommandLineParser& commandLineParser);
	void parseOption(const std::string& option,
	                 array_ref<std::string>& cmdLine) override;
	string_ref optionHelp() const override;
	void parseFileType(const std::string& filename,
	                   array_ref<std::string>& cmdLine) override;
	string_ref fileTypeHelp() const override;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

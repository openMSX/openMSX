#ifndef LASERDISCPLAYERCLI_HH
#define LASERDISCPLAYERCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class LaserdiscPlayerCLI final : public CLIOption, public CLIFileType
{
public:
	explicit LaserdiscPlayerCLI(CommandLineParser& commandLineParser);
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

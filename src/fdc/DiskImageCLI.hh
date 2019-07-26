#ifndef DISKIMAGEMANAGER_HH
#define DISKIMAGEMANAGER_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class DiskImageCLI final : public CLIOption, public CLIFileType
{
public:
	explicit DiskImageCLI(CommandLineParser& parser);
	void parseOption(const std::string& option,
	                 span<std::string>& cmdLine) override;
	std::string_view optionHelp() const override;
	void parseFileType(const std::string& filename,
	                   span<std::string>& cmdLine) override;
	std::string_view fileTypeHelp() const override;

private:
	void parse(std::string_view drive, std::string_view image,
	           span<std::string>& cmdLine);

	CommandLineParser& parser;
	char driveLetter;
};

} // namespace openmsx

#endif

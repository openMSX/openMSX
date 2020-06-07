#ifndef DISKIMAGEMANAGER_HH
#define DISKIMAGEMANAGER_HH

#include "CLIOption.hh"
#include "zstring_view.hh"

namespace openmsx {

class CommandLineParser;

class DiskImageCLI final : public CLIOption, public CLIFileType
{
public:
	explicit DiskImageCLI(CommandLineParser& parser);
	void parseOption(const std::string& option,
	                 std::span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view optionHelp() const override;
	void parseFileType(const std::string& filename,
	                   std::span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view fileTypeHelp() const override;
	[[nodiscard]] std::string_view fileTypeCategoryName() const override;

private:
	void parse(zstring_view drive, std::string_view image,
	           std::span<std::string>& cmdLine);

private:
	CommandLineParser& parser;
	char driveLetter;
};

} // namespace openmsx

#endif

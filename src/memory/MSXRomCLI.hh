#ifndef MSXROMCLI_HH
#define MSXROMCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class MSXRomCLI final : public CLIOption, public CLIFileType
{
public:
	static std::span<const std::string_view> getExtensions();

public:
	explicit MSXRomCLI(CommandLineParser& cmdLineParser);

	void parseOption(const std::string& option,
	                 std::span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view optionHelp() const override;

	void parseFileType(const std::string& arg,
	                   std::span<std::string>& cmdLine) override;
	[[nodiscard]] std::string_view fileTypeHelp() const override;
	[[nodiscard]] std::string_view fileTypeCategoryName() const override;

private:
	void parse(const std::string& arg, const std::string& slotName,
	           std::span<std::string>& cmdLine) const;

private:
	CommandLineParser& cmdLineParser;

	struct IpsOption final : CLIOption {
		[[noreturn]] void parseOption(const std::string& option,
		                 std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} ipsOption;
	struct RomTypeOption final : CLIOption {
		[[noreturn]] void parseOption(const std::string& option,
		                 std::span<std::string>& cmdLine) override;
		[[nodiscard]] std::string_view optionHelp() const override;
	} romTypeOption;
};

} // namespace openmsx

#endif

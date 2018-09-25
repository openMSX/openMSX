#ifndef MSXROMCLI_HH
#define MSXROMCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class MSXRomCLI final : public CLIOption, public CLIFileType
{
public:
	explicit MSXRomCLI(CommandLineParser& cmdLineParser);

	void parseOption(const std::string& option,
	                 array_ref<std::string>& cmdLine) override;
	string_view optionHelp() const override;

	void parseFileType(const std::string& arg,
	                   array_ref<std::string>& cmdLine) override;
	string_view fileTypeHelp() const override;

private:
	void parse(const std::string& arg, const std::string& slotname,
	           array_ref<std::string>& cmdLine);

	CommandLineParser& cmdLineParser;

	struct IpsOption final : CLIOption {
		void parseOption(const std::string& option,
		                 array_ref<std::string>& cmdLine) override;
		string_view optionHelp() const override;
	} ipsOption;
	struct RomTypeOption final : CLIOption {
		void parseOption(const std::string& option,
		                 array_ref<std::string>& cmdLine) override;
		string_view optionHelp() const override;
	} romTypeOption;
};

} // namespace openmsx

#endif

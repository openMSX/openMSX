#ifndef MSXROMCLI_HH
#define MSXROMCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class MSXRomCLI : public CLIOption, public CLIFileType
{
public:
	explicit MSXRomCLI(CommandLineParser& cmdLineParser);

	virtual void parseOption(const std::string& option,
	                         array_ref<std::string>& cmdLine);
	virtual string_ref optionHelp() const;

	virtual void parseFileType(const std::string& filename,
	                           array_ref<std::string>& cmdLine);
	virtual string_ref fileTypeHelp() const;

private:
	void parse(const std::string& arg, const std::string& slotname,
	           array_ref<std::string>& cmdLine);

	CommandLineParser& cmdLineParser;

	class IpsOption : public CLIOption {
		virtual void parseOption(const std::string& option,
		                         array_ref<std::string>& cmdLine);
		virtual string_ref optionHelp() const;
	} ipsOption;
	class RomTypeOption : public CLIOption {
		virtual void parseOption(const std::string& option,
		                         array_ref<std::string>& cmdLine);
		virtual string_ref optionHelp() const;
	} romTypeOption;
};

} // namespace openmsx

#endif

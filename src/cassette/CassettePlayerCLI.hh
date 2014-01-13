#ifndef CASSETTEPLAYERCLI_HH
#define CASSETTEPLAYERCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class CassettePlayerCLI : public CLIOption, public CLIFileType
{
public:
	explicit CassettePlayerCLI(CommandLineParser& commandLineParser);
	virtual void parseOption(const std::string& option,
	                         array_ref<std::string>& cmdLine);
	virtual string_ref optionHelp() const;
	virtual void parseFileType(const std::string& filename,
	                           array_ref<std::string>& cmdLine);
	virtual string_ref fileTypeHelp() const;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

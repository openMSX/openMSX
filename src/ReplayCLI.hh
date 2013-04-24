#ifndef REPLAYCLI_HH
#define REPLAYCLI_HH

#include "CLIOption.hh"

namespace openmsx {

class CommandLineParser;

class ReplayCLI : public CLIOption, public CLIFileType
{
public:
	explicit ReplayCLI(CommandLineParser& commandLineParser);
	virtual void parseOption(const std::string& option,
	                         std::deque<std::string>& cmdLine);
	virtual string_ref optionHelp() const;
	virtual void parseFileType(const std::string& filename,
	                           std::deque<std::string>& cmdLine);
	virtual string_ref fileTypeHelp() const;

private:
	CommandLineParser& parser;
};

} // namespace openmsx

#endif

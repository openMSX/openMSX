// $Id$

#ifndef __MSXROMCLI_HH__
#define __MSXROMCLI_HH__

#include "CommandLineParser.hh"

namespace openmsx {

class MSXRomCLI : public CLIOption, public CLIFileType
{
public:
	MSXRomCLI(CommandLineParser& cmdLineParser);

	virtual bool parseOption(const std::string& option,
	                         std::list<std::string>& cmdLine);
	virtual const std::string& optionHelp() const;

	virtual void parseFileType(const std::string& filename,
	                           std::list<std::string>& cmdLine);
	virtual const std::string& fileTypeHelp() const;

private:
	void parse(const std::string& arg, const std::string& slotname,
	           std::list<std::string>& cmdLine);
	
	int cartridgeNr;

	class IpsOption : public CLIOption {
		virtual bool parseOption(const std::string& option,
					 std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	} ipsOption;
	class RomTypeOption : public CLIOption {
		virtual bool parseOption(const std::string& option,
					 std::list<std::string>& cmdLine);
		virtual const std::string& optionHelp() const;
	} romTypeOption;
};

} // namespace openmsx

#endif

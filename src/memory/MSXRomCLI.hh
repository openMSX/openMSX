// $Id$

#ifndef __MSXROMCLI_HH__
#define __MSXROMCLI_HH__

#include "CommandLineParser.hh"

namespace openmsx {

class MSXRomCLI : public CLIOption, public CLIFileType
{
public:
	MSXRomCLI(CommandLineParser& cmdLineParser);

	virtual bool parseOption(const string& option,
	                         list<string>& cmdLine);
	virtual const string& optionHelp() const;

	virtual void parseFileType(const string& filename);
	virtual const string& fileTypeHelp() const;

private:
	void parse(const string& arg, const string& slotname);
	
	int cartridgeNr;
};

} // namespace openmsx

#endif

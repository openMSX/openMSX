// $Id$

#ifndef __CLIEXTENSION_HH__
#define __CLIEXTENSION_HH__

#include "CommandLineParser.hh"
#include "StringOp.hh"

namespace openmsx {

class CliExtension : public CLIOption
{
public:
	CliExtension(CommandLineParser& cmdLineParser);
	virtual ~CliExtension();
	
	virtual bool parseOption(const string& option, list<string>& cmdLine);
	virtual const string& optionHelp() const;
	
private:
	void createExtensions(const string& path);
	
#ifdef FS_CASEINSENSE
	// on win32, MacOSX, filesystem itself is case-insensitive...
	typedef map<string, string, StringOp::caseless> ExtensionsMap;
#else
	typedef map<string, string> ExtensionsMap;
#endif
	ExtensionsMap extensions;
};

} // namespace openmsx

#endif

// $Id$

#ifndef __CLIEXTENSION_HH__
#define __CLIEXTENSION_HH__

#include "CommandLineParser.hh"

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
	struct caseltstr {
		bool operator()(const string& s1, const string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};
	typedef map<string, string, caseltstr> ExtensionsMap;
#else
	typedef map<string, string> ExtensionsMap;
#endif
	ExtensionsMap extensions;
};

} // namespace openmsx

#endif

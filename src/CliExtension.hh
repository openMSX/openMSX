// $Id$

#ifndef __CLIEXTENSION_HH__
#define __CLIEXTENSION_HH__

#include "CommandLineParser.hh"


namespace openmsx {

class CliExtension : public CLIOption
{
	public:
		CliExtension();
		virtual ~CliExtension();
		
		virtual bool parseOption(const string &option,
		                         list<string> &cmdLine);
		virtual const string& optionHelp() const;
		
	private:
		void createExtensions(const string &path);
		
#ifdef	FS_CASEINSENSE
		// on win32, MacOSX, filesystem itself is case-insensitive...
		struct caseltstr {
			bool operator()(const string s1, const string s2) const {
				return strcasecmp(s1.c_str(), s2.c_str()) < 0;
			}
		};
		map<string, string, caseltstr> extensions;
#else
		map<string, string> extensions;
#endif
};

} // namespace openmsx

#endif

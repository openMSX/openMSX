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
		
		virtual void parseOption(const string &option,
		                         list<string> &cmdLine);
		virtual const string& optionHelp() const;
		
	private:
		void createExtensions(const string &path);
		
		map<string, string> extensions;
};

} // namespace openmsx

#endif

// $Id$

#ifndef __CLIEXTENSION_HH__
#define __CLIEXTENSION_HH__

#include "CommandLineParser.hh"


class CliExtension : public CLIOption
{
	public:
		CliExtension();
		virtual ~CliExtension();
		
		virtual void parseOption(const std::string &option,
		                         std::list<std::string> &cmdLine);
		virtual const std::string& optionHelp() const;
		
	private:
		void createExtensions(const std::string &path);
		
		std::map<std::string, std::string> extensions;
};

#endif

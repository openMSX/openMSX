// $Id$

#ifndef __COMMANDLINEPARSER_HH__
#define __COMMANDLINEPARSER_HH__

#include "openmsx.hh"
#include "config.h"
#include "MSXConfig.hh"

class CommandLineParser
{
	public:
	  CommandLineParser(int argc, char **argv);

	  //std::string   checkFileType();
	  std::string	checkFileExt(char* filename);
	  void		parse(MSXConfig::Backend* config);
	  void		populateContexts();

	  static char** argv;
	  int argc;
	  static int curArgc;

	  class Option
	  {
	    public:
		Option(std::string shrt, std::string lng, int parameters, int maxUsage);
		~Option();
		
		bool parse();
		
		std::string shortName;
		std::string longName;
		int nrParams;
		int timesUsed;
		int max;
		std::list <std::string> parameters;
	  };

	  class Context
	  {
	    public:
		Context(std::string contextName);
		~Context();
		bool isContext(char* contextName);
		void parse();
		void setFilename(std::string nameOfFile);
		Option* getOption(char* nameOfOption);

		std::string name;
		bool used;
		std::string filename;
		std::list <Option*> options;
	  };

	  std::list <Context*> contexts;
	  Context* currentContext;
	  Context*	getContext(std::string name);

	  void configDisk(MSXConfig::Backend* config,std::string diskLetter,Context* curCont);
	  void configCart(MSXConfig::Backend* config,int primslot,Context* curCont);

};

#endif

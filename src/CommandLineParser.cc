#include "CommandLineParser.hh"
#include <string>
#include <sstream>
#include "MSXConfig.hh"
#include <stdio.h>

char** CommandLineParser::argv;
int CommandLineParser::curArgc;


/***********************************
  implementation of Context subclas
 ***********************************/
CommandLineParser::Context::Context(std::string contextName)
{
  name=contextName;
};


bool CommandLineParser::Context::isContext(char* contextName)
{
  //remove possible leading '-'-sign
  if ( *contextName == '-'){contextName++;};
  // check
  if (strcasecmp(name.c_str(),contextName)==0) return true;
  return false;
};

void CommandLineParser::Context::parse()
{
  Option* currentOption=getOption(argv[curArgc]);
  if (currentOption != NULL ){
    // add needed info to option;
    if (currentOption->timesUsed < currentOption->max){
      currentOption->timesUsed++;
      // add parameters if needed
      currentOption->parse();
    } else {
      //PRT_DEBUG("option "<<optionName<<" used more then " << currentOption->max << " times.");
    }
  } else {
    //PRT_DEBUG("Unknown option "<<std:string(optionName) <<" in "<<name<<" context");
  }
};

void CommandLineParser::Context::setFilename(std::string nameOfFile)
{
  filename=nameOfFile;
  used=true;
};

CommandLineParser:: Option* CommandLineParser::Context::getOption(char* nameOfOption)
{
	Option* currentOption=NULL;
	int nrOfDashes=0;
	
	//remove possible two leading '-'-sign
	if (*nameOfOption == '-' ){nameOfOption++;nrOfDashes++;};
	if (*nameOfOption == '-' ){nameOfOption++;nrOfDashes++;};

	for (std::list<Option*>::const_iterator i = options.begin(); i != options.end(); i++)
	{
	  std::string name=(nrOfDashes==2?(*i)->longName:(*i)->shortName);
	  int nameLength=name.length();
	  if ( 0==strncmp((*i)->longName.c_str(),nameOfOption,nameLength)){
	    currentOption=*i;
	    break;
	  };
	}
	return currentOption;
};

/**********************************
  implementation of Option subclas
 **********************************/

CommandLineParser::Option::Option(std::string shrt, std::string lng, int parameters, int maxUsage)
{
	shortName=shrt;
	longName=lng;
	nrParams=parameters;
	max=maxUsage;
};

bool CommandLineParser::Option::parse()
{
	char* optionName=argv[curArgc];
	int nrOfDashes=0;
	
	//remove possible two leading '-'-sign
	if (*optionName == '-' ){optionName++;nrOfDashes++;};
	if (*optionName == '-' ){optionName++;nrOfDashes++;};
	std::string name=(nrOfDashes==2?longName:shortName);
	int nameLength=name.length();

	if (strncasecmp(name.c_str(), optionName,nameLength) ){return false;};
	// this is the wanted option, get parameters if needed
	if (nrOfDashes==2){
        //TODO check for missing '=' in longoption
		for (; (*optionName) != '=' ;optionName++){};
		optionName++;
		parameters.push_back(std::string(optionName));
	} else {
		curArgc++;
		parameters.push_back(std::string(argv[curArgc]));
	};
	//TODO find out a 'standard' way to have multiple parameters with a '--*' option
	// or delete underlying code,preventing it for '-*' options as wel :-)
	/*
	for(int i=0; i<nrParams ; i++){
		data=argv[(*index)];
		parameters.push_back( data );
		(*index)++;
	};
	*/
	return true;

};


/*********************************************
  Implementation of CommandLineParser itself
 *********************************************/

CommandLineParser::CommandLineParser(int orgArgc, char **orgArgv)
{
	argc=orgArgc;
	argv=orgArgv;
};

/*
std::string CommandLineParser::checkFileType(){
  std::string fileType;
  if ( 0 == strcasecmp(parameter,"-config")) { fileType=1; };
  if ( 0 == strcasecmp(parameter,"-disk"))   { fileType=2; };
  if ( 0 == strcasecmp(parameter,"-diska"))  { fileType=2; diskLetter='A'; };
  if ( 0 == strcasecmp(parameter,"-diskb"))  { fileType=2; diskLetter='B'; };
  if ( 0 == strcasecmp(parameter,"-tape"))   { fileType=3; };
  if ( 0 == strcasecmp(parameter,"-cart"))   { fileType=4;  };
  if ( 0 == strcasecmp(parameter,"-carta"))  { fileType=4; cartridge=0; };
  if ( 0 == strcasecmp(parameter,"-cartb"))  { fileType=4; cartridge=1; };
  return fileType;
}
*/

std::string CommandLineParser::checkFileExt(char* filename){
  std::string fileType="config";
  char* Extension=filename + strlen(filename) -4;
  printf("Extension : %s\n",Extension);
  // check the extension caseinsensitive
  if (0 == strcasecmp(Extension,".xml")) fileType="config";
  if (	(0 == strcasecmp(Extension,".dsk")) ||
	(0 == strcasecmp(Extension,".di1")) ||
	(0 == strcasecmp(Extension,".di2")) ){
		fileType="diska";
		if ( (getContext(fileType))->used ){
			fileType="diskb";
		}
  };
  if (0 == strcasecmp(Extension,".cas")) fileType="tape";
  if (0 == strcasecmp(Extension,".rom")){
	fileType="carta";
	if ( (getContext(fileType))->used ){
		fileType="cartb";
	}
  };
  return fileType;
}


void CommandLineParser::populateContexts()
{
	Context* newContext;
	Option* newOption;


    // create a new context
	newContext=new Context(std::string("global"));
	  // generate new options for this context
	  newOption=new Option(std::string("config"),std::string("config"),1,9);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("v"),std::string("volume"),1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("msx1"),std::string("msx1"),1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("msx2"),std::string("msx2"),1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("msx2plus"),std::string("msx2plus"),1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("turboR"),std::string("turboR"),1,1);
	  newContext->options.push_back(newOption);
	contexts.push_back(newContext);

	// create a new context
	newContext=new Context(std::string("diska"));
	  // generate new options for this context
	  newOption=new Option(std::string("s"),std::string("size"),1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("f"),std::string("format"),1,1);
	  newContext->options.push_back(newOption);
	contexts.push_back(newContext);

	// create a new context
	newContext=new Context(std::string("diskb"));
	  // generate new options for this context
	  newOption=new Option(std::string("s"),std::string("size"),1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("f"),std::string("format"),1,1);
	  newContext->options.push_back(newOption);
	contexts.push_back(newContext);

	// create a new context
	newContext=new Context("carta");
	  // generate a new option for this context
	  newOption=new Option("m","mappertype",1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option("ps","pslot",1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option("ss","sslot",1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("v"),std::string("volume"),1,1);
	  newContext->options.push_back(newOption);
	contexts.push_back(newContext);

	// create a new context
	newContext=new Context("cas");
	  // generate a new option for this context
	  newOption=new Option("ro","readonly",1,1);
	  newContext->options.push_back(newOption);
	contexts.push_back(newContext);



	// create a new context
	newContext=new Context("cartb");
	  // generate a new option for this context
	  newOption=new Option("m","mappertype",1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option("ps","pslot",1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option("ss","sslot",1,1);
	  newContext->options.push_back(newOption);
	  newOption=new Option(std::string("v"),std::string("volume"),1,1);
	  newContext->options.push_back(newOption);
	contexts.push_back(newContext);


};

CommandLineParser::Context*	CommandLineParser::getContext(std::string name)
{
  Context* curCont = NULL;
    for (std::list<Context*>::const_iterator i = contexts.begin(); i != contexts.end(); i++){
      if (0==strcmp((*i)->name.c_str(), name.c_str()) ) {curCont=(*i);};
    };
  if (curCont){return curCont;};
  // a few special cases needed during parsing
  if (0==strcmp( name.c_str(),"disk")) {
    curCont=getContext(std::string("diska"));
    if ( curCont->used ){
      curCont=getContext(std::string("diskb"));
    }
    return curCont;
  }

  if (0==strcmp( name.c_str(),"cart")) {
    curCont=getContext(std::string("carta"));
    if ( curCont->used ){
      curCont=getContext(std::string("cartb"));
    }
    return curCont;
  }
  if (0==strcmp( name.c_str(),"conf")) {
    curCont=getContext(std::string("global"));
  }

  return curCont;
};

void CommandLineParser::configDisk(MSXConfig::Backend* config,std::string diskLetter,Context* curCont)
{
  std::ostringstream s;
  s << "<?xml version=\"1.0\"?>";
  s << "<msxconfig>";
  s << "<config id=\"diskpatch_disk"<<diskLetter<<"\">";
  s << "<type>disk</type>";
  s << "<parameter name=\"filename\">"<<(curCont->filename)<<"</parameter>";
  s << "<parameter name=\"readonly\">false</parameter>"; // TODO get from parameter
  s << "<parameter name=\"defaultsize\">720</parameter>"; // TODO get from parameter
  s << "</config>";
  s << "</msxconfig>";
  PRT_INFO(s.str());
  config->loadStream(s);
};

void CommandLineParser::configCart(MSXConfig::Backend* config,int primslot,Context* curCont)
{
  std::ostringstream s;
  int cartridgeNr=primslot;
  /*
  if (currentContext->getOption("-ps")){
    cartridgeNr=atoi((*(currentContext->getOption("-ps"))->parameters.begin()).c_str() );
  };
  */
  s << "<?xml version=\"1.0\"?>";
  s << "<msxconfig>";
  s << " <device id=\"GameCartridge"<< (int)cartridgeNr <<"\">";
  s << "  <type>GameCartridge</type><slotted><ps>";
  s << (int)(cartridgeNr) <<"</ps><ss>0</ss><page>1</page></slotted>";
  s << "<slotted><ps>";
  s << (int)(cartridgeNr) <<"</ps><ss>0</ss><page>2</page></slotted>";
  s << "<parameter name=\"filename\">";
  s << (currentContext->filename) << "</parameter>";
  s << "<parameter name=\"filesize\">0</parameter>";
  s << "<parameter name=\"volume\">13000</parameter>";
  s << "<parameter name=\"automappertype\">true</parameter>";
  s << "<parameter name=\"mappertype\">2</parameter>";
  s << " </device>";
  s << "</msxconfig>";
  PRT_INFO(s.str());
  config->loadStream(s);
  cartridgeNr++;
};

void CommandLineParser::parse(MSXConfig::Backend* config)
{
        int nrXMLfiles=0;
	char* tmpcharp=NULL;
        populateContexts();
        currentContext=getContext(std::string("global"));

        for (curArgc=1; curArgc<argc; curArgc++) {
          if ( *(argv[curArgc]) == '-' ){
            printf("need to parse regular option: %s\n",argv[curArgc]);
	    // context names do not start with '-' so we remove them
	    for (tmpcharp=argv[curArgc];*tmpcharp == '-';tmpcharp++){};
	    
            if ( getContext(std::string(tmpcharp)) != NULL ){
	      printf("explicit context change : %s\n",tmpcharp);
              currentContext=getContext(std::string(tmpcharp));
              curArgc++;
              currentContext->setFilename(std::string(argv[curArgc]));
            } else {
              currentContext->parse();
            }
          } else {
            // no '-' as first character so it is a filename :-)
            printf("need to parse filename option: %s\n",argv[curArgc]);
            currentContext=getContext(checkFileExt(argv[curArgc]));
            currentContext->setFilename(std::string(argv[curArgc]));
          };
        };

        // use all parsed contexts stuff here, the global context is to be used elsewhere
	currentContext=getContext(std::string("global"));
	if (currentContext->used){
	  //xml files
	  Option* optie=currentContext->getOption("-config");
	  for (std::list<std::string>::iterator i = optie->parameters.begin(); i != optie->parameters.end(); i++){
	    config->loadFile(*i);
	    nrXMLfiles++;
	  };
	  //check if msx1 specified
	  optie=currentContext->getOption("-msx1");
	  if (optie){
	    config->loadFile("msx1.xml");
	    nrXMLfiles++;
	  };
	  //check if msx2 specified
	  optie=currentContext->getOption("-msx2");
	  if (optie){
	    config->loadFile("msx2.xml");
	    nrXMLfiles++;
	  };
	  //check if msx2plus specified
	  optie=currentContext->getOption("-msx2plus");
	  if (optie){
	    config->loadFile("msx2plus.xml");
	    nrXMLfiles++;
	  };
	  //check if turboR specified
	  optie=currentContext->getOption("-turboR");
	  if (optie){
	    config->loadFile("turbor.xml");
	    nrXMLfiles++;
	  };

	};

	currentContext=getContext(std::string("cas"));
	if (currentContext->used){
              std::ostringstream s;
              s << "<?xml version=\"1.0\"?>";
              s << "<msxconfig>";
              s << " <config id=\"tapepatch\">";
              s << "  <type>tape</type>";
              s << "  <parameter name=\"filename\">";
              s << (currentContext->filename)<< "</parameter>";
              s << " </config>";
              s << "</msxconfig>";
              PRT_INFO(s.str());
              config->loadStream(s);
            }
	
	currentContext=getContext(std::string("diska"));
	if (currentContext->used){
		configDisk(config,std::string("A"),currentContext);
	};

	currentContext=getContext(std::string("diskb"));
	if (currentContext->used){
		configDisk(config,std::string("B"),currentContext);
	};
	
	currentContext=getContext(std::string("carta"));
	if (currentContext->used){
		configCart(config,1,currentContext);
	}
	currentContext=getContext(std::string("cartb"));
	if (currentContext->used){
		configCart(config,2,currentContext);
	}

	if (nrXMLfiles==0){
	  PRT_INFO ("Using msxconfig.xml as default configuration file");
	  config->loadFile(std::string("msxconfig.xml"));
	} 
}

#include "CommandLineParser.hh"
#include <string>
#include <sstream>
#include "MSXConfig.hh"
#include <stdio.h>

CommandLineParser* CommandLineParser::oneInstance = NULL;

CommandLineParser* CommandLineParser::instance()
{
  if (oneInstance == NULL) {
    oneInstance=new CommandLineParser();
  }; 
  return oneInstance;
}

CommandLineParser::CommandLineOption::CommandLineOption(std::string cliOption,bool usesParameter,std::string help)
{
	option=cliOption;
	used=false;
	hasParameter=usesParameter;
	helpLine=help;
};

CommandLineParser::CommandLineParser()
{
	nrXMLfiles=0;
	driveLetter='A';
	cartridgeNr=0;
	addOption(HELP,"-?/-h/-help",false,"Shows this text");
	addOption(MSX1,"-msx1",false,"Loads a default MSX 1 configuration");
	addOption(MSX2,"-msx2",false,"Loads a default MSX 2 configuration");
	addOption(MSX2PLUS,"-msx2plus",false,"Loads a default MSX 2 Plus configuration");
	addOption(TURBOR,"-turboR",false,"Loads a MSX Turbo R configuration");
	addOption(FMPAC,"-fmpac",false,"inserts an FM-PAC into the MSX machine");
	addOption(MUSMOD,"-musmod",false,"inserts a Philips Music Module (rom disabled)");
	addOption(MBSTEREO,"-mbstereo",false,"enables -fmpac and -musmod with stereo registration");
	//addOption(JOY,"-joy <type>",true,"plug a joystick in the emulated MSX (key,stick,mouse)");
}

char* CommandLineParser::getParameter(CLIoption id)
{
	return optionList[id]->parameter;
}

bool CommandLineParser::isUsed(CLIoption id)
{
	return optionList[id]->used;
}

void CommandLineParser::addOption(CLIoption id,std::string cliOption,bool usesParameter, std::string help)
{
  optionList.insert(
      std::pair<CLIoption,CommandLineParser::CommandLineOption*>
            ( id, new CommandLineOption(cliOption,usesParameter,help)  ) 
      );
}


void CommandLineParser::showHelp()
{
    PRT_INFO("OpenMSX command line options");
    PRT_INFO("============================\n\n");
    PRT_INFO("Normal usage of openMSX\n");
    PRT_INFO(" ./open [extra options] [[specifier] <filename>[,<extra mode>]]\n");
    PRT_INFO("specifier: normally the files are recognized by their extension ");
    PRT_INFO("           however it is possible to explicite state the files  ");
    PRT_INFO("           nature by using one of the folowing specifiers:      ");
    PRT_INFO("           -config,-disk,-diska,-diskb,-tape,-cart,-carta,-cartb");
    PRT_INFO("extra mode: * for diskimages                                    ");
    PRT_INFO("                ro : open read-only                             ");
    PRT_INFO("            * for cartridges you can specify it types           ");
    PRT_INFO("                8kB,16kB,SCC,KONAMI5,KONAMI4,ASCII8,ASCII16     ");
    PRT_INFO("                GAMEMASTER2,KONAMIDAC                           ");
    PRT_INFO("\nHere is the list of extra options: ");
    std::map<CLIoption,CommandLineParser::CommandLineOption*>::iterator it;
    for (it= optionList.begin(); it != optionList.end(); it++ ){
        PRT_INFO ( (*it).second->option <<"  \t: " << (*it).second->helpLine );
    }
}

int CommandLineParser::checkFileType(char* parameter,int &i, char **argv)
{
  int fileType=0;
  printf("parameter: %s\n",parameter);
  // this option does not impact the filetype of the next element :-)
  if ( (0 == strcasecmp(parameter,"-?")) || 
       (0 == strcasecmp(parameter,"-h")) ||
       (0 == strcasecmp(parameter,"-help")) ) { 
  		    showHelp();
		    exit(0);
		    };
  // now we see if there is a 'general' option that matches
  // and if it needs a parameter
  std::map<CLIoption,CommandLineParser::CommandLineOption*>::iterator it;
  for (it= optionList.begin(); it != optionList.end(); it++ ){
        if ( 0 == strcasecmp(parameter,(*it).second->option.c_str())){
		if ((*it).second->used){
		  PRT_INFO ("Parameter "<< (*it).second->option <<" specified twice\nAborting!!");
		  exit(1);
		}
		(*it).second->used=true;
		if ((*it).second->hasParameter){
			i++;
			(*it).second->parameter=argv[i];
		};
		break;
	}
  }

  // these options do impact the filetype the next element that will be parsed later on
  if ( 0 == strcasecmp(parameter,"-config")) { fileType=1; };
  if ( 0 == strcasecmp(parameter,"-disk"))   { fileType=2; };
  if ( 0 == strcasecmp(parameter,"-diska"))  { fileType=2; driveLetter='A'; };
  if ( 0 == strcasecmp(parameter,"-diskb"))  { fileType=2; driveLetter='B'; };
  if ( 0 == strcasecmp(parameter,"-tape"))   { fileType=3; };
  if ( 0 == strcasecmp(parameter,"-cart"))   { fileType=4; };
  if ( 0 == strcasecmp(parameter,"-carta"))  { fileType=4; cartridgeNr=0; };
  if ( 0 == strcasecmp(parameter,"-cartb"))  { fileType=4; cartridgeNr=1; };
  return fileType;
}

int CommandLineParser::checkFileExt(char* filename){
  int fileType=0;
  char* Extension=filename + strlen(filename);
  //trace back until first '.'
  for (;(*Extension!='.') && (Extension>filename);Extension--){};
  
  printf("Extension : %s\n",Extension);
  // check the extension caseinsensitive
  if (0 == strncasecmp(Extension,".xml",4)) fileType=1;
  if (0 == strncasecmp(Extension,".dsk",4)) fileType=2;
  if (0 == strncasecmp(Extension,".di1",4)) fileType=2;
  if (0 == strncasecmp(Extension,".di2",4)) fileType=2;
  if (0 == strncasecmp(Extension,".cas",4)) fileType=3;
  if (0 == strncasecmp(Extension,".rom",4)) fileType=4;
  if (0 == strncasecmp(Extension,".crt",4)) fileType=4;
  if (0 == strncasecmp(Extension,".ri",3)) fileType=4;
  return fileType;
}

void CommandLineParser::configureTape(char* filename)
{
  /*
  // The split to test for ",ro" isn't used for the moment since tapes are always
  // opened with openFilePreferRW
  char* file;
  char* readonly;
  for (readonly=file=filename ; (*readonly!=0) && (*readonly!=',') ; readonly++){};
  if (*readonly == ','){
	*(readonly++)=0;
  };
  */
  std::ostringstream s;
  s << "<?xml version=\"1.0\"?>";
  s << "<msxconfig>";
  s << " <config id=\"tapepatch\">";
  s << "  <type>tape</type>";
  s << "  <parameter name=\"filename\">";
  s << std::string(filename) << "</parameter>";
  s << " </config>";
  s << "</msxconfig>";
  PRT_INFO(s.str());
  config->loadStream(s);
}

void CommandLineParser::configureDisk(char* filename)
{
  char* file;
  char* readonly;
  for (readonly=file=filename ; (*readonly!=0) && (*readonly!=',') ; readonly++){};
  if (*readonly == ','){
	*(readonly++)=0;
  };
    std::ostringstream s;
    s << "<?xml version=\"1.0\"?>";
    s << "<msxconfig>";
    s << "<config id=\"diskpatch_disk"<< driveLetter <<"\">";
    s << "<type>disk</type>";
    s << "<parameter name=\"filename\">"<<std::string(file)<<"</parameter>";
    s << "<parameter name=\"readonly\">";
    if (*readonly == 0){
	s << "false";
    } else {
	s << "true";
    }
    s << "</parameter>";
    s << "<parameter name=\"defaultsize\">720</parameter>";
    s << "</config>";
    s << "</msxconfig>";
    PRT_INFO(s.str());
    config->loadStream(s);
    driveLetter++;
}

void CommandLineParser::configureCartridge(char* filename)
{
  char* file;
  char* mapper;
  for (mapper=file=filename ; (*mapper!=0) && (*mapper!=',') ; mapper++){};
  if (*mapper == ','){
	*(mapper++)=0;
  };
  std::ostringstream s;
  s << "<?xml version=\"1.0\"?>";
  s << "<msxconfig>";
  s << " <device id=\"GameCartridge"<< (int)cartridgeNr <<"\">";
  s << "  <type>GameCartridge</type><slotted><ps>";
  s << (int)(1+cartridgeNr) <<"</ps><ss>0</ss><page>1</page></slotted>";
  s << "<slotted><ps>";
  s << (int)(1+cartridgeNr) <<"</ps><ss>0</ss><page>2</page></slotted>";
  s << "<parameter name=\"filename\">";
  s << std::string(file) << "</parameter>";
  s << "<parameter name=\"filesize\">0</parameter>";
  s << "<parameter name=\"volume\">13000</parameter>";
  s << "<parameter name=\"automappertype\">";
  if (*mapper != 0){
     s << "false</parameter>";
     s << "<parameter name=\"mappertype\">"<<std::string(mapper)<<"</parameter>";
  } else {
     s << "true</parameter>";
  };
  s << " </device>";
  s << "</msxconfig>";
  PRT_INFO(s.str());
  config->loadStream(s);
  cartridgeNr++;
}

void CommandLineParser::configureMusMod(std::string mode)
{
  std::ostringstream s;
  s << "<?xml version=\"1.0\"?>";
  s << "<msxconfig>";
  s << " <device id=\"MSX-Audio\">";
  s << "  <type>Audio</type>";
  s << "<parameter name=\"volume\">20000</parameter>";
  s << "<parameter name=\"mode\">"<< mode <<"</parameter>";
  s << " </device>";
  s << "</msxconfig>";
  PRT_INFO(s.str());
  config->loadStream(s);
  cartridgeNr++;
}

void CommandLineParser::configureFmPac(std::string mode)
{
  std::ostringstream s;
  s << "<?xml version=\"1.0\"?>";
  s << "<msxconfig>";
  s << " <device id=\"FM PAC\">";
  s << "  <type>FM-PAC</type>";
  s << "<slotted><ps>2</ps><ss>0</ss><page>1</page></slotted>";
  s << "<parameter name=\"filename\">FMPAC.ROM</parameter>";
  s << "<parameter name=\"volume\">13000</parameter>";
  s << "<parameter name=\"mode\">"<< mode <<"</parameter>";
  s << "<parameter name=\"load\">true</parameter>";
  s << "<parameter name=\"save\">true</parameter>";
  s << "<parameter name=\"sramname\">FMPAC.PAC</parameter>";
  s << " </device>";
  s << "</msxconfig>";
  PRT_INFO(s.str());
  config->loadStream(s);
  cartridgeNr++;
}

void CommandLineParser::parse(MSXConfig::Backend* Backconfig,int argc, char **argv)
{
	    int i;
	    int fileType=0;

	    config=Backconfig;

	    for (i=1; i<argc; i++) {
	      // check the current option
	      if ( *(argv[i]) == '-' ){
		printf("need to parse general option\n");
		fileType=checkFileType(argv[i],i,argv);
		//if fileType has changed then the next MUST be a filename
		if (fileType !=0) i++;
	      } else {
		// no '-' as first character so it is a filename :-)
		// and we atoumagically determine the filetype
		//printf("need to parse filename option: %s\n",argv[i]);
		PRT_INFO("need to parse filename option: "<<argv[i]);
		fileType=checkFileExt(argv[i]);
	      };

	      // if there is a filetype known now then we parse the next argv
	      
	      switch (fileType){
		  case 0: //no filetype determined
		    break;
		  case 1: //xml files
		    config->loadFile(std::string(argv[i]));
		    nrXMLfiles++;
		    break;
		  case 2: //dsk,di2,di1 files
		    configureDisk(argv[i]);
		    break;
		  case 3: //cas files
		    configureTape(argv[i]);
		    break;
		  case 4: //rom files
		    configureCartridge(argv[i]);
		    break;
		  default:
		    PRT_INFO("Couldn't make sense of argument\n");
	      }
	      fileType=0;
	    }
	    //load "special" configuration files
	    if ( isUsed(MSX1)) { 
		    config->loadFile(std::string("msx1.xml"));
		    nrXMLfiles++;
		    };
	    if ( isUsed(MSX2)) { 
		    config->loadFile(std::string("msx2.xml"));
		    nrXMLfiles++;
		    };
	    if ( isUsed(MSX2PLUS)) { 
		    config->loadFile(std::string("msx2plus.xml"));
		    nrXMLfiles++;
		    };
	    if ( isUsed(TURBOR)) { 
		    config->loadFile(std::string("turbor.xml"));
		    nrXMLfiles++;
		    };
	  
	    if (nrXMLfiles==0){
	      PRT_INFO ("Using msxconfig.xml as default configuration file");
	      config->loadFile(std::string("msxconfig.xml"));
	    } 
	    if ( isUsed(FMPAC)) { 
	      // Alter subslotting if we need to insert fmpac 
	      configureFmPac(std::string("mono"));
	    }
	    if ( isUsed(MUSMOD)) { 
	      configureMusMod(std::string("mono"));
	    }
	    if ( isUsed(MBSTEREO)){
	      // Alter subslotting if we need to insert fmpac 
	      configureMusMod(std::string("right"));
	      configureFmPac(std::string("left"));
	    }
}

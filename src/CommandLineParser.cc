#include "CommandLineParser.hh"
#include <string>
#include <sstream>
#include "MSXConfig.hh"
#include <stdio.h>

int CommandLineParser::checkFileType(char* parameter,char &diskLetter,byte &cartridge){
  int fileType=0;
  printf("parameter: %s\n",parameter);
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

int CommandLineParser::checkFileExt(char* filename){
  int fileType=0;
  char* Extension=filename + strlen(filename) -4;
  printf("Extension : %s\n",Extension);
  // check the extension caseinsensitive
  if (0 == strcasecmp(Extension,".xml")) fileType=1;
  if (0 == strcasecmp(Extension,".dsk")) fileType=2;
  if (0 == strcasecmp(Extension,".di1")) fileType=2;
  if (0 == strcasecmp(Extension,".di2")) fileType=2;
  if (0 == strcasecmp(Extension,".cas")) fileType=3;
  if (0 == strcasecmp(Extension,".rom")) fileType=4;
  return fileType;
}

void CommandLineParser::parse(MSXConfig::Backend* config,int argc, char **argv)
{
	    int i;
	    int fileType=0;
	    int nrXMLfiles=0;
	    char driveLetter='A';
	    byte cartridgeNr=0;

	    for (i=1; i<argc; i++) {
	      if ( *(argv[i]) == '-' ){
		printf("need to parse general option\n");
		fileType=checkFileType(argv[i],driveLetter,cartridgeNr);
		//TODO if fileType has changed then the next MUST be a filename
	      } else {
		// no '-' as first character so it is a filename :-)
		printf("need to parse filename option: %s\n",argv[i]);
		if (fileType == 0) 
		  fileType=checkFileExt(argv[i]);
		switch (fileType){
		  case 1: //xml files
		    config->loadFile(std::string(argv[i]));
		    nrXMLfiles++;
		    break;
		  case 2: //dsk,di2,di1 files
		    {
		      std::ostringstream s;
		      s << "<?xml version=\"1.0\"?>";
		      s << "<msxconfig>";
		      s << "<config id=\"diskpatch_disk"<< driveLetter <<"\">";
		      s << "<type>disk</type>";
		      s << "<parameter name=\"filename\">"<<std::string(argv[i])<<"</parameter>";
		      s << "<parameter name=\"readonly\">false</parameter>";
		      s << "<parameter name=\"defaultsize\">720</parameter>";
		      s << "</config>";
		      s << "</msxconfig>";
		      PRT_INFO(s.str());
		      config->loadStream(s);
		      driveLetter++;
		    }
		    break;
		  case 3: //cas files
		    {
		      std::ostringstream s;
		      s << "<?xml version=\"1.0\"?>";
		      s << "<msxconfig>";
		      s << " <config id=\"tapepatch\">";
		      s << "  <type>tape</type>";
		      s << "  <parameter name=\"filename\">";
		      s << std::string(argv[i]) << "</parameter>";
		      s << " </config>";
		      s << "</msxconfig>";
		      PRT_INFO(s.str());
		      config->loadStream(s);
		    }
		    break;
		  case 4: //rom files
		    {
		      std::ostringstream s;
		      s << "<?xml version=\"1.0\"?>";
		      s << "<msxconfig>";
		      s << " <device id=\"GameCartridge"<< (int)cartridgeNr <<"\">";
		      s << "  <type>GameCartridge</type><slotted><ps>";
		      s << (int)(1+cartridgeNr) <<"</ps><ss>0</ss><page>1</page></slotted>";
		      s << "<slotted><ps>";
		      s << (int)(1+cartridgeNr) <<"</ps><ss>0</ss><page>2</page></slotted>";
		      s << "<parameter name=\"filename\">";
		      s << std::string(argv[i]) << "</parameter>";
		      s << "<parameter name=\"filesize\">0</parameter>";
		      s << "<parameter name=\"volume\">13000</parameter>";
		      s << "<parameter name=\"automappertype\">true</parameter>";
		      s << "<parameter name=\"mappertype\">2</parameter>";
		      s << " </device>";
		      s << "</msxconfig>";
		      PRT_INFO(s.str());
		      config->loadStream(s);
		      cartridgeNr++;
		    }
		    break;
		  default:
		    PRT_INFO("Couldn't make sense of argument\n");
		}
		fileType=0;
	      };
	    }
	  
	  if (nrXMLfiles==0){
	    PRT_INFO ("Using msxconfig.xml as default configuration file");
	    config->loadFile(std::string("msxconfig.xml"));
	  } 
}

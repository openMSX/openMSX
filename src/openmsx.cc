// $Id$

/*
 *  openmsx - Emulate the MSX standard.
 *
 *  Copyright (C) 2001 David Heremans
 */

#include "config.h"
#include "MSXConfig.hh"
#include <string>
#include <sstream>
#include <SDL/SDL.h>
#include "Thread.hh"
#include "MSXMotherBoard.hh"
#include "devicefactory.hh"
#include "EventDistributor.hh"
#include "EmuTime.hh"

#define __OPENMSX_CC__

#include "icon.nn"

void initializeSDL()
{
	if (SDL_Init(SDL_INIT_VIDEO)<0)
		PRT_ERROR("Couldn't init SDL: " << SDL_GetError());
	atexit(SDL_Quit);
	SDL_WM_SetCaption("openMSX " VERSION " [alpha]", 0);

	// Set icon
	static int iconRGBA[256];
	for (int i = 0; i < 256; i++) {
		iconRGBA[i] = iconColours[iconData[i]];
	}
	SDL_Surface *iconSurf = SDL_CreateRGBSurfaceFrom(
		iconRGBA, 16, 16, 32, 64,
		0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	SDL_SetColorKey(iconSurf, SDL_SRCCOLORKEY, 0);
	SDL_WM_SetIcon(iconSurf, NULL);
}

int checkFileType(char* parameter,char &diskLetter,byte &cartridge){
  int fileType=0;
  printf("parameter: %s\n",parameter);
  if ( 0 == strcmp(parameter,"-config")) { fileType=1; };
  if ( 0 == strcmp(parameter,"-disk"))   { fileType=2; };
  if ( 0 == strcmp(parameter,"-diska"))  { fileType=2; diskLetter='A'; };
  if ( 0 == strcmp(parameter,"-diskb"))  { fileType=2; diskLetter='B'; };
  if ( 0 == strcmp(parameter,"-tape"))   { fileType=3; };
  if ( 0 == strcmp(parameter,"-cart"))   { fileType=4;  };
  if ( 0 == strcmp(parameter,"-carta"))  { fileType=4; cartridge=0; };
  if ( 0 == strcmp(parameter,"-cartb"))  { fileType=4; cartridge=1; };
  return fileType;
}

int checkFileExt(char* filename){
  int fileType=0;
  char* Extension=filename + strlen(filename) -4;
  printf("Extension : %s\n",Extension);
  // check the extension caseinsensitive
  if (0 == strcmp(Extension,".xml")) fileType=1;
  if (0 == strcmp(Extension,".dsk")) fileType=2;
  if (0 == strcmp(Extension,".di1")) fileType=2;
  if (0 == strcmp(Extension,".di2")) fileType=2;
  if (0 == strcmp(Extension,".cas")) fileType=3;
  if (0 == strcmp(Extension,".rom")) fileType=4;
  return fileType;
}

int main (int argc, char **argv)
{
	// create configuration backend
	// for now there is only one, "xml" based
	MSXConfig::Backend* config = MSXConfig::Backend::createBackend("xml");
	
	try {
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
		    break;
		  case 3: //cas files
		    {
		      std::ostringstream s;
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

	  initializeSDL();

	  EmuTime zero;
	  config->initDeviceIterator();
	  MSXConfig::Device* d;
	  while ((d=config->getNextDevice()) != 0) {
	    //std::cout << "<device>" << std::endl;
	    //d->dump();
	    //std::cout << "</device>" << std::endl << std::endl;
	    MSXDevice *device = deviceFactory::create(d, zero);
	    MSXMotherBoard::instance()->addDevice(device);
	    PRT_DEBUG ("Instantiated: " << d->getType());
	  }

	  // Start a new thread for event handling
	  Thread thread(EventDistributor::instance());
	  thread.start();

	  PRT_DEBUG ("starting MSX");
	  MSXMotherBoard::instance()->StartMSX();

	  // When we return we clean everything up
	  thread.stop();
	  MSXMotherBoard::instance()->DestroyMSX();
	} 
	catch (MSXException& e) {
		PRT_ERROR("Uncatched exception: " << e.desc);
	}
	exit (0);
}


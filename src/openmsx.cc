/* 
   openmsx - Emulate the MSX standard.

   Copyright (C) 2001 David Heremans

*/

#include <stdio.h>
#include <sys/types.h>
#include "system.h"

#define EXIT_FAILURE 1

#include "msxconfig.hh"


//David Heremans
#include <iostream>
#include <fstream.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXZ80.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"
#include "devicefactory.hh"

//David Heremans

int
main (int argc, char **argv)
{
	char *configfile="msxconfig.xml";

	if (argc<2) {
		cout << "Using " << configfile << " as default configuration file." << endl;
	} else {
	configfile=argv[1];
	}
	// this is mainly for me (Joost), for testing the xml parsing
	try
	{
		MSXConfig::instance()->loadFile(configfile);
    	
        list<MSXConfig::Device*>::const_iterator i=MSXConfig::instance()->deviceList.begin();
    	for (; i != MSXConfig::instance()->deviceList.end(); i++)
    	{
    		(*i)->dump();
    	}
    }
	catch (MSXException e)
	{
		cerr << e.desc << endl;
		exit(1);
    }
	// End of Joosts test routine 
	// Here comes the Real Stuff(tm) :-)
	// (Actualy David's test stuff)
	


	//MSXMotherBoard* MSX;
	MSXMotherBoard* moederbord;
	MSXDevice* device;
	MSXDevice* geheugen;
	MSXRom16KB *rom1,*rom2;
	//MSXDevice* processor;
	MSXZ80* processor;

	//Werkt SDL ?
	if (SDL_Init(SDL_INIT_VIDEO)<0)
	{
	  printf("Couldn't init SDL: %s\n", SDL_GetError());
	  exit(1);
	}
	atexit(SDL_Quit);

	// Een moederbord als eerste
	moederbord=MSXMotherBoard::instance();
	// en dan nu al de devices die volgens de xml nodig zijn
	list<MSXConfig::Device*>::const_iterator j=MSXConfig::instance()->deviceList.begin();
	for (; j != MSXConfig::instance()->deviceList.end(); j++)
	{
		(*j)->dump();
		//TODO 
		//device=deviceFactory::create( (*j)->getType() );
		device=deviceFactory::create( (*j) );
		if (device != 0){
			moederbord->addDevice(device);
		cout << "---------------------------\nfactory:"<< (*j)->getType() << "\n\n";
		}
	}
	// dan maar de CPU
	processor=new MSXZ80();
	processor->Motherb=moederbord;
	moederbord->addDevice(processor);
	moederbord->setActiveCPU(processor);
	// dan geheugen
//	geheugen=new MSXSimple64KB();
//	moederbord->addDevice(geheugen);
//	moederbord->visibleDevices[2]=geheugen;
//	moederbord->visibleDevices[3]=geheugen;
	// dan rom 1
//	rom1=new MSXRom16KB();
//	strcpy(rom1->romfile,"BIOS.ROM");
//	moederbord->addDevice(rom1);
//	moederbord->visibleDevices[0]=rom1;
	// dan rom 2
//	rom2=new MSXRom16KB();
//	strcpy(rom2->romfile,"BASIC.ROM");
//	moederbord->addDevice(rom2);
//	moederbord->visibleDevices[1]=rom2;
	//moederbord->registerSlottedDevice(geheugen,0,0,0);
	//moederbord->registerSlottedDevice(geheugen,0,0,1);
//	moederbord->registerSlottedDevice(geheugen,0,0,2);
//	moederbord->registerSlottedDevice(geheugen,0,0,3);
	
	cout << "Initing MSX\n\n";
	moederbord->InitMSX();

	cout << "Initing MSX\n\n";
	moederbord->InitMSX();
	
	//Emutime dummy(DUMMY_FREQ, 100000);
	//moederbord->scheduler.insertStamp(dummy , *geheugen); //om toch een SP te hebben :-)
	cout << "starting MSX\n\n";
	moederbord->StartMSX();

	exit (0);
}

/*
static void
usage (int status)
{
  printf (_("%s - \
Emulate the MSX standard.\n"), program_name);
  printf (_("Usage: %s [OPTION]... [FILE]...\n"), program_name);
  printf (_("\
Options:
  -q, --quiet, --silent      inhibit usual output
  --verbose                  print more information
  -h, --help                 display this help and exit\n\
  -V, --version              output version information and exit\n\
"));
  exit (status);
}
*/

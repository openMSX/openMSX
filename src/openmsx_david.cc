#include <iostream>
#include <fstream.h>
#include <stdlib.h>
#include <string.h>
#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXZ80.hh"
#include "MSXSimple64KB.hh"
#include "MSXRom16KB.hh"

#include "SDL.h"

//using std::cout;
//using std::cerr;

int main(int argc, char **argv)
{
	//MSXMotherBoard* MSX;
	MSXMotherBoard* moederbord;
	MSXDevice* geheugen;
	MSXRom16KB *rom1,*rom2;
	//MSXDevice* processor;
	MSXZ80* processor;
	char *fileName;

	//Werkt SDL ?
	if (SDL_Init(SDL_INIT_VIDEO)<0)
	{
	  printf("Couldn't init SDL: %s\n", SDL_GetError());
	  exit(1);
	}
	atexit(SDL_Quit);

	// Een moederbord als eerste
	moederbord=MSXMotherBoard::instance();
	// dan maar de CPU
	processor=new MSXZ80();
	processor->Motherb=moederbord;
	moederbord->addDevice(processor);
	moederbord->setActiveCPU(processor);
	// dan geheugen
	geheugen=new MSXSimple64KB();
	moederbord->addDevice(geheugen);
	moederbord->visibleDevices[2]=geheugen;
	moederbord->visibleDevices[3]=geheugen;
	// dan rom 1
	rom1=new MSXRom16KB();
	strcpy(rom1->romfile,"BIOS.ROM");
	moederbord->addDevice(rom1);
	moederbord->visibleDevices[0]=rom1;
	// dan rom 2
	rom2=new MSXRom16KB();
	strcpy(rom2->romfile,"BASIC.ROM");
	moederbord->addDevice(rom2);
	moederbord->visibleDevices[1]=rom2;
	//moederbord->registerSlottedDevice(geheugen,0,0,0);
	//moederbord->registerSlottedDevice(geheugen,0,0,1);
	moederbord->registerSlottedDevice(geheugen,0,0,2);
	moederbord->registerSlottedDevice(geheugen,0,0,3);
	
	moederbord->InitMSX();
	
	moederbord->scheduler.setLaterSP(10000,geheugen); //om toch een SP te hebben :-)
	cout << "starting MSX\n\n";
	moederbord->StartMSX();
	fileName=argv[1];
	cout << "reading"<<fileName<<"\n";
	cout << "reading"<<argv[1] <<"\n";

//	ifstream fin(argv[1] );
//	if (!fin){	// open failed ??
//		std::cerr << "Couldn't open "<<fileName<<"\n";
//		return(1);
//	}
//	fin.close();

	
}


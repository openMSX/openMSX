// $Id$

#include "MSXGameCartridge.hh" 
#include "MSXMotherBoard.hh"
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"

MSXGameCartridge::MSXGameCartridge(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXGameCartridge object");
	
	//TODO  if needed return dynamically determine romSize
	romSize = deviceConfig->getParameterAsInt("filesize");
	loadUnknownFile(&memoryBank, romSize);
	
	// Calculate mapperMask
	int nrblocks = romSize>>13;	//number of 8kB pages
	for (mapperMask=1; mapperMask<nrblocks; mapperMask<<=1);
	mapperMask--;
	
	mapperType = retriefMapperType();
	//only instanciate SCC if needed
	if (mapperType==2){
	  short volume = (short)config->getParameterAsInt("volume");
	  cartridgeSCC=new SCC(volume);
	}
	if (mapperType==64){
	  short volume = (short)config->getParameterAsInt("volume");
	  dac=new DACSound(volume,16000,time);
	}


	reset(time);
}

MSXGameCartridge::~MSXGameCartridge()
{
	PRT_DEBUG("Destructing an MSXGameCartridge object");
	if (dac)
		delete dac;
	if (cartridgeSCC)
		delete cartridgeSCC;
}

void MSXGameCartridge::reset(const EmuTime &time)
{
  byte* ptr;
	if (cartridgeSCC){
	  cartridgeSCC->reset();
	}
	if (dac){
	  dac->reset(time);
	}
	if (mapperType < 128 ){
	  // set internalMemoryBank
	  // TODO: mirror if number of 8kB blocks not fully filled ?
	  internalMemoryBank[0]=0;		// unused
	  internalMemoryBank[1]=0;		// unused
	  internalMemoryBank[2]=memoryBank;	// 0x4000 - 0x5fff
	  internalMemoryBank[3]=memoryBank+0x2000;// 0x6000 - 0x7fff
	  internalMemoryBank[4]=memoryBank+0x4000;// 0x8000 - 0x9fff
	  internalMemoryBank[5]=memoryBank+0x6000;// 0xa000 - 0xbfff
	  internalMemoryBank[6]=0;		// unused
	  internalMemoryBank[7]=0;		// unused
	} else {
	  // this is a simple gamerom less then 64 kB
	  switch (romSize>>14){ // blocks of 16kB
	    case 0:
	      // An 8 Kb game ????
	      for (int i=0;i<8;i++){
		internalMemoryBank[i]=memoryBank;
	      }
	      break;
	    case 1:
	      for (int i=0;i<8;i+=2){
		internalMemoryBank[i]=memoryBank;
		internalMemoryBank[i+1]=memoryBank+0x2000;
	      }
	      break;
	    case 2:
	      internalMemoryBank[0]=memoryBank;		// 0x0000 - 0x1fff
	      internalMemoryBank[1]=memoryBank+0x2000;	// 0x2000 - 0x3fff
	      internalMemoryBank[2]=memoryBank;		// 0x4000 - 0x5fff
	      internalMemoryBank[3]=memoryBank+0x2000;	// 0x6000 - 0x7fff
	      internalMemoryBank[4]=memoryBank+0x4000;	// 0x8000 - 0x9fff
	      internalMemoryBank[5]=memoryBank+0x6000;	// 0xa000 - 0xbfff
	      internalMemoryBank[6]=memoryBank+0x4000;	// 0xc000 - 0xdfff
	      internalMemoryBank[7]=memoryBank+0x6000;	// 0xe000 - 0xffff
	      break;
	    case 4:
	      ptr=memoryBank;
	      for (int i=0;i<8;i++,ptr+=0x2000){
	        internalMemoryBank[i]=ptr;
	      }
	      break;
	    default: 
	      // not possible
	      assert (false);
	  }
	}
	
}

int MSXGameCartridge::retriefMapperType()
{
	unsigned int typeGuess[]={0,0,0,0,0,0};

	//TODO make configurable or use 'auto'
	//if (deviceConfig->getParameter("mappertype")){
	//  return atoi(deviceConfig->getParameter("mappertype").c_str());
	//};

	//  GameCartridges do their bankswitching by using the Z80 instruction ld(nn),a in 
	//  the middle of program code. The adress nn depends upon the GameCartridge mappertype used
	//
	//  To gues which mapper it is, we will look how much writes with this 
	//  instruction to the mapper-registers-addresses occure.

	// if smaller then 32kB it must be a simple rom so we return 128
	if  (deviceConfig->getParameterAsBool("automappertype")){
	  if (romSize <=0xFFFF){
	    if (romSize == 0x8000){
	      //TODO: Autodetermine for KonamiSynthesiser
	      // works for now since most 32 KB cartridges don't write to themselves
	      return 64;
	    } else {
	      return 128;
	    }
	  }
	
	  for (int i=0; i<romSize-2; i++) {
		if (memoryBank[i] == 0x32) {
			int value = memoryBank[i+1]+(memoryBank[i+2]<<8);
			switch (value){
			case 0x5000:
			case 0x9000:
			case 0xB000:
				typeGuess[2]++;
				break;
			case 0x4000:
			case 0x8000:
			case 0xA000:
				typeGuess[3]++;
				break;
			case 0x6800:
			case 0x7800:
				typeGuess[4]++;
				break;
			case 0x6000:
				typeGuess[3]++;
				typeGuess[4]++;
				typeGuess[5]++;
				break;
			case 0x7000:
				typeGuess[2]++;
				typeGuess[4]++;
				typeGuess[5]++;
				break;
			case 0x77FF:
				typeGuess[5]++;
			}
		}
	  }
	  // in case of doubt we go for type 0
	  typeGuess[0]++;
	  // in case of even type 5 and 4 we would prefer 5 
	  // but we would still prefer 0 above 4 or 5 so no increment
	  if (typeGuess[4]) typeGuess[4]--; // decrement of zero is max_int :-)
	  int type = 0;
	  for (int i=0; i<6; i++) {
	  	if (typeGuess[i]>typeGuess[type]) 
	  		type = i;
	  }
	  PRT_DEBUG("I Guess this is a nr. " << type << " GameCartridge mapper type.")
	    return type;
	} else {
	  PRT_DEBUG("Configured as a nr. " << deviceConfig->getParameterAsInt("mappertype") << " GameCartridge mapper type.")
	  return deviceConfig->getParameterAsInt("mappertype");
	};
	
}

byte MSXGameCartridge::readMem(word address, const EmuTime &time)
{
	//TODO optimize this!!!
  byte regio;
  if ( (mapperType == 2) && enabledSCC ){
    regio=(address-0x5000)>>13;
    if ( (regio==2) ){
      return cartridgeSCC->readMemInterface(address & 0xFF , time );
    }
  }
  return internalMemoryBank[(address>>13)][address&0x1fff];
}

void MSXGameCartridge::writeMem(word address, byte value, const EmuTime &time)
{
	byte regio;
  
	switch (mapperType) {
	case 0: 
		//TODO check the SCC in here, is this correct for this type?
		// if so, implement writememinterface
		// also in the memRead

		//--==>> Generic 8kB cartridges <<==--
		if ((address<0x4000) || (address>0xBFFF))
			return;
		if ((address&0xe000) == 0x8000 ) {
			// 0x8000..0x9FFF is used as SCC switch
			enabledSCC=((value&0x3F)==0x3F)?true:false;
		}
		// change internal mapper
		value &= mapperMask;
		regio = (address>>13);	// 0-7
		internalMemoryBank[regio] = memoryBank+(value<<13);
		break;
	case 1: 
		//--==**>> Generic 16kB cartridges (MSXDOS2, Hole in one special) <<**==--
		if ((address<0x4000) || (address>0xBFFF))
			return;
		regio = (address&0xc000)>>13;	// 0, 2, 4, 6
		value &= (2*value)&mapperMask;
		internalMemoryBank[regio]   = memoryBank+(value<<13);
		internalMemoryBank[regio+1] = memoryBank+(value<<13)+0x2000;
		break;
	case 2: 
		//--==**>> KONAMI5 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do have an SCC and some others
		// example of catrridges: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow
		// Solid Snake, Quarth, Ashguine 1, Animal, Arkanoid 2, ...
		// 
		// The address to change banks:
		//  bank 1: 0x5000 - 0x57FF (0x5000 used)
		//  bank 2: 0x7000 - 0x77FF (0x7000 used)
		//  bank 3: 0x9000 - 0x97FF (0x9000 used)
		//  bank 4: 0xB000 - 0xB7FF (0xB000 used)
      
		if ((address<0x5000) || (address>0xc000) )
			return;
		// Enable/disable SCC
		if ((address&0xF800) == 0x9000 )  {
			// 0x9000-0x97ff is used as SCC switch
			enabledSCC = ((value&0x3F)==0x3F) ? true : false;
			if (enabledSCC)
				// TODO find out how it changes the page in  0x9000-0x97ff
				return; 
		}
		// change internal mapper
		regio = (address-0x1000)>>13;
		if ( (regio==4) && enabledSCC && (address >= 0x9800)){
		  cartridgeSCC->writeMemInterface(address & 0xFF, value , time);
		  PRT_DEBUG("GameCartridge: writeMemInterface (" << address <<","<<(int)value<<")"<<time );
		} else {
		  if ((address & 0x1800)!=0x1000) return;
		  value &= mapperMask;
		  internalMemoryBank[regio] = memoryBank+(value<<13);
		  //internalMapper[regio]=value;
		}
		break;
	case 3: 
		//--==**>> KONAMI4 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do not have an SCC and some others
		// example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom, 
		// The Maze of Galious, Aleste 1, 1942,Heaven, Mystery World, ...
		//
		// page at 4000 is fixed, other banks are switched
		// by writting at 0x6000,0x8000 and 0xA000
		 
		if ((address<0x6000) || (address>0xa000) || (address&0x1FFF))
			return;
		regio = (address>>13);
		value &= mapperMask;
		internalMemoryBank[regio] = memoryBank+(value<<13);
		break;
	case 4: 
		//--==**>> ASCII 8kB cartridges <<**==--
		// this type is used in many japanese-only cartridges.
		// example of cartridges: Valis(Fantasm Soldier), Dragon Slayer, Outrun, Ashguine 2, ...
		//
		// The address to change banks:
		//  bank 1: 0x6000 - 0x67FF (0x5000 used)
		//  bank 2: 0x6800 - 0x6FFF (0x7000 used)
		//  bank 3: 0x7000 - 0x77FF (0x9000 used)
		//  bank 4: 0x7800 - 0x7FFF (0xB000 used)
      
		if ((address<0x6000) || (address>0x7fff))
			return;
		regio = ((address>>11)&3)+2;
		value &= mapperMask;
		internalMemoryBank[regio] = memoryBank+(value<<13);
		break;
	case 5:
		//--==**>> ASCII 16kB cartridges <<**==--
		// this type is used in a few cartridges.
		// example of cartridges: Xevious, Fantasy Zone 2, Return of Ishitar, Androgynus, ...
		//
		// Gallforce is a special case after a reset the second 16kb has to start with 
		// the first 16kb after a reset
		//
		// The address to change banks:
		//  first  16kb: 0x6000 - 0x67FF (0x6000 used)
		//  second 16kb: 0x7000 - 0x77FF (0x7000 and 0x77FF used)
      
		if ((address<0x6000) || (address>0x77ff) || (address&0x800))
			return;
		regio = ((address>>11)&2)+2;
		value &= (2*value)&mapperMask;
		internalMemoryBank[regio]   = memoryBank+(value<<13);
		internalMemoryBank[regio+1] = memoryBank+(value<<13)+0x2000;
		break;
	case 6: 
		//--==**>> GameMaster2+SRAM cartridge <<**==--
		//// GameMaster2 mayi become an independend MSXDevice
		assert(false);
		break;
	case 64:
		//--==**>> <<**==--
		// Konami Synthezier cartridge
		// Should be only for 0x4000, but since this isn't confirmed on a real
		// cratridge we will simply use every write.
		dac->writeDAC(value,time);
		
	case 128: 
		//--==**>> Simple romcartridge <= 64 KB <<**==--
		// No extra writable hardware
		break;
	default:
		//// Unknow mapper type for GameCartridge cartridge!!!
		assert(false);
	}
}

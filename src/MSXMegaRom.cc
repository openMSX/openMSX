// $Id$

#include "MSXMegaRom.hh" 
#include "MSXMotherBoard.hh"
#include <string>
#include <iostream>
#include <fstream>

#include "config.h"

MSXMegaRom::MSXMegaRom(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXMegaRom object");
}

MSXMegaRom::~MSXMegaRom()
{
	delete [] memoryBank; // C++ can handle null-pointers
	PRT_DEBUG("Destructing an MSXMegaRom object");
}

void MSXMegaRom::init()
{
	int offset,nrblocks;
	MSXDevice::init();
	
	std::string filename=deviceConfig->getParameter("romfile");
	offset = atoi(deviceConfig->getParameter("skip_headerbytes").c_str());
	ROM_SIZE = atoi(deviceConfig->getParameter("filesize").c_str());
	
	// allocate buffer
	memoryBank=new byte[ROM_SIZE];
	if (memoryBank == NULL)
		PRT_ERROR("Couldn't create megarom bank !!!!!!");
	
	// read the rom file
#ifdef HAVE_FSTREAM_TEMPL
	std::ifstream<byte> file(filename.c_str());
#else
	std::ifstream file(filename.c_str());
#endif
	//TODO dynamically determine ROM_SIZE
	//file.seekg(0,end);
	//ROM_SIZE=-offset;
	file.seekg(offset);
	file.read(memoryBank, ROM_SIZE);
	if (file.fail())
		PRT_ERROR("Error reading " << filename);
	// TODO: maybe check for AB signature ?
	
	// Calculate mapperMask
	nrblocks=ROM_SIZE>>13;  //number of 8kB pages
	for (mapperMask=1;mapperMask<nrblocks;mapperMask<<=1);
	mapperMask--;
	
	// TODO: mirror if number of 8kB blocks not fully filled ?
	
	// set internalMemoryBank
	internalMemoryBank[0]=memoryBank;
	internalMemoryBank[1]=memoryBank+0x2000;
	internalMemoryBank[2]=memoryBank+0x4000;
	internalMemoryBank[3]=memoryBank+0x6000;


	
	mapperType=retriefMapperType();

	
	// register in slot-structure
	std::list<MSXConfig::Device::Slotted*>::const_iterator i;
	for (i=deviceConfig->slotted.begin(); i!=deviceConfig->slotted.end(); i++) {
		int ps=(*i)->getPS();
		int ss=(*i)->getSS();
		int page=(*i)->getPage();
		MSXMotherBoard::instance()->registerSlottedDevice(this,ps,ss,page);
	}

}

int MSXMegaRom::retriefMapperType()
{
  unsigned int i,value;
  unsigned int typeGuess[]={0,0,0,0,0,0};

  //TODO make configurable or use 'auto'
  //if (deviceConfig->getParameter("mappertype")){
  //  return atoi(deviceConfig->getParameter("mappertype").c_str());
  //};

  /*
     MegaRoms do their bankswitching by using the Z80 instruction ld(nn),a in 
     the middle of program code. The adress nn depends upon the megarom mappertype used

     To gues which mapper it is, we will look how much writes with this 
     instruction to the mapper-registers-addresses occure.
   */
  for (i=0;i<ROM_SIZE-2;i++){
    if (memoryBank[i] == 0x32){
      value=memoryBank[i+1]+(memoryBank[i+2]<<8);
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
  for (i=0,value=0;i<6;i++){
    if (typeGuess[i]>typeGuess[value]) value=i;
  }
  PRT_DEBUG("I Guess this is a nr. " << value << " megarom mapper type.")
  return(value);
}

byte MSXMegaRom::readMem(word address, Emutime &time)
{
  return internalMemoryBank[(address>>13)-2][address & 0x1fff];
}

void MSXMegaRom::writeMem(word address, byte value, Emutime &time)
{
  byte regio;
  
  // Enable/disable SCC
  // TODO: make this only working for empty cartridge or correct mapperType
  // when done you could simply return from this case
  if (address == 0x9000){
    enabledSCC=(value==0x3F)?true:false;
    //TODO: (un)register as sound device :-)
  }

  // Write to SCC register
  if ( enabledSCC && ((address & 0xFF00) == 0X9800)){
    //cartridgeSCC((addr&0x7F),value); //TODO check if bit 7 as influence 
    return;
  }

  // If no cartridge or no mapper return
  if (memoryBank == 0)  return;


  switch (mapperType){
    case 0: //   --==>> Generic 8kB cartridges <<==--
      if ((address < 0x4000) || (address >0xBFFF)) return;
      if ((address & 0xe000) == 0x8000 ) { // 0x8000..0x9FFF is used as SCC switch
	enabledSCC=((value&0x3F)==0x3F)?true:false;
      }
      /* change internal mapper*/
      value&=mapperMask;
      regio=(address>>13)-2;
      internalMemoryBank[regio]=memoryBank+(value<<13);
      internalMapper[regio]=value;
      break;
    case 1: //   --==**>> Generic 16kB cartridges (MSXDOS2, Hole in one special) <<**==--
      if ((address < 0x4000) || (address >0xBFFF)) return;
      regio=(address & 0x8000 ) >>14;
      value&=(value<<1)&mapperMask;
      internalMemoryBank[regio]=memoryBank+(value<<13);
      internalMemoryBank[regio+1]=internalMemoryBank[regio]+0x2000; //TODO check if fMSX is right?
      internalMapper[regio]=value;
      break;
    case 2: //   --==**>> KONAMI5 8kB cartridges <<**==--
      /* this type is used by Konami cartridges that do have an SCC and some others
      * example of catrridges: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow
      * Solid Snake, Quarth, Ashguine 1, Animal, Arkanoid 2, ...
      */
      
      /* The address to change banks:
       bank 1: 0x5000 - 0x57FF (0x5000 used)
       bank 2: 0x7000 - 0x77FF (0x7000 used)
       bank 3: 0x9000 - 0x97FF (0x9000 used)
       bank 4: 0xB000 - 0xB7FF (0xB000 used)
      */
      if ((address < 0x5000) || (address >0xB000) || ((address & 0x1800)!=0x1000)) return;
      // Enable/disable SCC
      if ((address & 0xF800) == 0x9000 )  { // 0x9000-0x97ff is used as SCC switch
	enabledSCC=((value&0x3F)==0x3F)?true:false;
      }
      /* change internal mapper*/
      regio=(address-0x5000)>>13;
      value&=mapperMask;
      internalMemoryBank[regio]=memoryBank+(value<<13);
      internalMapper[regio]=value;
      break;
    case 3: //   --==**>> KONAMI4 8kB cartridges <<**==--
      /* this type is used by Konami cartridges that do not have an SCC and some others
      * example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom, 
      * The Maze of Galious, Aleste 1, 1942,Heaven, Mystery World, ...
      */
      
      /* page at 4000 is fixed, other banks are switched
         by writting at 0x6000,0x8000 and 0xA000 */
      if ((address < 0x6000) || (address >0xa000) || (address & 0x1FFF)) return;
      regio=(address>>13)-2;
      value&=mapperMask;
      internalMemoryBank[regio]=memoryBank+(value<<13);
      internalMapper[regio]=value;
      break;
    case 4: //   --==**>> ASCII 8kB cartridges <<**==--
      /* this type is used in many japanese-only cartridges.
      * example of cartridges: Valis(Fantasm Soldier), Dragon Slayer, Outrun, Ashguine 2, ...
      */
      
      /* The address to change banks:
       bank 1: 0x6000 - 0x67FF (0x5000 used)
       bank 2: 0x6800 - 0x6FFF (0x7000 used)
       bank 3: 0x7000 - 0x77FF (0x9000 used)
       bank 4: 0x7800 - 0x7FFF (0xB000 used)
      */
      if ((address < 0x6000) || (address >0x7fff)) return;
      regio=(address>>11)&3;
      value&=mapperMask;
      internalMemoryBank[regio]=memoryBank+(value<<13);
      internalMapper[regio]=value;
      break;
    case 5: //   --==**>> ASCII 16kB cartridges <<**==--
      /* this type is used in a few cartridges.
      * example of cartridges: Xevious, Fantasy Zone 2, Return of Ishitar, Androgynus, ...
      *
      * Gallforce is a special case after a reset the second 16kb has to start with 
      * the first 16kb after a reset
      *
      */
      
      /* The address to change banks:
       first  16kb: 0x6000 - 0x67FF (0x6000 used)
       second 16kb: 0x7000 - 0x77FF (0x7000 and 0x77FF used)
      */
      if ((address < 0x6000) || (address >0x77ff) || (address&0x800)) return;
      regio=(address>>11)&2;
      value&=(value<<1)&mapperMask;
      internalMemoryBank[regio]=memoryBank+(value<<13);
      internalMemoryBank[regio+1]=internalMemoryBank[regio]+0x2000;
      internalMapper[regio]=value;
      break;
    case 6: //   --==**>> GameMaster2+SRAM cartridge <<**==--
      PRT_DEBUG("GameMaster2 must become an independend MSXDevice");
      assert(1);
      break;
    default:
      PRT_DEBUG("Unknow mapper type for megarom cartridge!!!");
      assert(1);
  }

}

#include "MSXDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXPPI.hh"
#include <SDL/SDL.h>

//#include "string.h"
//#include "stdlib.h"

MSXPPI *volatile MSXPPI::oneInstance;

MSXPPI::MSXPPI()
{
	cout << "Creating an MSXPPI object \n";
}

MSXPPI::~MSXPPI()
{
	cout << "Destroying an MSXPPI object \n";
}

MSXDevice* MSXPPI::instance(void)
{
	if (oneInstance == NULL ){
		oneInstance == new MSXPPI;
	};
	return oneInstance;
}

void MSXPPI::InitMSX()
{
  // Register I/O ports A8..AB for reading
	MSXMotherBoard::instance()->register_IO_In(0xA8,this);
	MSXMotherBoard::instance()->register_IO_In(0xA9,this);
	MSXMotherBoard::instance()->register_IO_In(0xAA,this);
	MSXMotherBoard::instance()->register_IO_In(0xAB,this);
	// AB is writeonly but port reservation to prevent other devices :-/

	// Register I/O ports A8..AB for writing
	MSXMotherBoard::instance()->register_IO_Out(0xA8,this);
	MSXMotherBoard::instance()->register_IO_Out(0xA9,this); 
	// A9 is readonly but port reservation to prevent other devices :-/
	MSXMotherBoard::instance()->register_IO_Out(0xAA,this);
	MSXMotherBoard::instance()->register_IO_Out(0xAB,this);
}

byte MSXPPI::readIO(byte port,UINT64 TStates)
{
  switch( port ){
	case 0xA8:
		return MSXMotherBoard::instance()->A8_Register;
		break;
	case 0xA9:
		break;
	case 0xAA:
		break;
	case 0xAB:
		break;
  }
}

void MSXPPI::writeIO(byte port,byte value,UINT64 TStates)
{
  int J;

  switch( port ){
	case 0xA8:
	  if ( value != MSXMotherBoard::instance()->A8_Register){
	    //TODO: do slotswitching if needed
	    MSXMotherBoard::instance()->A8_Register=value;
	    for (J=0;J<4;J++,value>>=2){
	      // Change the slot structure
	      MSXMotherBoard::instance()->PrimarySlotState[J]=value&3;
	      MSXMotherBoard::instance()->SecondarySlotState[J]= 
		3&(MSXMotherBoard::instance()->SubSlot_Register[value&3]>>(J*2));
	      // Change the visible devices
	      MSXMotherBoard::instance()->visibleDevices[J]=
		MSXMotherBoard::instance()->SlotLayout
		[MSXMotherBoard::instance()->PrimarySlotState[J]]
		[MSXMotherBoard::instance()->SecondarySlotState[J]]
		[J];
	    }
	  }
	  break;
	case 0xA9:
		break;
	case 0xAA:
		break;
	case 0xAB:
		break;
  }
}

/** Keyboard bindings ****************************************/
byte MSXPPI::Keys[336][2] =
{
/* 0000 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {7,0x20},{7,0x08},{0,0x00},{0,0x00},{0,0x00},{7,0x80},{0,0x00},{0,0x00},
/* 0010 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{7,0x04},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0020 */
  {8,0x01},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{2,0x01},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{2,0x04},{1,0x04},{2,0x08},{2,0x10},
/* 0030 */
  {0,0x01},{0,0x02},{0,0x04},{0,0x08},{0,0x10},{0,0x20},{0,0x40},{0,0x80},
  {1,0x01},{1,0x02},{0,0x00},{1,0x80},{0,0x00},{1,0x08},{0,0x00},{0,0x00},
/* 0040 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0050 */
  {0,0x00},{8,0x10},{8,0x20},{8,0x80},{8,0x40},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{1,0x20},{1,0x10},{1,0x40},{0,0x00},{0,0x00},
/* 0060 */
  {2,0x02},{2,0x40},{2,0x80},{3,0x01},{3,0x02},{3,0x04},{3,0x08},{3,0x10},
  {3,0x20},{3,0x40},{3,0x80},{4,0x01},{4,0x02},{4,0x04},{4,0x08},{4,0x10},
/* 0070 */
  {4,0x20},{4,0x40},{4,0x80},{5,0x01},{5,0x02},{5,0x04},{5,0x08},{5,0x10},
  {5,0x20},{5,0x40},{5,0x80},{0,0x00},{0,0x00},{0,0x00},{6,0x04},{8,0x08},
/* 0080 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0090 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00A0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00B0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00C0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00D0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {8,0x02},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00E0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 00F0 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0100 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0110 */
  {0,0x00},{8,0x20},{8,0x40},{8,0x80},{8,0x10},{8,0x04},{8,0x02},{0,0x00},
  {0,0x00},{0,0x00},{6,0x20},{6,0x40},{6,0x80},{7,0x01},{7,0x02},{0,0x00},
/* 0120 */
  {7,0x40},{7,0x10},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{6,0x01},
/* 0130 */
  {6,0x01},{2,0x20},{6,0x02},{6,0x10},{6,0x04},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
/* 0140 */
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},
  {0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00},{0,0x00}
};

/** Keyboard() ***********************************************/
/** Check for keyboard events, parse them, and modify MSX   **/
/** keyboard matrix.                                        **/
/*************************************************************/
void MSXPPI::Keyboard(void)
{

  SDL_Event event;
  static byte Control=0;
  int key,I;

  /* Check for keypresses/keyreleases */
  while(SDL_PollEvent(&event))
  {
	 key=event.key.keysym.sym;

     //if(event.type==SDL_QUIT) ExitNow=1;

     if(event.type==SDL_KEYDOWN)
	 {
//	   switch(key)
//	   {
//#ifdef DEBUG
//         case SDLK_F9:
//		   CPU.Trace=!CPU.Trace;
//		   break;
//#endif
//
//	 case SDLK_F6:
//           if(Control) AutoFire=!AutoFire;
//           break;
//
//         case SDLK_F10:
//         case SDLK_F11:
//           if(Control)
//		   {
//             I=(key==SDLK_F11)? 1:0;
//             if(Disks[I][0])
//			 {
//               CurDisk[I]=(CurDisk[I]+1)%MAXDISKS;
//			   if(!Disks[I][CurDisk[I]]) CurDisk[I]=0;
//			   ChangeDisk(I,Disks[I][CurDisk[I]]);
//			 }
//		   }
//           break;
//
//         case SDLK_F12:
//           ExitNow=1;
//           break;
//
//         case SDLK_UP:    JoyState|=0x01;break;
//         case SDLK_DOWN:  JoyState|=0x02;break;
//         case SDLK_LEFT:  JoyState|=0x04;break;
//         case SDLK_RIGHT: JoyState|=0x08;break;
//         case SDLK_LALT:  JoyState|=0x10;break;
//         case SDLK_LCTRL:
//           if(key==SDLK_LCTRL) JoyState|=0x20;
//             Control=1;
//             break;
//	   }
//
       /* Key pressed: reset bit in KeyMap */
       if(key<0x150) MSXKeyMatrix[Keys[key][0]]&=~Keys[key][1];
	 }
	 
	 if(event.type==SDL_KEYUP)
	 {	
       /* Special keys released... */
//       switch(key)
//	   {
//         case SDLK_UP:    JoyState&=0xFE;break;
//         case SDLK_DOWN:  JoyState&=0xFD;break;
//         case SDLK_LEFT:  JoyState&=0xFB;break;
//         case SDLK_RIGHT: JoyState&=0xF7;break;
//         case SDLK_LALT:  JoyState&=0xEF;break;
//         case SDLK_LCTRL:
//           if(key==SDLK_LCTRL) JoyState&=0xDF;
//             Control=0;
//             break;
//	   }
//
	   /* Key released: set bit in KeyMap */
       if(key<0x150) MSXKeyMatrix[Keys[key][0]]|=Keys[key][1];
	 }

  }

  /* Keyboard handling is done once per screen update */
  /* That's whay we wait here for the timer */
 // WaitTimer();

}

/** SetColor() ***********************************************/
/** Set color N (0..15) to R,G,B.                           **/
/*************************************************************/
//void SetColor(register byte N,register byte R,register byte G,register byte B)
//{
//  unsigned int J;
//
//  J=SDL_MapRGB(screen->format,R,G,B);
//
//  if(N) XPal[N]= J; else XPal0=J;
//}

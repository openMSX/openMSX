// $Id$

#include "Inputs.hh"
#include "MSXPPI.hh"
#include <SDL/SDL.h>

//#include "string.h"
//#include "stdlib.h"

MSXPPI *volatile MSXPPI::oneInstance;

MSXPPI::MSXPPI()
{
	cout << "Creating an MSXPPI object \n";
	real_MSX_keyboard=1;
}

MSXPPI::~MSXPPI()
{
	cout << "Destroying an MSXPPI object \n";
}

MSXDevice* MSXPPI::instance(void)
{
	if (oneInstance == NULL ){
		oneInstance = new MSXPPI;
	};
	return oneInstance;
}

void MSXPPI::init()
{	
	// in most cases just one PPI so we register permenantly with 
	// Inputs object for now.
	//
	// The Sunrise Dual Headed MSX experiment had two!
	// Recreating this would mean making a second PPI instance by means
	// of a derived class (MSXPPI is singleton !!)
	Inputs::instance()->setPPI(this);

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

byte MSXPPI::readIO(byte port, Emutime &time)
{
  switch( port ){
	case 0xA8:
		return MSXMotherBoard::instance()->A8_Register;
		break;
	case 0xA9:
		Inputs::instance()->getKeys(); //reading the keyboard events into the matrix
		if (real_MSX_keyboard){
			KeyGhosting();
		}
		return MSXKeyMatrix[port_aa & 15];
		break;
	case 0xAA:
        return port_aa;
		break;
	case 0xAB:
        //TODO: test what a real MSX returns
        // normally after a the MSX is started 
        // you don't tough this register, but hey
        // we go for the perfect emulation :-)
        return port_ab;
		break;
  }
}

void MSXPPI::writeIO(byte port, byte value, Emutime &time)
{
  switch( port ){
	case 0xA8:
	  if ( value != MSXMotherBoard::instance()->A8_Register){
	    //TODO: do slotswitching if needed
	    MSXMotherBoard::instance()->set_A8_Register(value);
	  }
	  break;
	case 0xA9:
        //BOGUS read-only register
		break;
	case 0xAA:
        port_aa=value;
        //TODO use these bits
		//4    CASON  Cassette motor relay        (0=On, 1=Off)
		//5    CASW   Cassette audio out          (Pulse)
		//6    CAPS   CAPS-LOCK lamp              (0=On, 1=Off)
		//7    SOUND  Keyboard klick bit          (Pulse)

		break;
	case 0xAB:
	//Theoretically these registers can be used as input or output 
	//ports. However, in the MSX, PPI Port A and C are always used as output, 
	//and PPI Port B as input. The BIOS initializes it like that by writing 
	//82h to Port ABh on startup. Afterwards it makes no sense to change this 
	//setting, and thus we simply discard any write :-)
	//
	// see 8255.pdf for more detail

		break;
  }
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

void MSXPPI::KeyGhosting(void)
{

#define NR_KEYROWS 15

int changed_something;
int rowanded;

// This routine enables keyghosting as seen on a real MSX
//
// If on a real MSX in the keyboardmatrix the
// real buttons are pressed as in the left matrix
//           then the matrix to the
// 10111111  right will be read by   10110101
// 11110101  because of the simple   10110101
// 10111101  electrical connections  10110101
//           that are established  by
// the closed switches 

  changed_something=0;
  do {
    for (int i=0 ; i<NR_KEYROWS ;i++){
      for (int j=i+1 ; j<NR_KEYROWS+1 ; j++){
        if ( (MSXKeyMatrix[i]|MSXKeyMatrix[j])!=255 ){
          rowanded=MSXKeyMatrix[i]&MSXKeyMatrix[j];
          if  ( rowanded != MSXKeyMatrix[i]){
            MSXKeyMatrix[i]=rowanded;
            changed_something=1;
          }
        }
      }
    }
  } while (changed_something);

};


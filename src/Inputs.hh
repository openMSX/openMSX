// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include "MSXPPI.hh"
#include <SDL/SDL.h>

class Inputs 
{	// this class is a singleton class
	// usage: Inputs::instance()->method(args);
 
	static byte Keys[336][2];

	private:
		Inputs(); // private constructor -> can only construct self
		static Inputs *volatile oneInstance; 
		byte KeyMatrix[16];
		MSXPPI* PPI;
	public:
		~Inputs(); 
		static Inputs *instance();
		void setPPI(MSXPPI* PPIdevice);
		void getKeys(void);
};
#endif

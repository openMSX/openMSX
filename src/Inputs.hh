// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include "openmsx.hh"
#include <SDL/SDL.h>

#define NR_KEYROWS 11

class Inputs 
{
	public:
		~Inputs(); 
		static Inputs *instance();
		const byte* getKeys();
	
	private:
		Inputs(); // private constructor -> can only construct self
		static Inputs *oneInstance; 
		byte keyMatrix[NR_KEYROWS];
		static byte Keys[336][2];
};
#endif

// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include "openmsx.hh"
#include "EventDistributor.hh"

#define NR_KEYROWS 11

class Inputs : public EventListener 
{
	public:
		virtual ~Inputs(); 
		static Inputs *instance();
		const byte* getKeys();
		void signalEvent(SDL_Event &event);
	
	private:
		Inputs(); // private constructor -> can only construct self
		static Inputs *oneInstance; 
		byte keyMatrix[NR_KEYROWS];
		static byte Keys[336][2];
};
#endif

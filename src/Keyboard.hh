// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include "openmsx.hh"
#include "EventDistributor.hh"


class Keyboard : public EventListener 
{
	public:
		virtual ~Keyboard(); 
		static Keyboard *instance();
		const byte* getKeys();
		void signalEvent(SDL_Event &event);
		
		static const int NR_KEYROWS = 11;

	private:
		Keyboard(); // private constructor -> can only construct self
		static Keyboard *oneInstance; 
		byte keyMatrix[NR_KEYROWS];
		static byte Keys[336][2];
};
#endif

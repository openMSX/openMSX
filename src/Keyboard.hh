// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include "openmsx.hh"
#include "EventDistributor.hh"


class Keyboard : public EventListener 
{
	public:
		Keyboard(bool keyGhosting);
		virtual ~Keyboard(); 
		const byte* getKeys();
		void signalEvent(SDL_Event &event);
		
		static const int NR_KEYROWS = 11;

	private:
		void doKeyGhosting();

		byte keyMatrix[NR_KEYROWS];
		bool keyGhosting;
		bool lazyGhosting;
		static byte Keys[336][2];
};
#endif

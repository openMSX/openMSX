// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include "openmsx.hh"
#include "EventDistributor.hh"


class Keyboard : public EventListener 
{
	public:
		/**
		 * Constructs a new Keyboard object.
		 * @param keyGhosting turn keyGhosting on/off
		 */
		Keyboard(bool keyGhosting);

		/**
		 * Destructor
		 */
		virtual ~Keyboard(); 

		/**
		 * Returns a pointer to the current KeyBoard matrix
		 */
		const byte* getKeys();
		
		//EventListener
		void signalEvent(SDL_Event &event);
		
		static const int NR_KEYROWS = 11;

	private:
		void doKeyGhosting();

		byte keyMatrix[NR_KEYROWS];
		byte keyMatrix2[NR_KEYROWS];
		bool keyGhosting;
		bool keysChanged;
		static const byte Keys[336][2];
};
#endif

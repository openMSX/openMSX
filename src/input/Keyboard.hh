// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include "openmsx.hh"
#include "EventListener.hh"


namespace openmsx {

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
		virtual bool signalEvent(SDL_Event &event);

		static const int NR_KEYROWS = 12;

	private:
		void doKeyGhosting();
		void parseKeymapfile(const byte *, unsigned int);
		void loadKeymapfile(const std::string &);

		byte keyMatrix[16];
		byte keyMatrix2[16];
		bool keyGhosting;
		bool keysChanged;
		static byte Keys[336][2];
};

} // namespace openmsx
#endif

// $Id$

#ifndef __INPUTS_HH__
#define __INPUTS_HH__

#include <string>
#include "openmsx.hh"
#include "EventListener.hh"

using std::string;


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
		virtual bool signalEvent(SDL_Event &event) throw();

		static const int NR_KEYROWS = 12;

	private:
		void doKeyGhosting();
		void parseKeymapfile(const byte* buf, unsigned size);
		void loadKeymapfile(const string& filename);

		byte keyMatrix[16];
		byte keyMatrix2[16];
		bool keyGhosting;
		bool keysChanged;
		static byte Keys[336][2];
};

} // namespace openmsx
#endif

// $Id$

#ifndef __MSXKANJI_HH__
#define __MSXKANJI_HH__

#include "MSXIODevice.hh"
#include "Rom.hh"


class MSXKanji : public MSXIODevice
{
	public:
		/**
		 * Constructor
		 */
		MSXKanji(Device *config, const EmuTime &time);

		/**
		 * Destructor
		 */
		virtual ~MSXKanji();
		
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
		virtual void reset(const EmuTime &time);

	private:
		Rom rom;
		int adr1, adr2;
};

#endif //__MSXKANJI_HH__


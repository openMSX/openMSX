// $Id$

#ifndef __MSXFMPAC_HH__
#define __MSXFMPAC_HH__

#include "MSXMusic.hh"
#include "SRAM.hh"


class MSXFmPac : public MSXMusic
{
	public:
		MSXFmPac(Device *config, const EmuTime &time);
		virtual ~MSXFmPac(); 
		
		virtual void reset(const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
		virtual byte readMem(word address, const EmuTime &time);
		virtual void writeMem(word address, byte value, const EmuTime &time);
		virtual const byte* getReadCacheLine(word start) const;

	private:
		void checkSramEnable();
		
		bool sramEnabled;
		byte enable;
		byte bank;
		byte r5ffe, r5fff;
		SRAM sram;
};

#endif

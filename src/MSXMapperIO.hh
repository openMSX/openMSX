// $Id$

#ifndef __MSXMAPPERIO_HH__
#define __MSXMAPPERIO_HH__

#include "openmsx.hh"
#include "MSXIODevice.hh"
#include <list>

// forward declarations
class EmuTime;


class MapperMask
{
	public:
		virtual byte calcMask(std::list<int> &mapperSizes) = 0;
};

class MSXMapperIO : public MSXIODevice
{
	public:
		~MSXMapperIO();
		static MSXMapperIO* instance();

		void reset(const EmuTime &time);
		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
		
		/**
		 * Every MSXMemoryMapper must (un)register its size.
		 * This is used to influence the result returned in readIO().
		 */
		void registerMapper(int blocks);
		void unregisterMapper(int blocks);
		
		/**
		 * Returns the actual selected page for the given bank.
		 */
		byte getSelectedPage(int bank);

	private:
		MSXMapperIO(MSXConfig::Device *config, const EmuTime &time);
		
		static MSXMapperIO* oneInstance;
		MapperMask* mapperMask;
		std::list<int> mapperSizes;
		byte mask;
		byte page[4];
};

#endif

// $Id$

#ifndef __MSXMAPPERIO_HH__
#define __MSXMAPPERIO_HH__

#include "openmsx.hh"
#include "MSXIODevice.hh"
#include <list>

using std::list;

class MapperMask
{
	public:
		virtual byte calcMask(list<int> &mapperSizes) = 0;
};

class MSXMapperIO : public MSXIODevice
{
	public:
		virtual ~MSXMapperIO();
		static MSXMapperIO* instance();

		virtual void reset(const EmuTime &time);
		virtual byte readIO(byte port, const EmuTime &time);
		virtual void writeIO(byte port, byte value, const EmuTime &time);
		
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
		MSXMapperIO(Device *config, const EmuTime &time);
		MSXMapperIO();
		
		MapperMask* mapperMask;
		list<int> mapperSizes;
		byte mask;
		byte page[4];
};

#endif

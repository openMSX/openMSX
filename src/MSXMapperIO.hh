// $Id$

#ifndef __MSXMAPPERIO_HH__
#define __MSXMAPPERIO_HH__

#include "openmsx.hh"
#include "MSXIODevice.hh"

// forward declarations
class EmuTime;


class MSXMapperIO : public MSXIODevice
{
	public:
		~MSXMapperIO();
		static MSXMapperIO* instance();

		void reset(const EmuTime &time);
		byte readIO(byte port, const EmuTime &time);
		void writeIO(byte port, byte value, const EmuTime &time);
		
		/**
		 * Convert a previously written byte (out &hff,1) to a byte 
		 * returned from an read operation (inp(&hff)).
		 */
		virtual byte convert(byte value) = 0;

		/**
		 * Every MSXMemoryMapper must registers its size.
		 * This is used to influence the result returned in convert().
		 */
		virtual void registerMapper(int blocks) = 0;
		
		//TODO make private
		static int pageAddr[4];
		
	protected:
		MSXMapperIO(MSXConfig::Device *config, const EmuTime &time);
	
	private:
		static MSXMapperIO* oneInstance;
};

#endif

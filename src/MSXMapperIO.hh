// $Id$

#ifndef __MSXMAPPERIO_HH__
#define __MSXMAPPERIO_HH__

#include "openmsx.hh"

class MSXMapperIO
{
	public:
		/**
		 * Convert a previously written byte (out &hff,1) to a byte 
		 * returned from an read operation (inp(&hff)).
		 */
		virtual byte convert(byte value) = 0;

		/**
		 * Every mapper registers itself with MSXMapperIO (not this class)
		 * MSXMapperIO than calls this method. This can be used to influence
		 * the result returned in convert().
		 */
		virtual void registerMapper(int blocks) = 0;
};

#endif

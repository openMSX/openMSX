// $Id$

#ifndef __FILEBASE_HH__
#define __FILEBASE_HH__

#include "openmsx.hh"


class FileBase
{
	public:
		virtual ~FileBase() {}
		virtual void read (byte* buffer, int num) = 0;
		virtual void write(const byte* buffer, int num) = 0;
		virtual int size() = 0;
		virtual void seek(int pos) = 0;
		virtual int pos() = 0;
};

#endif

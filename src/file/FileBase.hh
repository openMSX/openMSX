// $Id$

#ifndef __FILEBASE_HH__
#define __FILEBASE_HH__

#include "openmsx.hh"


class FileBase
{
	public:
		FileBase() : mmem(NULL) {}
		virtual ~FileBase();
		virtual void read (byte* buffer, int num) = 0;
		virtual void write(const byte* buffer, int num) = 0;
		virtual byte* mmap(bool write = false);
		virtual void munmap();
		virtual int size() = 0;
		virtual void seek(int pos) = 0;
		virtual int pos() = 0;

	protected:
		byte* mmem;

	private:
		bool mmapWrite;
		int mmapSize;
};

#endif

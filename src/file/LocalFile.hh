// $Id$

#ifndef __LOCALFILE_HH__
#define __LOCALFILE_HH__

#include "FileBase.hh"
#include <string>
#include <stdio.h>


class LocalFile : public FileBase
{
	public:
		LocalFile(const std::string &filename, int options);
		virtual ~LocalFile();
		virtual void read (byte* buffer, int num);
		virtual void write(const byte* buffer, int num);
		virtual byte* mmap(bool writeBack);
		virtual void munmap();
		virtual int getSize();
		virtual void seek(int pos);
		virtual int getPos();
		virtual const std::string getURL() const;
		virtual const std::string getLocalName() const;

	private:
		std::string filename;
		FILE* file;
};

#endif


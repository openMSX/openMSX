// $Id$

#ifndef __LOCALFILE_HH__
#define __LOCALFILE_HH__

#include <string>
#include <stdio.h>
#include "FileBase.hh"
#include "File.hh"


class LocalFile : public FileBase
{
	public:
		LocalFile(const std::string &filename, OpenMode mode);
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
		virtual bool isReadOnly() const;

	private:
		std::string filename;
		FILE* file;
		bool readOnly;
};

#endif


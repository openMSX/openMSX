// $Id$

#ifndef __LOCALFILE_HH__
#define __LOCALFILE_HH__

#include <string>
#include <stdio.h>
#include "FileBase.hh"
#include "File.hh"


namespace openmsx {

class LocalFile : public FileBase
{
	public:
		LocalFile(const string &filename, OpenMode mode);
		virtual ~LocalFile();
		virtual void read (byte* buffer, int num);
		virtual void write(const byte* buffer, int num);
		virtual byte* mmap(bool writeBack);
		virtual void munmap();
		virtual int getSize();
		virtual void seek(int pos);
		virtual int getPos();
		virtual const string getURL() const;
		virtual const string getLocalName();
		virtual bool isReadOnly() const;

	private:
		string filename;
		FILE* file;
		bool readOnly;
};

} // namespace openmsx

#endif


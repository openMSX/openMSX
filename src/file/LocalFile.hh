// $Id$

#ifndef __LOCALFILE_HH__
#define __LOCALFILE_HH__

#include "FileBase.hh"
#include "File.hh"
#include "config.h"
#include <cstdio>


namespace openmsx {

class LocalFile : public FileBase
{
	public:
		LocalFile(const string &filename, OpenMode mode);
		virtual ~LocalFile();
		virtual void read (byte* buffer, unsigned num);
		virtual void write(const byte* buffer, unsigned num);
#ifdef	HAVE_MMAP
		virtual byte* mmap(bool writeBack);
		virtual void munmap();
#endif
		virtual unsigned getSize();
		virtual void seek(unsigned pos);
		virtual unsigned getPos();
#ifdef HAVE_FTRUNCATE
		virtual void truncate(unsigned size);
#endif
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


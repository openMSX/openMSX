// $Id$

#ifndef LOCALFILE_HH
#define LOCALFILE_HH

#include <cstdio>
#include "File.hh"
#include "FileBase.hh"
#include "probed_defs.hh"

namespace openmsx {

class LocalFile : public FileBase
{
public:
	LocalFile(const std::string& filename, File::OpenMode mode);
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
	virtual void flush();
	virtual const std::string getURL() const;
	virtual const std::string getLocalName();
	virtual bool isReadOnly() const;
	virtual time_t getModificationDate();

private:
	std::string filename;
	FILE* file;
	bool readOnly;
};

} // namespace openmsx

#endif

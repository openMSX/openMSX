// $Id$

#ifndef __FILE_HH__
#define __FILE_HH__

#include "openmsx.hh"

#include <string>

class File
{
public:
	typedef enum
	{
		read   = 1, // want to be able to read
		write  = 2, // want to be able to write
		prefer = 4  // want to be able to write, but silently ignore if fails to write
	} mode_t;

	static File* createFile(const std::string& uri, byte mode=read|write|prefer);

	// does the file exist?
	virtual bool head()=0;

	// get the file in memory
	virtual void fetch()=0;

	// write the file back
	virtual void put()=0;

	// get memory buffer, do as you please with it, just
	// don't delete it
	virtual byte* getData(size_t& len)=0;

	// set memory buffer, this assumes data != buffer
	virtual void  setData(byte* data, size_t len)=0;

protected:
	File(byte mode);
	File();
	virtual ~File();

	byte* buffer;

	byte mode;
private:
	File(const File& foo);
	File& operator=(const File& foo);
};

#endif
